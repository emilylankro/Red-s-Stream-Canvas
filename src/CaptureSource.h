#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

#include <d3d11_4.h>
#include <wrl/client.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

class GraphicsDevice;

struct CaptureFrameSnapshot {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    uint32_t width{ 0 };
    uint32_t height{ 0 };
    uint64_t generation{ 0 };
};

struct CanvasTransform {
    float x{ 0.05f };
    float y{ 0.05f };
    float width{ 0.90f };
    float height{ 0.90f };
    float cropLeft{ 0.0f };
    float cropTop{ 0.0f };
    float cropRight{ 0.0f };
    float cropBottom{ 0.0f };
    bool visible{ true };
};

class CaptureSource : public std::enable_shared_from_this<CaptureSource> {
public:
    CaptureSource(
        std::shared_ptr<GraphicsDevice> graphics,
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item);
    ~CaptureSource();

    void Start();
    void Stop();
    void SetMaxFps(uint32_t fps) noexcept;

    CaptureFrameSnapshot Snapshot() const;
    std::wstring Name() const;
    bool IsClosed() const noexcept { return m_closed.load(); }
    uint64_t FrameCount() const noexcept { return m_frameCount.load(std::memory_order_relaxed); }

    CanvasTransform const& Transform() const noexcept { return m_transform; }
    CanvasTransform& Transform() noexcept { return m_transform; }
    void ResetCrop() noexcept;

private:
    void OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const&);
    void EnsureCopyTexture(ID3D11Texture2D* source, uint32_t width, uint32_t height);

    std::shared_ptr<GraphicsDevice> m_graphics;
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };
    winrt::event_token m_frameToken{};
    winrt::event_token m_closedToken{};

    mutable std::mutex m_mutex;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_copyTexture;
    uint32_t m_width{ 0 };
    uint32_t m_height{ 0 };
    uint64_t m_generation{ 0 };
    std::atomic<bool> m_closed{ false };
    std::atomic<bool> m_started{ false };
    std::atomic<bool> m_stopped{ false };
    std::atomic<uint32_t> m_maxFps{ 30 };
    std::atomic<uint64_t> m_frameCount{ 0 };
    std::chrono::steady_clock::time_point m_lastAcceptedFrame{};
    CanvasTransform m_transform{};
};
