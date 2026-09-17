#include "CanvasRenderer.h"
#include "CaptureSource.h"
#include "GraphicsDevice.h"

#include <algorithm>
#include <d2d1_1helper.h>

using Microsoft::WRL::ComPtr;

CanvasRenderer::CanvasRenderer(std::shared_ptr<GraphicsDevice> graphics, HWND hwnd)
    : m_graphics(std::move(graphics)), m_hwnd(hwnd) {

    ComPtr<ID2D1DeviceContext> baseContext;
    winrt::check_hresult(m_graphics->D2DDevice()->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        &baseContext));
    winrt::check_hresult(baseContext.As(&m_d2dContext));

    CreateSwapChain();
    CreateTargetBitmap();
    CreateEditorResources();
}

void CanvasRenderer::CreateSwapChain() {
    ComPtr<IDXGIAdapter> adapter;
    winrt::check_hresult(m_graphics->DxgiDevice()->GetAdapter(&adapter));

    ComPtr<IDXGIFactory2> factory;
    winrt::check_hresult(adapter->GetParent(IID_PPV_ARGS(&factory)));

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    winrt::check_hresult(factory->CreateSwapChainForHwnd(
        m_graphics->D3DDevice(),
        m_hwnd,
        &desc,
        nullptr,
        nullptr,
        &m_swapChain));

    factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);
}

void CanvasRenderer::CreateTargetBitmap() {
    m_targetBitmap.Reset();
    m_d2dContext->SetTarget(nullptr);

    ComPtr<IDXGISurface> surface;
    winrt::check_hresult(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&surface)));

    const auto props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));

    winrt::check_hresult(m_d2dContext->CreateBitmapFromDxgiSurface(
        surface.Get(),
        &props,
        &m_targetBitmap));

    m_d2dContext->SetTarget(m_targetBitmap.Get());
}

void CanvasRenderer::CreateEditorResources() {
    winrt::check_hresult(m_d2dContext->CreateSolidColorBrush(
        D2D1::ColorF(0.20f, 0.70f, 1.00f, 1.0f),
        &m_selectionBrush));
    winrt::check_hresult(m_d2dContext->CreateSolidColorBrush(
        D2D1::ColorF(1.00f, 0.62f, 0.12f, 1.0f),
        &m_cropBrush));
    winrt::check_hresult(m_d2dContext->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White),
        &m_handleFillBrush));
}

void CanvasRenderer::Resize(uint32_t width, uint32_t height) {
    if (!m_swapChain || width == 0 || height == 0 || (width == m_width && height == m_height)) {
        return;
    }

    m_width = width;
    m_height = height;
    m_d2dContext->SetTarget(nullptr);
    m_targetBitmap.Reset();
    m_bitmapCache.clear();

    winrt::check_hresult(m_swapChain->ResizeBuffers(
        0,
        width,
        height,
        DXGI_FORMAT_UNKNOWN,
        0));

    CreateTargetBitmap();
}

D2D1_RECT_F CanvasRenderer::DestinationRect(const CanvasTransform& transform) const {
    return D2D1::RectF(
        transform.x * static_cast<float>(m_width),
        transform.y * static_cast<float>(m_height),
        (transform.x + transform.width) * static_cast<float>(m_width),
        (transform.y + transform.height) * static_cast<float>(m_height));
}

void CanvasRenderer::SetFixedFps(uint32_t fps) noexcept {
    m_autoPerformance = false;
    m_targetFps = std::clamp(fps, 15u, 60u);
}

void CanvasRenderer::UpdateAdaptivePerformance(double renderMs, size_t sourceCount) {
    if (!m_autoPerformance) {
        return;
    }

    m_perfAccumulatedMs += renderMs;
    ++m_perfSampleCount;
    if (m_perfSampleCount < 120) {
        return;
    }

    const double avg = m_perfAccumulatedMs / static_cast<double>(m_perfSampleCount);
    if (m_targetFps == 30 && sourceCount <= 4 && avg < 4.5) {
        m_targetFps = 60;
    } else if (m_targetFps == 60 && avg > 10.0) {
        m_targetFps = 30;
    }

    m_perfSampleCount = 0;
    m_perfAccumulatedMs = 0.0;
}

void CanvasRenderer::DrawSelection(const D2D1_RECT_F& rect, bool cropMode) {
    ID2D1SolidColorBrush* outline = cropMode ? m_cropBrush.Get() : m_selectionBrush.Get();
    m_d2dContext->DrawRectangle(rect, outline, 2.0f);

    constexpr float handle = 8.0f;
    const float half = handle * 0.5f;
    const float cx = (rect.left + rect.right) * 0.5f;
    const float cy = (rect.top + rect.bottom) * 0.5f;

    const D2D1_POINT_2F points[] = {
        D2D1::Point2F(rect.left, rect.top),
        D2D1::Point2F(cx, rect.top),
        D2D1::Point2F(rect.right, rect.top),
        D2D1::Point2F(rect.right, cy),
        D2D1::Point2F(rect.right, rect.bottom),
        D2D1::Point2F(cx, rect.bottom),
        D2D1::Point2F(rect.left, rect.bottom),
        D2D1::Point2F(rect.left, cy),
    };

    for (const auto& point : points) {
        const auto box = D2D1::RectF(point.x - half, point.y - half, point.x + half, point.y + half);
        m_d2dContext->FillRectangle(box, m_handleFillBrush.Get());
        m_d2dContext->DrawRectangle(box, outline, 1.0f);
    }
}

void CanvasRenderer::Render(
    const std::vector<std::shared_ptr<CaptureSource>>& sources,
    const CaptureSource* selected,
    bool editMode,
    bool cropMode) {

    if (!m_swapChain || !m_d2dContext) {
        return;
    }

    const auto begin = std::chrono::steady_clock::now();

    m_d2dContext->BeginDraw();
    m_d2dContext->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    size_t visibleCount = 0;
    for (const auto& source : sources) {
        if (!source || source->IsClosed() || !source->Transform().visible) {
            continue;
        }
        ++visibleCount;

        auto snapshot = source->Snapshot();
        if (!snapshot.texture || snapshot.width == 0 || snapshot.height == 0) {
            continue;
        }

        auto& cache = m_bitmapCache[source.get()];
        if (!cache.bitmap || cache.generation != snapshot.generation) {
            ComPtr<IDXGISurface> surface;
            if (FAILED(snapshot.texture.As(&surface))) {
                continue;
            }

            const auto props = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_NONE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));

            ComPtr<ID2D1Bitmap1> bitmap;
            if (FAILED(m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), &props, &bitmap))) {
                continue;
            }

            cache.bitmap = std::move(bitmap);
            cache.generation = snapshot.generation;
        }

        const auto& transform = source->Transform();
        const auto dest = DestinationRect(transform);

        const float sourceW = static_cast<float>(snapshot.width);
        const float sourceH = static_cast<float>(snapshot.height);
        const D2D1_RECT_F src = D2D1::RectF(
            std::clamp(transform.cropLeft, 0.0f, 0.95f) * sourceW,
            std::clamp(transform.cropTop, 0.0f, 0.95f) * sourceH,
            (1.0f - std::clamp(transform.cropRight, 0.0f, 0.95f)) * sourceW,
            (1.0f - std::clamp(transform.cropBottom, 0.0f, 0.95f)) * sourceH);

        if (src.right > src.left && src.bottom > src.top) {
            m_d2dContext->DrawBitmap(
                cache.bitmap.Get(),
                dest,
                1.0f,
                D2D1_INTERPOLATION_MODE_LINEAR,
                src);
        }
    }

    if (editMode && selected && selected->Transform().visible && !selected->IsClosed()) {
        DrawSelection(DestinationRect(selected->Transform()), cropMode);
    }

    const HRESULT endHr = m_d2dContext->EndDraw();
    if (endHr == D2DERR_RECREATE_TARGET) {
        CreateTargetBitmap();
        return;
    }
    winrt::check_hresult(endHr);

    const HRESULT presentHr = m_swapChain->Present(0, 0);
    if (presentHr == DXGI_STATUS_OCCLUDED) {
        return;
    }
    winrt::check_hresult(presentHr);

    const auto end = std::chrono::steady_clock::now();
    const double renderMs = std::chrono::duration<double, std::milli>(end - begin).count();
    UpdateAdaptivePerformance(renderMs, visibleCount);
}
