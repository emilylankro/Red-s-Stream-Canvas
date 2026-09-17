#include "CaptureSource.h"
#include "GraphicsDevice.h"

#include <windows.graphics.directx.direct3d11.interop.h>
#include <algorithm>

using Microsoft::WRL::ComPtr;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;

CaptureSource::CaptureSource(
    std::shared_ptr<GraphicsDevice> graphics,
    GraphicsCaptureItem item)
    : m_graphics(std::move(graphics)), m_item(std::move(item)) {

    const auto size = m_item.Size();
    m_width = static_cast<uint32_t>(std::max(size.Width, 1));
    m_height = static_cast<uint32_t>(std::max(size.Height, 1));

    m_framePool = Direct3D11CaptureFramePool::CreateFreeThreaded(
        m_graphics->WinRtDevice(),
        DirectXPixelFormat::B8G8R8A8UIntNormalized,
        2,
        size);

    m_session = m_framePool.CreateCaptureSession(m_item);
}

CaptureSource::~CaptureSource() {
    Stop();
}

void CaptureSource::Start() {
    if (m_started.exchange(true) || m_stopped.load() || m_closed.load() || !m_session) {
        return;
    }

    const auto weak = weak_from_this();
    m_frameToken = m_framePool.FrameArrived([weak](auto const& sender, auto const& args) {
        if (auto self = weak.lock()) {
            self->OnFrameArrived(sender, args);
        }
    });
    m_closedToken = m_item.Closed([weak](auto&&, auto&&) {
        if (auto self = weak.lock()) {
            self->m_closed.store(true);
        }
    });

    m_session.StartCapture();
}

void CaptureSource::Stop() {
    if (m_stopped.exchange(true)) {
        return;
    }
    m_closed.store(true);

    try {
        if (m_started.load() && m_framePool) {
            m_framePool.FrameArrived(m_frameToken);
        }
        if (m_started.load() && m_item) {
            m_item.Closed(m_closedToken);
        }
    } catch (...) {
        // The selected app/window may already have been torn down.
    }

    if (m_session) {
        m_session.Close();
        m_session = nullptr;
    }
    if (m_framePool) {
        m_framePool.Close();
        m_framePool = nullptr;
    }

    std::scoped_lock lock(m_mutex);
    m_copyTexture.Reset();
}

void CaptureSource::SetMaxFps(uint32_t fps) noexcept {
    m_maxFps.store(std::clamp(fps, 5u, 60u));
}

void CaptureSource::ResetCrop() noexcept {
    m_transform.cropLeft = 0.0f;
    m_transform.cropTop = 0.0f;
    m_transform.cropRight = 0.0f;
    m_transform.cropBottom = 0.0f;
}

std::wstring CaptureSource::Name() const {
    if (!m_item) {
        return L"Source";
    }
    const auto name = m_item.DisplayName();
    return std::wstring(name.c_str(), name.size());
}

CaptureFrameSnapshot CaptureSource::Snapshot() const {
    std::scoped_lock lock(m_mutex);
    CaptureFrameSnapshot snapshot;
    snapshot.texture = m_copyTexture;
    snapshot.width = m_width;
    snapshot.height = m_height;
    snapshot.generation = m_generation;
    return snapshot;
}

void CaptureSource::EnsureCopyTexture(ID3D11Texture2D* source, uint32_t width, uint32_t height) {
    if (!source) {
        return;
    }

    D3D11_TEXTURE2D_DESC sourceDesc{};
    source->GetDesc(&sourceDesc);

    bool recreate = false;
    {
        std::scoped_lock lock(m_mutex);
        recreate = !m_copyTexture || m_width != width || m_height != height;
    }

    if (!recreate) {
        return;
    }

    D3D11_TEXTURE2D_DESC desc = sourceDesc;
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    ComPtr<ID3D11Texture2D> newTexture;
    winrt::check_hresult(m_graphics->D3DDevice()->CreateTexture2D(&desc, nullptr, &newTexture));

    std::scoped_lock lock(m_mutex);
    m_copyTexture = std::move(newTexture);
    m_width = width;
    m_height = height;
    ++m_generation;
}

void CaptureSource::OnFrameArrived(
    Direct3D11CaptureFramePool const& sender,
    winrt::Windows::Foundation::IInspectable const&) {

    if (m_closed.load()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto fps = std::max(m_maxFps.load(), 1u);
    const auto minInterval = std::chrono::microseconds(1'000'000 / fps);
    if (m_lastAcceptedFrame.time_since_epoch().count() != 0 && now - m_lastAcceptedFrame < minInterval) {
        auto skipped = sender.TryGetNextFrame();
        return;
    }

    auto frame = sender.TryGetNextFrame();
    if (!frame) {
        return;
    }

    const auto size = frame.ContentSize();
    if (size.Width <= 0 || size.Height <= 0) {
        return;
    }

    auto access = frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    ComPtr<ID3D11Texture2D> sourceTexture;
    winrt::check_hresult(access->GetInterface(
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(sourceTexture.GetAddressOf())));

    const auto width = static_cast<uint32_t>(size.Width);
    const auto height = static_cast<uint32_t>(size.Height);
    bool sizeChanged = false;
    {
        std::scoped_lock lock(m_mutex);
        sizeChanged = width != m_width || height != m_height;
    }

    EnsureCopyTexture(sourceTexture.Get(), width, height);

    ComPtr<ID3D11Texture2D> destination;
    {
        std::scoped_lock lock(m_mutex);
        destination = m_copyTexture;
    }

    if (destination) {
        m_graphics->D3DContext()->CopyResource(destination.Get(), sourceTexture.Get());
        m_lastAcceptedFrame = now;
    }

    if (sizeChanged) {
        frame.Close();
        sender.Recreate(
            m_graphics->WinRtDevice(),
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            size);
    }
}
