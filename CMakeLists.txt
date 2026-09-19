cmake_minimum_required(VERSION 3.25)
project(RedsStreamCanvas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "Red's Stream Canvas only builds for Windows (Win32 + D3D11 + WinRT).")
endif()

# Self-contained releases: static MSVC runtime so users don't need the
# Visual C++ Redistributable installed separately.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_executable(RedsStreamCanvas WIN32
    src/main.cpp
    src/App.cpp
    src/Graphics/GraphicsDevice.cpp
    src/Graphics/Renderer.cpp
    src/Capture/CaptureSource.cpp
    src/Capture/CaptureManager.cpp
    src/Canvas/CanvasModel.cpp
    src/UI/ControlWindow.cpp
    src/UI/OutputWindow.cpp
    src/app.manifest
)

target_include_directories(RedsStreamCanvas PRIVATE src)

target_compile_definitions(RedsStreamCanvas PRIVATE
    UNICODE _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(RedsStreamCanvas PRIVATE /W4 /permissive- /EHsc /await:strict)
endif()

target_link_libraries(RedsStreamCanvas PRIVATE
    d3d11
    dxgi
    d3dcompiler
    windowsapp
    shcore
    runtimeobject
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RedsStreamCanvas)
