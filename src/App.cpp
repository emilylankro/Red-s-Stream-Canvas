#include "App.h"
#include "CanvasRenderer.h"
#include "GraphicsDevice.h"

#include <shobjidl_core.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
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
constexpr int IdEdit = 1004;
constexpr int IdCrop = 1005;
constexpr int IdGrid = 1006;
constexpr int IdSide = 1007;
constexpr int IdPip = 1008;
constexpr int IdResetCrop = 1009;
constexpr int IdVisibility = 1010;
constexpr int IdBack = 1011;
constexpr int IdFront = 1012;
constexpr int IdRemove = 1013;
constexpr float MinSize = 0.05f;
constexpr float MinVisibleCrop = 0.05f;

HMENU ButtonId(int id) {
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}
}

App::App(HINSTANCE instance)
    : m_instance(instance), m_graphics(std::make_shared<GraphicsDevice>()) {
}

App::~App() {
    for (auto& source : m_sources) {
        if (source) {
            source->Stop();
        }
    }
    if (m_outputWindow) {
        KillTimer(m_outputWindow, RenderTimerId);
    }
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
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        760,
        330,
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
    m_addButton = CreateWindowExW(0, L"BUTTON", L"+ Add screen / window", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        18, 18, 180, 34, m_controlWindow, ButtonId(IdAdd), m_instance, nullptr);
    m_clearButton = CreateWindowExW(0, L"BUTTON", L"Clear all", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        208, 18, 90, 34, m_controlWindow, ButtonId(IdClear), m_instance, nullptr);
    m_editButton = CreateWindowExW(0, L"BUTTON", L"Edit: ON", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        308, 18, 100, 34, m_controlWindow, ButtonId(IdEdit), m_instance, nullptr);
    m_cropButton = CreateWindowExW(0, L"BUTTON", L"Crop: OFF", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        418, 18, 100, 34, m_controlWindow, ButtonId(IdCrop), m_instance, nullptr);
    m_autoButton = CreateWindowExW(0, L"BUTTON", L"Auto performance: ON", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        528, 18, 190, 34, m_controlWindow, ButtonId(IdAuto), m_instance, nullptr);

    m_gridButton = CreateWindowExW(0, L"BUTTON", L"Grid", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        18, 64, 100, 34, m_controlWindow, ButtonId(IdGrid), m_instance, nullptr);
    m_sideButton = CreateWindowExW(0, L"BUTTON", L"Side by side", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        128, 64, 120, 34, m_controlWindow, ButtonId(IdSide), m_instance, nullptr);
    m_pipButton = CreateWindowExW(0, L"BUTTON", L"Picture-in-picture", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        258, 64, 150, 34, m_controlWindow, ButtonId(IdPip), m_instance, nullptr);
    m_resetCropButton = CreateWindowExW(0, L"BUTTON", L"Reset crop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        418, 64, 100, 34, m_controlWindow, ButtonId(IdResetCrop), m_instance, nullptr);

    m_visibilityButton = CreateWindowExW(0, L"BUTTON", L"Hide selected", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        18, 110, 120, 34, m_controlWindow, ButtonId(IdVisibility), m_instance, nullptr);
    m_backButton = CreateWindowExW(0, L"BUTTON", L"Send back", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        148, 110, 100, 34, m_controlWindow, ButtonId(IdBack), m_instance, nullptr);
    m_frontButton = CreateWindowExW(0, L"BUTTON", L"Bring front", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        258, 110, 100, 34, m_controlWindow, ButtonId(IdFront), m_instance, nullptr);
    m_removeButton = CreateWindowExW(0, L"BUTTON", L"Remove selected", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        368, 110, 140, 34, m_controlWindow, ButtonId(IdRemove), m_instance, nullptr);

    m_statusText = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        18, 158, 700, 120, m_controlWindow, nullptr, m_instance, nullptr);
}

void App::UpdateStatusText() {
    if (!m_statusText || !m_renderer) {
        return;
    }

    const auto vramMb = m_graphics->DedicatedVideoMemory() / (1024ull * 1024ull);
    const std::wstring gpuMode = m_graphics->UsingWarp() ? L"software fallback" : L"GPU";
    const std::wstring selectedName = m_selected ? m_selected->Name() : L"None";
    const auto text = std::format(
        L"Sources: {} | Selected: {}\r\nOutput: {} FPS ({}) | Renderer: {} | Dedicated VRAM: {} MB\r\n"
        L"Edit ON: drag a source to move it; drag handles to resize. Crop ON: drag an edge handle inward.\r\n"
        L"Shortcuts in Output: G grid, P picture-in-picture, C crop, H hide/show, Delete remove.",
        m_sources.size(),
        selectedName,
        m_renderer->TargetFps(),
        m_renderer->AutoPerformance() ? L"auto" : L"fixed",
        gpuMode,
        vramMb);

    SetWindowTextW(m_statusText, text.c_str());
    SetWindowTextW(m_editButton, m_editMode ? L"Edit: ON" : L"Edit: OFF");
    SetWindowTextW(m_cropButton, m_cropMode ? L"Crop: ON" : L"Crop: OFF");
    SetWindowTextW(m_autoButton, m_renderer->AutoPerformance() ? L"Auto performance: ON" : L"Auto performance: OFF");
    if (m_selected) {
        SetWindowTextW(m_visibilityButton, m_selected->Transform().visible ? L"Hide selected" : L"Show selected");
    } else {
        SetWindowTextW(m_visibilityButton, L"Hide/show selected");
    }
}

void App::UpdateRenderTimer() {
    if (!m_outputWindow || !m_renderer) {
        return;
    }

    const uint32_t fps = std::max(m_renderer->TargetFps(), 1u);
    if (fps != m_lastTimerFps) {
        KillTimer(m_outputWindow, RenderTimerId);
        SetTimer(m_outputWindow, RenderTimerId, std::max(1u, 1000u / fps), nullptr);
        m_lastTimerFps = fps;

        for (auto& source : m_sources) {
            source->SetMaxFps(fps);
        }
    }
    UpdateStatusText();
}

void App::PruneClosedSources() {
    const auto before = m_sources.size();
    std::erase_if(m_sources, [this](const std::shared_ptr<CaptureSource>& source) {
        const bool remove = !source || source->IsClosed();
        if (remove && source == m_selected) {
            m_selected.reset();
        }
        return remove;
    });
    if (m_sources.size() != before) {
        UpdateStatusText();
    }
}

winrt::fire_and_forget App::AddSourceAsync() {
    try {
        GraphicsCapturePicker picker;
        auto initializeWithWindow = picker.as<IInitializeWithWindow>();
        winrt::check_hresult(initializeWithWindow->Initialize(m_controlWindow));

        auto item = co_await picker.PickSingleItemAsync();
        if (!item) {
            co_return;
        }

        auto source = std::make_shared<CaptureSource>(m_graphics, item);
        source->SetMaxFps(m_renderer->TargetFps());
        source->Start();
        m_sources.push_back(source);
        m_selected = source;
        ApplyGridLayout();
        UpdateStatusText();
    } catch (const winrt::hresult_error& error) {
        const std::wstring message = L"Could not add this capture source.\n\n" + std::wstring(error.message());
        MessageBoxW(m_controlWindow, message.c_str(), L"Red's Stream Canvas", MB_OK | MB_ICONERROR);
    }
}

void App::ApplyGridLayout() {
    std::vector<std::shared_ptr<CaptureSource>> visible;
    for (auto& source : m_sources) {
        if (source && source->Transform().visible) visible.push_back(source);
    }
    if (visible.empty()) return;

    const auto columns = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(visible.size()))));
    const auto rows = static_cast<uint32_t>((visible.size() + columns - 1) / columns);
    const float gap = 0.008f;
    const float cellW = 1.0f / static_cast<float>(columns);
    const float cellH = 1.0f / static_cast<float>(rows);

    for (size_t i = 0; i < visible.size(); ++i) {
        const uint32_t col = static_cast<uint32_t>(i % columns);
        const uint32_t row = static_cast<uint32_t>(i / columns);
        auto& t = visible[i]->Transform();
        t.x = col * cellW + gap;
        t.y = row * cellH + gap;
        t.width = std::max(cellW - gap * 2.0f, MinSize);
        t.height = std::max(cellH - gap * 2.0f, MinSize);
    }
}

void App::ApplySideBySideLayout() {
    std::vector<std::shared_ptr<CaptureSource>> visible;
    for (auto& source : m_sources) {
        if (source && source->Transform().visible) visible.push_back(source);
    }
    if (visible.empty()) return;

    const float gap = 0.008f;
    const float cellW = 1.0f / static_cast<float>(visible.size());
    for (size_t i = 0; i < visible.size(); ++i) {
        auto& t = visible[i]->Transform();
        t.x = static_cast<float>(i) * cellW + gap;
        t.y = gap;
        t.width = std::max(cellW - gap * 2.0f, MinSize);
        t.height = 1.0f - gap * 2.0f;
    }
}

void App::ApplyPictureInPictureLayout() {
    std::vector<std::shared_ptr<CaptureSource>> visible;
    for (auto& source : m_sources) {
        if (source && source->Transform().visible) visible.push_back(source);
    }
    if (visible.empty()) return;

    auto& main = visible.front()->Transform();
    main.x = 0.0f;
    main.y = 0.0f;
    main.width = 1.0f;
    main.height = 1.0f;

    const float pipW = 0.27f;
    const float pipH = 0.27f;
    const float gap = 0.018f;
    for (size_t i = 1; i < visible.size(); ++i) {
        const size_t slot = i - 1;
        const size_t column = slot % 3;
        const size_t row = slot / 3;
        auto& t = visible[i]->Transform();
        t.width = pipW;
        t.height = pipH;
        t.x = std::max(0.0f, 1.0f - gap - pipW - static_cast<float>(column) * (pipW + gap));
        t.y = std::max(0.0f, 1.0f - gap - pipH - static_cast<float>(row) * (pipH + gap));
    }
}

void App::RemoveSelected() {
    if (!m_selected) return;
    m_selected->Stop();
    std::erase(m_sources, m_selected);
    m_selected.reset();
    UpdateStatusText();
}

void App::ToggleSelectedVisibility() {
    if (!m_selected) return;
    m_selected->Transform().visible = !m_selected->Transform().visible;
    UpdateStatusText();
}

void App::BringSelectedToFront() {
    if (!m_selected) return;
    const auto selected = m_selected;
    std::erase(m_sources, selected);
    m_sources.push_back(selected);
}

void App::SendSelectedToBack() {
    if (!m_selected) return;
    const auto selected = m_selected;
    std::erase(m_sources, selected);
    m_sources.insert(m_sources.begin(), selected);
}

void App::SetEditMode(bool enabled) {
    m_editMode = enabled;
    if (!enabled) {
        EndPointerDrag();
        m_cropMode = false;
    }
    UpdateStatusText();
}

void App::SetCropMode(bool enabled) {
    m_cropMode = enabled && m_editMode;
    UpdateStatusText();
}

std::shared_ptr<CaptureSource> App::HitTestSource(float x, float y) const {
    if (!m_renderer) return {};
    const float width = static_cast<float>(std::max(m_renderer->Width(), 1u));
    const float height = static_cast<float>(std::max(m_renderer->Height(), 1u));
    const float nx = x / width;
    const float ny = y / height;

    for (auto it = m_sources.rbegin(); it != m_sources.rend(); ++it) {
        const auto& source = *it;
        if (!source || source->IsClosed() || !source->Transform().visible) continue;
        const auto& t = source->Transform();
        if (nx >= t.x && nx <= t.x + t.width && ny >= t.y && ny <= t.y + t.height) {
            return source;
        }
    }
    return {};
}

App::DragMode App::HitTestHandle(const CaptureSource& source, float x, float y) const {
    if (!m_renderer) return DragMode::None;
    const auto& t = source.Transform();
    const float canvasW = static_cast<float>(std::max(m_renderer->Width(), 1u));
    const float canvasH = static_cast<float>(std::max(m_renderer->Height(), 1u));
    const float left = t.x * canvasW;
    const float top = t.y * canvasH;
    const float right = (t.x + t.width) * canvasW;
    const float bottom = (t.y + t.height) * canvasH;
    constexpr float hit = 12.0f;

    const bool nearLeft = std::abs(x - left) <= hit && y >= top - hit && y <= bottom + hit;
    const bool nearRight = std::abs(x - right) <= hit && y >= top - hit && y <= bottom + hit;
    const bool nearTop = std::abs(y - top) <= hit && x >= left - hit && x <= right + hit;
    const bool nearBottom = std::abs(y - bottom) <= hit && x >= left - hit && x <= right + hit;

    if (m_cropMode) {
        if (nearLeft) return DragMode::CropLeft;
        if (nearRight) return DragMode::CropRight;
        if (nearTop) return DragMode::CropTop;
        if (nearBottom) return DragMode::CropBottom;
        return DragMode::None;
    }

    if (nearLeft && nearTop) return DragMode::ResizeTopLeft;
    if (nearRight && nearTop) return DragMode::ResizeTopRight;
    if (nearLeft && nearBottom) return DragMode::ResizeBottomLeft;
    if (nearRight && nearBottom) return DragMode::ResizeBottomRight;
    if (nearLeft) return DragMode::ResizeLeft;
    if (nearRight) return DragMode::ResizeRight;
    if (nearTop) return DragMode::ResizeTop;
    if (nearBottom) return DragMode::ResizeBottom;
    return DragMode::None;
}

void App::BeginPointerDrag(int x, int y) {
    if (!m_editMode || !m_renderer) return;

    if (m_selected && m_selected->Transform().visible) {
        const auto handle = HitTestHandle(*m_selected, static_cast<float>(x), static_cast<float>(y));
        if (handle != DragMode::None) {
            m_dragMode = handle;
        }
    }

    if (m_dragMode == DragMode::None) {
        auto hit = HitTestSource(static_cast<float>(x), static_cast<float>(y));
        if (!hit) {
            m_selected.reset();
            UpdateStatusText();
            return;
        }
        m_selected = std::move(hit);
        const auto handle = HitTestHandle(*m_selected, static_cast<float>(x), static_cast<float>(y));
        m_dragMode = handle != DragMode::None ? handle : (m_cropMode ? DragMode::None : DragMode::Move);
    }

    if (m_selected && m_dragMode != DragMode::None) {
        m_dragStartPoint = POINT{ x, y };
        m_dragStartTransform = m_selected->Transform();
        SetCapture(m_outputWindow);
    }
    UpdateStatusText();
}

void App::UpdatePointerDrag(int x, int y) {
    if (!m_selected || m_dragMode == DragMode::None || !m_renderer) return;

    const float canvasW = static_cast<float>(std::max(m_renderer->Width(), 1u));
    const float canvasH = static_cast<float>(std::max(m_renderer->Height(), 1u));
    const float dx = static_cast<float>(x - m_dragStartPoint.x) / canvasW;
    const float dy = static_cast<float>(y - m_dragStartPoint.y) / canvasH;
    auto t = m_dragStartTransform;
    const float right = m_dragStartTransform.x + m_dragStartTransform.width;
    const float bottom = m_dragStartTransform.y + m_dragStartTransform.height;

    auto resizeLeft = [&] {
        t.x = std::clamp(m_dragStartTransform.x + dx, 0.0f, right - MinSize);
        t.width = right - t.x;
    };
    auto resizeRight = [&] {
        t.width = std::clamp(m_dragStartTransform.width + dx, MinSize, 1.0f - m_dragStartTransform.x);
    };
    auto resizeTop = [&] {
        t.y = std::clamp(m_dragStartTransform.y + dy, 0.0f, bottom - MinSize);
        t.height = bottom - t.y;
    };
    auto resizeBottom = [&] {
        t.height = std::clamp(m_dragStartTransform.height + dy, MinSize, 1.0f - m_dragStartTransform.y);
    };

    switch (m_dragMode) {
    case DragMode::Move:
        t.x = std::clamp(m_dragStartTransform.x + dx, 0.0f, 1.0f - m_dragStartTransform.width);
        t.y = std::clamp(m_dragStartTransform.y + dy, 0.0f, 1.0f - m_dragStartTransform.height);
        break;
    case DragMode::ResizeLeft: resizeLeft(); break;
    case DragMode::ResizeRight: resizeRight(); break;
    case DragMode::ResizeTop: resizeTop(); break;
    case DragMode::ResizeBottom: resizeBottom(); break;
    case DragMode::ResizeTopLeft: resizeLeft(); resizeTop(); break;
    case DragMode::ResizeTopRight: resizeRight(); resizeTop(); break;
    case DragMode::ResizeBottomLeft: resizeLeft(); resizeBottom(); break;
    case DragMode::ResizeBottomRight: resizeRight(); resizeBottom(); break;
    case DragMode::CropLeft: {
        const float visible = 1.0f - m_dragStartTransform.cropLeft - m_dragStartTransform.cropRight;
        const float delta = dx / std::max(m_dragStartTransform.width, MinSize) * visible;
        t.cropLeft = std::clamp(m_dragStartTransform.cropLeft + delta, 0.0f,
            1.0f - m_dragStartTransform.cropRight - MinVisibleCrop);
        break;
    }
    case DragMode::CropRight: {
        const float visible = 1.0f - m_dragStartTransform.cropLeft - m_dragStartTransform.cropRight;
        const float delta = -dx / std::max(m_dragStartTransform.width, MinSize) * visible;
        t.cropRight = std::clamp(m_dragStartTransform.cropRight + delta, 0.0f,
            1.0f - m_dragStartTransform.cropLeft - MinVisibleCrop);
        break;
    }
    case DragMode::CropTop: {
        const float visible = 1.0f - m_dragStartTransform.cropTop - m_dragStartTransform.cropBottom;
        const float delta = dy / std::max(m_dragStartTransform.height, MinSize) * visible;
        t.cropTop = std::clamp(m_dragStartTransform.cropTop + delta, 0.0f,
            1.0f - m_dragStartTransform.cropBottom - MinVisibleCrop);
        break;
    }
    case DragMode::CropBottom: {
        const float visible = 1.0f - m_dragStartTransform.cropTop - m_dragStartTransform.cropBottom;
        const float delta = -dy / std::max(m_dragStartTransform.height, MinSize) * visible;
        t.cropBottom = std::clamp(m_dragStartTransform.cropBottom + delta, 0.0f,
            1.0f - m_dragStartTransform.cropTop - MinVisibleCrop);
        break;
    }
    default:
        break;
    }

    t.x = Clamp01(t.x);
    t.y = Clamp01(t.y);
    t.width = std::clamp(t.width, MinSize, 1.0f - t.x);
    t.height = std::clamp(t.height, MinSize, 1.0f - t.y);
    m_selected->Transform() = t;
}

void App::EndPointerDrag() {
    if (GetCapture() == m_outputWindow) {
        ReleaseCapture();
    }
    m_dragMode = DragMode::None;
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
        case IdAdd: AddSourceAsync(); return 0;
        case IdClear:
            for (auto& source : m_sources) if (source) source->Stop();
            m_sources.clear();
            m_selected.reset();
            UpdateStatusText();
            return 0;
        case IdEdit: SetEditMode(!m_editMode); return 0;
        case IdCrop: SetCropMode(!m_cropMode); return 0;
        case IdAuto:
            if (m_renderer->AutoPerformance()) m_renderer->SetFixedFps(30);
            else m_renderer->SetAutoPerformance(true);
            UpdateRenderTimer();
            return 0;
        case IdGrid: ApplyGridLayout(); return 0;
        case IdSide: ApplySideBySideLayout(); return 0;
        case IdPip: ApplyPictureInPictureLayout(); return 0;
        case IdResetCrop:
            if (m_selected) m_selected->ResetCrop();
            return 0;
        case IdVisibility: ToggleSelectedVisibility(); return 0;
        case IdBack: SendSelectedToBack(); return 0;
        case IdFront: BringSelectedToFront(); return 0;
        case IdRemove: RemoveSelected(); return 0;
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
            const auto width = std::max(static_cast<uint32_t>(LOWORD(lParam)), 1u);
            const auto height = std::max(static_cast<uint32_t>(HIWORD(lParam)), 1u);
            m_renderer->Resize(width, height);
        }
        return 0;
    case WM_TIMER:
        if (wParam == RenderTimerId && m_renderer) {
            PruneClosedSources();
            m_renderer->Render(m_sources, m_selected.get(), m_editMode, m_cropMode);
            UpdateRenderTimer();
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        BeginPointerDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON) {
            UpdatePointerDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;
    case WM_LBUTTONUP:
        EndPointerDrag();
        return 0;
    case WM_CAPTURECHANGED:
        m_dragMode = DragMode::None;
        return 0;
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_DELETE: RemoveSelected(); return 0;
        case 'H': ToggleSelectedVisibility(); return 0;
        case 'C': SetCropMode(!m_cropMode); return 0;
        case 'G': ApplyGridLayout(); return 0;
        case 'P': ApplyPictureInPictureLayout(); return 0;
        case VK_ESCAPE:
            m_selected.reset();
            SetCropMode(false);
            UpdateStatusText();
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
