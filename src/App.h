#pragma once

#include <memory>
#include <vector>
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>

#include "CaptureSource.h"

class GraphicsDevice;
class CanvasRenderer;

class App {
public:
    explicit App(HINSTANCE instance);
    ~App();
    int Run(int showCommand);

private:
    enum class DragMode {
        None,
        Move,
        ResizeLeft,
        ResizeRight,
        ResizeTop,
        ResizeBottom,
        ResizeTopLeft,
        ResizeTopRight,
        ResizeBottomLeft,
        ResizeBottomRight,
        CropLeft,
        CropRight,
        CropTop,
        CropBottom,
    };

    static LRESULT CALLBACK ControlWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK OutputWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleControlMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleOutputMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void RegisterWindowClasses();
    void CreateWindows(int showCommand);
    void CreateControlButtons();
    void UpdateStatusText();
    void UpdateRenderTimer();
    void PruneClosedSources();
    winrt::fire_and_forget AddSourceAsync();

    void ApplyGridLayout();
    void ApplySideBySideLayout();
    void ApplyPictureInPictureLayout();
    void RemoveSelected();
    void ToggleSelectedVisibility();
    void BringSelectedToFront();
    void SendSelectedToBack();
    void SetEditMode(bool enabled);
    void SetCropMode(bool enabled);

    std::shared_ptr<CaptureSource> HitTestSource(float x, float y) const;
    DragMode HitTestHandle(const CaptureSource& source, float x, float y) const;
    void BeginPointerDrag(int x, int y);
    void UpdatePointerDrag(int x, int y);
    void EndPointerDrag();

    HINSTANCE m_instance{};
    HWND m_controlWindow{};
    HWND m_outputWindow{};
    HWND m_addButton{};
    HWND m_clearButton{};
    HWND m_editButton{};
    HWND m_cropButton{};
    HWND m_autoButton{};
    HWND m_gridButton{};
    HWND m_sideButton{};
    HWND m_pipButton{};
    HWND m_resetCropButton{};
    HWND m_visibilityButton{};
    HWND m_backButton{};
    HWND m_frontButton{};
    HWND m_removeButton{};
    HWND m_statusText{};

    std::shared_ptr<GraphicsDevice> m_graphics;
    std::unique_ptr<CanvasRenderer> m_renderer;
    std::vector<std::shared_ptr<CaptureSource>> m_sources;
    std::shared_ptr<CaptureSource> m_selected;

    uint32_t m_lastTimerFps{ 0 };
    bool m_editMode{ true };
    bool m_cropMode{ false };
    DragMode m_dragMode{ DragMode::None };
    POINT m_dragStartPoint{};
    CanvasTransform m_dragStartTransform{};
};
