#include "CanvasRenderer.h"
#include "CaptureSource.h"
#include "GraphicsDevice.h"

#include <algorithm>
#include <cstring>
#include <d2d1_1helper.h>
#include <d3dcompiler.h>

using Microsoft::WRL::ComPtr;

namespace {
constexpr char VertexShaderSource[] = R"(
cbuffer TransformBuffer : register(b0)
{
    float4 destination;
    float4 sourceUv;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    float2 corner = float2(vertexId & 1, (vertexId >> 1) & 1);
    float2 canvas = destination.xy + corner * destination.zw;

    VSOutput output;
    output.position = float4(canvas.x * 2.0f - 1.0f, 1.0f - canvas.y * 2.0f, 0.0f, 1.0f);
    output.uv = lerp(sourceUv.xy, sourceUv.zw, corner);
    return output;
}
)";

constexpr char PixelShaderSource[] = R"(
Texture2D sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 color = sourceTexture.Sample(sourceSampler, uv);
    return float4(color.rgb, 1.0f);
}
)";

ComPtr<ID3DBlob> CompileShader(const char* source, const char* target) {
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompile(
        source,
        std::strlen(source),
        nullptr,
        nullptr,
        nullptr,
        "main",
        target,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        &shader,
        &errors);

    if (FAILED(hr) && errors) {
        OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
    }
    winrt::check_hresult(hr);
    return shader;
}
}

CanvasRenderer::CanvasRenderer(std::shared_ptr<GraphicsDevice> graphics, HWND hwnd)
    : m_graphics(std::move(graphics)), m_hwnd(hwnd) {

    ComPtr<ID2D1DeviceContext> baseContext;
    winrt::check_hresult(m_graphics->D2DDevice()->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        &baseContext));
    winrt::check_hresult(baseContext.As(&m_d2dContext));

    CreateSwapChain();
    CreateBackBufferResources();
    CreateTexturePipeline();
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

void CanvasRenderer::CreateBackBufferResources() {
    m_d2dContext->SetTarget(nullptr);
    m_targetBitmap.Reset();
    m_renderTargetView.Reset();

    ComPtr<ID3D11Texture2D> backBuffer;
    winrt::check_hresult(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
    winrt::check_hresult(m_graphics->D3DDevice()->CreateRenderTargetView(
        backBuffer.Get(),
        nullptr,
        &m_renderTargetView));

    ComPtr<IDXGISurface> surface;
    winrt::check_hresult(backBuffer.As(&surface));

    const auto props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));

    winrt::check_hresult(m_d2dContext->CreateBitmapFromDxgiSurface(
        surface.Get(),
        &props,
        &m_targetBitmap));
    m_d2dContext->SetTarget(m_targetBitmap.Get());
}

void CanvasRenderer::CreateTexturePipeline() {
    auto vertexBlob = CompileShader(VertexShaderSource, "vs_4_0");
    auto pixelBlob = CompileShader(PixelShaderSource, "ps_4_0");

    winrt::check_hresult(m_graphics->D3DDevice()->CreateVertexShader(
        vertexBlob->GetBufferPointer(),
        vertexBlob->GetBufferSize(),
        nullptr,
        &m_vertexShader));

    winrt::check_hresult(m_graphics->D3DDevice()->CreatePixelShader(
        pixelBlob->GetBufferPointer(),
        pixelBlob->GetBufferSize(),
        nullptr,
        &m_pixelShader));

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = sizeof(TransformConstants);
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    winrt::check_hresult(m_graphics->D3DDevice()->CreateBuffer(
        &bufferDesc,
        nullptr,
        &m_transformBuffer));

    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    winrt::check_hresult(m_graphics->D3DDevice()->CreateSamplerState(
        &samplerDesc,
        &m_sampler));
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
    m_renderTargetView.Reset();

    winrt::check_hresult(m_swapChain->ResizeBuffers(
        0,
        width,
        height,
        DXGI_FORMAT_UNKNOWN,
        0));

    CreateBackBufferResources();
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

bool CanvasRenderer::DrawSource(CaptureSource const& source) {
    auto snapshot = source.Snapshot();
    if (!snapshot.texture || snapshot.width == 0 || snapshot.height == 0) {
        return false;
    }

    auto& cache = m_textureCache[&source];
    if (!cache.view || cache.generation != snapshot.generation) {
        ComPtr<ID3D11ShaderResourceView> view;
        if (FAILED(m_graphics->D3DDevice()->CreateShaderResourceView(
            snapshot.texture.Get(),
            nullptr,
            &view))) {
            return false;
        }
        cache.view = std::move(view);
        cache.generation = snapshot.generation;
    }

    const auto& transform = source.Transform();
    TransformConstants constants{};
    constants.destination[0] = transform.x;
    constants.destination[1] = transform.y;
    constants.destination[2] = transform.width;
    constants.destination[3] = transform.height;
    constants.sourceUv[0] = std::clamp(transform.cropLeft, 0.0f, 0.95f);
    constants.sourceUv[1] = std::clamp(transform.cropTop, 0.0f, 0.95f);
    constants.sourceUv[2] = 1.0f - std::clamp(transform.cropRight, 0.0f, 0.95f);
    constants.sourceUv[3] = 1.0f - std::clamp(transform.cropBottom, 0.0f, 0.95f);

    if (constants.sourceUv[2] <= constants.sourceUv[0] ||
        constants.sourceUv[3] <= constants.sourceUv[1]) {
        return false;
    }

    auto* context = m_graphics->D3DContext();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(
        m_transformBuffer.Get(),
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &mapped))) {
        return false;
    }
    std::memcpy(mapped.pData, &constants, sizeof(constants));
    context->Unmap(m_transformBuffer.Get(), 0);

    ID3D11Buffer* constantBuffer = m_transformBuffer.Get();
    context->VSSetConstantBuffers(0, 1, &constantBuffer);

    ID3D11ShaderResourceView* sourceView = cache.view.Get();
    context->PSSetShaderResources(0, 1, &sourceView);
    context->Draw(4, 0);
    return true;
}

void CanvasRenderer::Render(
    const std::vector<std::shared_ptr<CaptureSource>>& sources,
    const CaptureSource* selected,
    bool editMode,
    bool cropMode) {

    if (!m_swapChain || !m_renderTargetView) {
        return;
    }

    const auto begin = std::chrono::steady_clock::now();
    auto* context = m_graphics->D3DContext();

    ID3D11RenderTargetView* renderTarget = m_renderTargetView.Get();
    context->OMSetRenderTargets(1, &renderTarget, nullptr);

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    ID3D11SamplerState* sampler = m_sampler.Get();
    context->PSSetSamplers(0, 1, &sampler);

    size_t visibleCount = 0;
    for (const auto& source : sources) {
        if (!source || source->IsClosed() || !source->Transform().visible) {
            continue;
        }
        ++visibleCount;
        DrawSource(*source);
    }

    ID3D11ShaderResourceView* nullView = nullptr;
    context->PSSetShaderResources(0, 1, &nullView);

    if (editMode && selected && selected->Transform().visible && !selected->IsClosed()) {
        // D3D and Direct2D share the same back buffer. Flush only while editing;
        // the clean output path stays entirely in D3D for minimum overhead.
        context->Flush();
        m_d2dContext->BeginDraw();
        DrawSelection(DestinationRect(selected->Transform()), cropMode);
        const HRESULT endHr = m_d2dContext->EndDraw();
        if (endHr == D2DERR_RECREATE_TARGET) {
            CreateBackBufferResources();
            return;
        }
        winrt::check_hresult(endHr);
    }

    const HRESULT presentHr = m_swapChain->Present(0, 0);
    if (presentHr == DXGI_STATUS_OCCLUDED) {
        return;
    }
    winrt::check_hresult(presentHr);

    const auto end = std::chrono::steady_clock::now();
    const double renderMs = std::chrono::duration<double, std::milli>(end - begin).count();
    UpdateAdaptivePerformance(renderMs, visibleCount);
}
