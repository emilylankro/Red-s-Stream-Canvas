#include "App.h"
#include "CanvasRenderer.h"
#include "CaptureSource.h"
#include "GraphicsDevice.h"

#include <shobjidl_core.h>
#include <algorithm>
#include <format>

using namespace winrt;
using namespace winrt::Windows::Graphics::Capture;

namespace {
constexpr wchar_t ControlClassName[] = L"RSC_ControlWindow";
constexpr wchar_t OutputClassName[] = L"RSC_OutputWindow";
constexpr UINT_PTR RenderTimerId = 1;
constexpr int IdAdd = 1001;
constexpr int IdClear = 1002;
constexpr int IdAuto = 1003;
}

App::App(HINSTANCE instance)
    : m_instance(instance), m_graphics(std::make_shared<GraphicsDevice>()) {
}

int App::Run(int showCommand) {
    RegisterWindowClasses();
    CreateWindows(showCommand);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void App::RegisterWindowClasses() {
    WNDCLASSEXW control{};
    control.cbSize = sizeof(control);
    control.hInstance = m_instance;
    control.lpfnWndProc = &App::ControlWndProc;
    control.lpszClassName = ControlClassName;
    control.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    control.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    winrt::check_bool(RegisterClassExW(&control) != 0);

    WNDCLASSEXW output{};
    output.cbSize = sizeof(output);
    output.hInstance = m_instance;
    output.lpfnWndProc = &App::OutputWndProc;
    output.lpszClassName = OutputClassName;
    output.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    output.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    winrt::check_bool(RegisterClassExW(&output) != 0);
}

void App::CreateWindows(int showCommand) {
    m_controlWindow = CreateWindowExW(
        0,
        ControlClassName,
        L"Red's Stream Canvas — Control",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        520,
        220,
        nullptr,
        nullptr,
        m_instance,
        this);
    winrt::check_bool(m_controlWindow != nullptr);

    m_outputWindow = CreateWindowExW(
        0,
        OutputClassName,
        L"Red's Stream Canvas — Output",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        720,
        nullptr,
        nullptr,
        m_instance,
        this);
    winrt::check_bool(m_outputWindow != nullptr);

    m_renderer = std::make_unique<CanvasRenderer>(m_graphics, m_outputWindow);
    RECT outputClient{};
    if (GetClientRect(m_outputWindow, &outputClient)) {
        m_renderer->Resize(
            static_cast<uint32_t>(std::max<LONG>(outputClient.right - outputClient.left, 1)),
            static_cast<uint32_t>(std::max<LONG>(outputClient.bottom - outputClient.top, 1)));
    }
    CreateControlButtons();
    UpdateStatusText();
    UpdateRenderTimer();

    ShowWindow(m_controlWindow, showCommand);
    ShowWindow(m_outputWindow, SW_SHOWNORMAL);
    UpdateWindow(m_controlWindow);
    UpdateWindow(m_outputWindow);
}

void App::CreateControlButtons() {
    m_addButton = CreateWindowExW(
        0, L"BUTTON", L"+ Add screen / window",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 20, 190, 36,
        m_controlWindow,
        reinterpret_cast<HMENU>(IdAdd),
        m_instance,
        nullptr);

    m_clearButton = CreateWindowExW(
        0, L"BUTTON", L"Clear all",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        220, 20, 110, 36,
        m_controlWindow,
        reinterpret_cast<HMENU>(IdClear),
        m_instance,
        nullptr);

    m_autoButton = CreateWindowExW(
        0, L"BUTTON", L"Auto performance: ON",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        340, 20, 150, 36,
        m_controlWindow,
        reinterpret_cast<HMENU>(IdAuto),
        m_instance,
        nullptr);

    m_statusText = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE,
        20, 78, 470, 82,
        m_controlWindow,
        nullptr,
        m_instance,
        nullptr);
}

void App::UpdateStatusText() {
    if (!m_statusText || !m_renderer) {
        return;
    }

    const auto vramMb = m_graphics->DedicatedVideoMemory() / (1024ull * 1024ull);
    const std::wstring gpuMode = m_graphics->UsingWarp() ? L"software fallback" : L"GPU";
    const auto text = std::format(
        L"Sources: {}\r\nOutput cadence: {} FPS ({})\r\nRenderer: {} | Dedicated VRAM: {} MB",
        m_sources.size(),
        m_renderer->TargetFps(),
        m_renderer->AutoPerformance() ? L"auto" : L"fixed",
        gpuMode,
        vramMb);

    SetWindowTextW(m_statusText, text.c_str());
    SetWindowTextW(m_autoButton, m_renderer->AutoPerformance() ? L"Auto performance: ON" : L"Auto performance: OFF");
}

void App::UpdateRenderTimer() {
    if (!m_outputWindow || !m_renderer) {
        return;
    }

    const uint32_t fps = std::max(m_renderer->TargetFps(), 1u);
    if (fps == m_lastTimerFps) {
        return;
    }

    KillTimer(m_outputWindow, RenderTimerId);
    SetTimer(m_outputWindow, RenderTimerId, std::max(1u, 1000u / fps), nullptr);
    m_lastTimerFps = fps;

    for (auto& source : m_sources) {
        source->SetMaxFps(fps);
    }
    UpdateStatusText();
}

void App::PruneClosedSources() {
    const auto before = m_sources.size();
    std::erase_if(m_sources, [](const std::shared_ptr<CaptureSource>& source) {
        return !source || source->IsClosed();
    });
    if (m_sources.size() != before) {
        UpdateStatusText();
    }
}

winrt::fire_and_forget App::AddSourceAsync() {
    GraphicsCapturePicker picker;
    auto initializeWithWindow = picker.as<IInitializeWithWindow>();
    winrt::check_hresult(initializeWithWindow->Initialize(m_controlWindow));

    const auto item = co_await picker.PickSingleItemAsync();
    if (!item) {
        co_return;
    }

    auto source = std::make_shared<CaptureSource>(m_graphics, item);
    source->SetMaxFps(m_renderer->TargetFps());
    source->Start();
    m_sources.push_back(std::move(source));
    UpdateStatusText();
}

LRESULT CALLBACK App::ControlWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = nullptr;
    if (message == WM_NCCREATE) {
        auto create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    return app ? app->HandleControlMessage(hwnd, message, wParam, lParam)
               : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK App::OutputWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = nullptr;
    if (message == WM_NCCREATE) {
        auto create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    return app ? app->HandleOutputMessage(hwnd, message, wParam, lParam)
               : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT App::HandleControlMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IdAdd:
            AddSourceAsync();
            return 0;
        case IdClear:
            for (auto& source : m_sources) {
                if (source) source->Stop();
            }
            m_sources.clear();
            UpdateStatusText();
            return 0;
        case IdAuto:
            if (m_renderer) {
                if (m_renderer->AutoPerformance()) {
                    m_renderer->SetFixedFps(30);
                } else {
                    m_renderer->SetAutoPerformance(true);
                }
                UpdateRenderTimer();
                UpdateStatusText();
            }
            return 0;
        }
        break;
    case WM_DESTROY:
        if (m_outputWindow) DestroyWindow(m_outputWindow);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT App::HandleOutputMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        if (m_renderer && wParam != SIZE_MINIMIZED) {
            const uint32_t width = LOWORD(lParam);
            const uint32_t height = HIWORD(lParam);
            m_renderer->Resize(width, height);
        }
        return 0;
    case WM_TIMER:
        if (wParam == RenderTimerId && m_renderer) {
            PruneClosedSources();
            m_renderer->Render(m_sources);
            UpdateRenderTimer();
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
