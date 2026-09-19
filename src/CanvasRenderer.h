#pragma once

#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>

#include <windows.h>
#include <d2d1_3.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

class GraphicsDevice;
class CaptureSource;
struct CanvasTransform;

class CanvasRenderer {
public:
    CanvasRenderer(std::shared_ptr<GraphicsDevice> graphics, HWND hwnd);

    void Resize(uint32_t width, uint32_t height);
    void Render(
        const std::vector<std::shared_ptr<CaptureSource>>& sources,
        const CaptureSource* selected,
        bool editMode,
        bool cropMode);

    uint32_t TargetFps() const noexcept { return m_targetFps; }
    bool AutoPerformance() const noexcept { return m_autoPerformance; }
    void SetAutoPerformance(bool enabled) noexcept { m_autoPerformance = enabled; }
    void SetFixedFps(uint32_t fps) noexcept;
    uint32_t Width() const noexcept { return m_width; }
    uint32_t Height() const noexcept { return m_height; }

private:
    struct TextureCacheEntry {
        uint64_t generation{ 0 };
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    };

    struct TransformConstants {
        float destination[4];
        float sourceUv[4];
    };

    void CreateSwapChain();
    void CreateBackBufferResources();
    void CreateTexturePipeline();
    void CreateEditorResources();
    void UpdateAdaptivePerformance(double renderMs, size_t sourceCount);
    void UpdateCaptureDiagnostic(const std::vector<std::shared_ptr<CaptureSource>>& sources);
    D2D1_RECT_F DestinationRect(const CanvasTransform& transform) const;
    void DrawSelection(const D2D1_RECT_F& rect, bool cropMode);
    bool DrawSource(CaptureSource const& source);

    std::shared_ptr<GraphicsDevice> m_graphics;
    HWND m_hwnd{};

    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_transformBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState;

    Microsoft::WRL::ComPtr<ID2D1DeviceContext2> m_d2dContext;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_targetBitmap;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_selectionBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_cropBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_handleFillBrush;

    std::unordered_map<const CaptureSource*, TextureCacheEntry> m_textureCache;

    uint32_t m_width{ 1280 };
    uint32_t m_height{ 720 };
    uint32_t m_targetFps{ 30 };
    bool m_autoPerformance{ true };
    uint32_t m_perfSampleCount{ 0 };
    double m_perfAccumulatedMs{ 0.0 };
    std::chrono::steady_clock::time_point m_lastDiagnosticUpdate{};
};
