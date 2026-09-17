cmake_minimum_required(VERSION 3.25)
project(RedsStreamCanvas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(RedsStreamCanvas WIN32
    src/main.cpp
    src/App.cpp
    src/GraphicsDevice.cpp
    src/CaptureSource.cpp
    src/CanvasRenderer.cpp
    src/app.manifest
)

target_include_directories(RedsStreamCanvas PRIVATE src)
target_compile_definitions(RedsStreamCanvas PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(RedsStreamCanvas PRIVATE /W4 /permissive- /EHsc)

target_link_libraries(RedsStreamCanvas PRIVATE
    d3d11
    dxgi
    d2d1
    dwrite
    windowsapp
    shcore
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RedsStreamCanvas)
