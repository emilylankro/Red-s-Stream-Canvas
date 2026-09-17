#pragma once

#include <memory>
#include <vector>
#include <windows.h>
#include <winrt/Windows.Graphics.Capture.h>

class GraphicsDevice;
class CaptureSource;
class CanvasRenderer;

class App {
public:
    App(HINSTANCE instance);
    int Run(int showCommand);

private:
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

    HINSTANCE m_instance{};
    HWND m_controlWindow{};
    HWND m_outputWindow{};
    HWND m_addButton{};
    HWND m_clearButton{};
    HWND m_autoButton{};
    HWND m_statusText{};

    std::shared_ptr<GraphicsDevice> m_graphics;
    std::unique_ptr<CanvasRenderer> m_renderer;
    std::vector<std::shared_ptr<CaptureSource>> m_sources;
    uint32_t m_lastTimerFps{ 0 };
};
