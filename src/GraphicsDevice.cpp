#include "GraphicsDevice.h"

#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/base.h>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

GraphicsDevice::GraphicsDevice() {
    CreateD3DDevice();
    CreateD2DDevice();
    ReadAdapterInfo();
}

void GraphicsDevice::CreateD3DDevice() {
    constexpr UINT baseFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    constexpr UINT debugFlags = D3D11_CREATE_DEVICE_DEBUG;
#else
    constexpr UINT debugFlags = 0;
#endif

    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL createdLevel{};
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        baseFlags | debugFlags,
        levels,
        ARRAYSIZE(levels),
        D3D11_SDK_VERSION,
        &m_d3dDevice,
        &createdLevel,
        &m_d3dContext);

#if defined(_DEBUG)
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING) {
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            baseFlags,
            levels,
            ARRAYSIZE(levels),
            D3D11_SDK_VERSION,
            &m_d3dDevice,
            &createdLevel,
            &m_d3dContext);
    }
#endif

    if (FAILED(hr)) {
        m_usingWarp = true;
        winrt::check_hresult(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            baseFlags,
            levels,
            ARRAYSIZE(levels),
            D3D11_SDK_VERSION,
            &m_d3dDevice,
            &createdLevel,
            &m_d3dContext));
    }

    winrt::check_hresult(m_d3dDevice.As(&m_dxgiDevice));

    ComPtr<ID3D11Multithread> multithread;
    if (SUCCEEDED(m_d3dContext.As(&multithread))) {
        multithread->SetMultithreadProtected(TRUE);
    }

    winrt::com_ptr<IInspectable> inspectable;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(m_dxgiDevice.Get(), inspectable.put()));
    m_winRtDevice = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

void GraphicsDevice::CreateD2DDevice() {
    D2D1_FACTORY_OPTIONS options{};
#if defined(_DEBUG)
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    winrt::check_hresult(D2D1CreateFactory(
        D2D1_FACTORY_TYPE_MULTI_THREADED,
        __uuidof(ID2D1Factory3),
        &options,
        reinterpret_cast<void**>(m_d2dFactory.GetAddressOf())));

    ComPtr<ID2D1Device> baseDevice;
    winrt::check_hresult(m_d2dFactory->CreateDevice(m_dxgiDevice.Get(), &baseDevice));
    winrt::check_hresult(baseDevice.As(&m_d2dDevice));
}

void GraphicsDevice::ReadAdapterInfo() {
    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(m_dxgiDevice->GetAdapter(&adapter))) {
        return;
    }

    DXGI_ADAPTER_DESC desc{};
    if (SUCCEEDED(adapter->GetDesc(&desc))) {
        m_dedicatedVideoMemory = desc.DedicatedVideoMemory;
    }
}
