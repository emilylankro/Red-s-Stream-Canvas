#pragma once

#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>

#include <windows.h>
#include <d2d1_3.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

class GraphicsDevice;
class CaptureSource;

class CanvasRenderer {
public:
    CanvasRenderer(std::shared_ptr<GraphicsDevice> graphics, HWND hwnd);

    void Resize(uint32_t width, uint32_t height);
    void Render(const std::vector<std::shared_ptr<CaptureSource>>& sources);
    uint32_t TargetFps() const noexcept { return m_targetFps; }
    bool AutoPerformance() const noexcept { return m_autoPerformance; }
    void SetAutoPerformance(bool enabled) noexcept { m_autoPerformance = enabled; }
    void SetFixedFps(uint32_t fps) noexcept;

private:
    struct BitmapCacheEntry {
        uint64_t generation{ 0 };
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
    };

    void CreateSwapChain();
    void CreateTargetBitmap();
    void UpdateAdaptivePerformance(double renderMs, size_t sourceCount);
    D2D1_RECT_F FitRect(float cellX, float cellY, float cellW, float cellH, float sourceW, float sourceH) const;

    std::shared_ptr<GraphicsDevice> m_graphics;
    HWND m_hwnd{};
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext2> m_d2dContext;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_targetBitmap;
    std::unordered_map<const CaptureSource*, BitmapCacheEntry> m_bitmapCache;
    uint32_t m_width{ 1280 };
    uint32_t m_height{ 720 };
    uint32_t m_targetFps{ 30 };
    bool m_autoPerformance{ true };
    uint32_t m_perfSampleCount{ 0 };
    double m_perfAccumulatedMs{ 0.0 };
};
