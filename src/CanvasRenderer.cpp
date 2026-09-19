#include "CanvasRenderer.h"
#include "CaptureSource.h"
#include "GraphicsDevice.h"

#include <algorithm>
#include <cstring>
#include <d3dcompiler.h>
#include <string>

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
    output.position = float4(
        canvas.x * 2.0f - 1.0f,
        1.0f - canvas.y * 2.0f,
        0.0f,
        1.0f);
    output.uv = lerp(sourceUv.xy, sourceUv.zw, corner);
    return output;
}
)";

constexpr char TexturePixelShaderSource[] = R"(
Texture2D sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 color = sourceTexture.Sample(sourceSampler, uv);
    return float4(color.rgb, 1.0f);
}
)";

constexpr char ColorPixelShaderSource[] = R"(
cbuffer ColorBuffer : register(b0)
{
    float4 solidColor;
};

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    return solidColor;
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
    CreateSwapChain();
    CreateBackBufferResources();
    CreatePipeline();
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
    m_renderTargetView.Reset();

    ComPtr<ID3D11Texture2D> backBuffer;
    winrt::check_hresult(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
    winrt::check_hresult(m_graphics->D3DDevice()->CreateRenderTargetView(
        backBuffer.Get(),
        nullptr,
        &m_renderTargetView));
}

void CanvasRenderer::CreatePipeline() {
    auto vertexBlob = CompileShader(VertexShaderSource, "vs_4_0");
    auto texturePixelBlob = CompileShader(TexturePixelShaderSource, "ps_4_0");
    auto colorPixelBlob = CompileShader(ColorPixelShaderSource, "ps_4_0");

    winrt::check_hresult(m_graphics->D3DDevice()->CreateVertexShader(
        vertexBlob->GetBufferPointer(),
        vertexBlob->GetBufferSize(),
        nullptr,
        &m_vertexShader));

    winrt::check_hresult(m_graphics->D3DDevice()->CreatePixelShader(
        texturePixelBlob->GetBufferPointer(),
        texturePixelBlob->GetBufferSize(),
        nullptr,
        &m_texturePixelShader));

    winrt::check_hresult(m_graphics->D3DDevice()->CreatePixelShader(
        colorPixelBlob->GetBufferPointer(),
        colorPixelBlob->GetBufferSize(),
        nullptr,
        &m_colorPixelShader));

    D3D11_BUFFER_DESC transformDesc{};
    transformDesc.ByteWidth = sizeof(TransformConstants);
    transformDesc.Usage = D3D11_USAGE_DYNAMIC;
    transformDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    transformDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    winrt::check_hresult(m_graphics->D3DDevice()->CreateBuffer(
        &transformDesc,
        nullptr,
        &m_transformBuffer));

    D3D11_BUFFER_DESC colorDesc{};
    colorDesc.ByteWidth = sizeof(ColorConstants);
    colorDesc.Usage = D3D11_USAGE_DYNAMIC;
    colorDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    colorDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    winrt::check_hresult(m_graphics->D3DDevice()->CreateBuffer(
        &colorDesc,
        nullptr,
        &m_colorBuffer));

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

    D3D11_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.ScissorEnable = FALSE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    winrt::check_hresult(m_graphics->D3DDevice()->CreateRasterizerState(
        &rasterizerDesc,
        &m_rasterizerState));

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    winrt::check_hresult(m_graphics->D3DDevice()->CreateBlendState(
        &blendDesc,
        &m_blendState));

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = FALSE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    depthDesc.StencilEnable = FALSE;
    winrt::check_hresult(m_graphics->D3DDevice()->CreateDepthStencilState(
        &depthDesc,
        &m_depthStencilState));
}

void CanvasRenderer::Resize(uint32_t width, uint32_t height) {
    if (!m_swapChain || width == 0 || height == 0 || (width == m_width && height == m_height)) {
        return;
    }

    m_width = width;
    m_height = height;

    auto* context = m_graphics->D3DContext();
    context->OMSetRenderTargets(0, nullptr, nullptr);
    m_renderTargetView.Reset();

    winrt::check_hresult(m_swapChain->ResizeBuffers(
        0,
        width,
        height,
        DXGI_FORMAT_UNKNOWN,
        0));

    CreateBackBufferResources();
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

void CanvasRenderer::UpdateCaptureDiagnostic(
    const std::vector<std::shared_ptr<CaptureSource>>& sources) {
    const auto now = std::chrono::steady_clock::now();
    if (m_lastDiagnosticUpdate.time_since_epoch().count() != 0 &&
        now - m_lastDiagnosticUpdate < std::chrono::seconds(1)) {
        return;
    }
    m_lastDiagnosticUpdate = now;

    uint64_t totalFrames = 0;
    for (const auto& source : sources) {
        if (source) {
            totalFrames += source->FrameCount();
        }
    }

    const std::wstring title = L"Red's Stream Canvas - Output | Frames: " +
        std::to_wstring(totalFrames) + L" | Sources: " + std::to_wstring(sources.size());
    SetWindowTextW(m_hwnd, title.c_str());
}

void CanvasRenderer::SetTransform(
    float x,
    float y,
    float width,
    float height,
    float u0,
    float v0,
    float u1,
    float v1) {
    TransformConstants constants{};
    constants.destination[0] = x;
    constants.destination[1] = y;
    constants.destination[2] = width;
    constants.destination[3] = height;
    constants.sourceUv[0] = u0;
    constants.sourceUv[1] = v0;
    constants.sourceUv[2] = u1;
    constants.sourceUv[3] = v1;

    auto* context = m_graphics->D3DContext();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    winrt::check_hresult(context->Map(
        m_transformBuffer.Get(),
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &mapped));
    std::memcpy(mapped.pData, &constants, sizeof(constants));
    context->Unmap(m_transformBuffer.Get(), 0);

    ID3D11Buffer* buffer = m_transformBuffer.Get();
    context->VSSetConstantBuffers(0, 1, &buffer);
}

bool CanvasRenderer::DrawSource(CaptureSource const& source) {
    bool drew = false;

    source.WithFrame([&](const CaptureFrameSnapshot& snapshot) {
        auto& cache = m_textureCache[&source];
        if (!cache.view || cache.generation != snapshot.generation) {
            ComPtr<ID3D11ShaderResourceView> view;
            if (FAILED(m_graphics->D3DDevice()->CreateShaderResourceView(
                snapshot.texture.Get(),
                nullptr,
                &view))) {
                return;
            }
            cache.view = std::move(view);
            cache.generation = snapshot.generation;
        }

        const auto& transform = source.Transform();
        const float u0 = std::clamp(transform.cropLeft, 0.0f, 0.95f);
        const float v0 = std::clamp(transform.cropTop, 0.0f, 0.95f);
        const float u1 = 1.0f - std::clamp(transform.cropRight, 0.0f, 0.95f);
        const float v1 = 1.0f - std::clamp(transform.cropBottom, 0.0f, 0.95f);

        if (u1 <= u0 || v1 <= v0 || transform.width <= 0.0f || transform.height <= 0.0f) {
            return;
        }

        SetTransform(
            transform.x,
            transform.y,
            transform.width,
            transform.height,
            u0,
            v0,
            u1,
            v1);

        auto* context = m_graphics->D3DContext();
        context->PSSetShader(m_texturePixelShader.Get(), nullptr, 0);

        ID3D11ShaderResourceView* sourceView = cache.view.Get();
        context->PSSetShaderResources(0, 1, &sourceView);
        context->Draw(4, 0);

        // Unbind immediately so the capture callback can safely CopyResource
        // into this texture after the per-source lock is released.
        ID3D11ShaderResourceView* nullView = nullptr;
        context->PSSetShaderResources(0, 1, &nullView);
        drew = true;
    });

    return drew;
}

void CanvasRenderer::DrawSolidRect(
    float x,
    float y,
    float width,
    float height,
    const float color[4]) {
    if (width <= 0.0f || height <= 0.0f) {
        return;
    }

    SetTransform(x, y, width, height);

    ColorConstants constants{};
    constants.color[0] = color[0];
    constants.color[1] = color[1];
    constants.color[2] = color[2];
    constants.color[3] = color[3];

    auto* context = m_graphics->D3DContext();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    winrt::check_hresult(context->Map(
        m_colorBuffer.Get(),
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &mapped));
    std::memcpy(mapped.pData, &constants, sizeof(constants));
    context->Unmap(m_colorBuffer.Get(), 0);

    ID3D11Buffer* colorBuffer = m_colorBuffer.Get();
    context->PSSetConstantBuffers(0, 1, &colorBuffer);
    context->PSSetShader(m_colorPixelShader.Get(), nullptr, 0);
    context->Draw(4, 0);
}

void CanvasRenderer::DrawSelection(CaptureSource const& source, bool cropMode) {
    const auto& t = source.Transform();
    if (t.width <= 0.0f || t.height <= 0.0f || m_width == 0 || m_height == 0) {
        return;
    }

    const float selectionColor[4] = { 0.20f, 0.70f, 1.00f, 1.0f };
    const float cropColor[4] = { 1.00f, 0.62f, 0.12f, 1.0f };
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    const float* outline = cropMode ? cropColor : selectionColor;

    const float lineX = 2.0f / static_cast<float>(m_width);
    const float lineY = 2.0f / static_cast<float>(m_height);

    DrawSolidRect(t.x, t.y, t.width, lineY, outline);
    DrawSolidRect(t.x, t.y + t.height - lineY, t.width, lineY, outline);
    DrawSolidRect(t.x, t.y, lineX, t.height, outline);
    DrawSolidRect(t.x + t.width - lineX, t.y, lineX, t.height, outline);

    const float outerW = 10.0f / static_cast<float>(m_width);
    const float outerH = 10.0f / static_cast<float>(m_height);
    const float innerW = 6.0f / static_cast<float>(m_width);
    const float innerH = 6.0f / static_cast<float>(m_height);

    const float left = t.x;
    const float right = t.x + t.width;
    const float top = t.y;
    const float bottom = t.y + t.height;
    const float centerX = (left + right) * 0.5f;
    const float centerY = (top + bottom) * 0.5f;

    const float points[8][2] = {
        { left, top },
        { centerX, top },
        { right, top },
        { right, centerY },
        { right, bottom },
        { centerX, bottom },
        { left, bottom },
        { left, centerY },
    };

    for (const auto& point : points) {
        DrawSolidRect(
            point[0] - outerW * 0.5f,
            point[1] - outerH * 0.5f,
            outerW,
            outerH,
            outline);
        DrawSolidRect(
            point[0] - innerW * 0.5f,
            point[1] - innerH * 0.5f,
            innerW,
            innerH,
            white);
    }
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

    // D2D used by older builds could leave arbitrary D3D state behind. Clear
    // everything and explicitly bind the tiny compositor pipeline every frame.
    context->ClearState();

    ID3D11RenderTargetView* renderTarget = m_renderTargetView.Get();
    context->OMSetRenderTargets(1, &renderTarget, nullptr);

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);
    context->RSSetState(m_rasterizerState.Get());

    const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xffffffffu);
    context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);

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

    if (editMode && selected && !selected->IsClosed() && selected->Transform().visible) {
        DrawSelection(*selected, cropMode);
    }

    UpdateCaptureDiagnostic(sources);

    const HRESULT presentHr = m_swapChain->Present(0, 0);
    if (presentHr == DXGI_STATUS_OCCLUDED) {
        return;
    }
    winrt::check_hresult(presentHr);

    const auto end = std::chrono::steady_clock::now();
    const double renderMs = std::chrono::duration<double, std::milli>(end - begin).count();
    UpdateAdaptivePerformance(renderMs, visibleCount);
}
