#include "CanvasRenderer.h"
#include "CaptureSource.h"
#include "GraphicsDevice.h"

#include <algorithm>
#include <cmath>
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

D2D1_RECT_F CanvasRenderer::FitRect(
    float cellX,
    float cellY,
    float cellW,
    float cellH,
    float sourceW,
    float sourceH) const {

    if (sourceW <= 0 || sourceH <= 0) {
        return D2D1::RectF(cellX, cellY, cellX + cellW, cellY + cellH);
    }

    const float scale = std::min(cellW / sourceW, cellH / sourceH);
    const float w = sourceW * scale;
    const float h = sourceH * scale;
    const float x = cellX + (cellW - w) * 0.5f;
    const float y = cellY + (cellH - h) * 0.5f;
    return D2D1::RectF(x, y, x + w, y + h);
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

    // Conservative auto mode: only use 60 FPS when composition is very cheap.
    if (m_targetFps == 30 && sourceCount <= 4 && avg < 4.5) {
        m_targetFps = 60;
    } else if (m_targetFps == 60 && avg > 10.0) {
        m_targetFps = 30;
    }

    m_perfSampleCount = 0;
    m_perfAccumulatedMs = 0.0;
}

void CanvasRenderer::Render(const std::vector<std::shared_ptr<CaptureSource>>& sources) {
    if (!m_swapChain || !m_d2dContext) {
        return;
    }

    const auto begin = std::chrono::steady_clock::now();

    m_d2dContext->BeginDraw();
    m_d2dContext->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    const size_t n = sources.size();
    if (n > 0) {
        const uint32_t columns = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(n))));
        const uint32_t rows = static_cast<uint32_t>((n + columns - 1) / columns);
        const float cellW = static_cast<float>(m_width) / static_cast<float>(columns);
        const float cellH = static_cast<float>(m_height) / static_cast<float>(rows);

        for (size_t i = 0; i < n; ++i) {
            const auto& source = sources[i];
            if (!source || source->IsClosed()) {
                continue;
            }

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

            const uint32_t col = static_cast<uint32_t>(i % columns);
            const uint32_t row = static_cast<uint32_t>(i / columns);
            const float x = col * cellW;
            const float y = row * cellH;
            const auto dest = FitRect(
                x,
                y,
                cellW,
                cellH,
                static_cast<float>(snapshot.width),
                static_cast<float>(snapshot.height));

            m_d2dContext->DrawBitmap(
                cache.bitmap.Get(),
                dest,
                1.0f,
                D2D1_INTERPOLATION_MODE_LINEAR,
                nullptr);
        }
    }

    const HRESULT endHr = m_d2dContext->EndDraw();
    if (endHr == D2DERR_RECREATE_TARGET) {
        CreateTargetBitmap();
        return;
    }
    winrt::check_hresult(endHr);

    // Do not force vsync here. Our lightweight timer controls the output cadence.
    const HRESULT presentHr = m_swapChain->Present(0, 0);
    if (presentHr == DXGI_STATUS_OCCLUDED) {
        return;
    }
    winrt::check_hresult(presentHr);

    const auto end = std::chrono::steady_clock::now();
    const double renderMs = std::chrono::duration<double, std::milli>(end - begin).count();
    UpdateAdaptivePerformance(renderMs, sources.size());
}
