#pragma once

#include <d3d11_4.h>
#include <d2d1_3.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

class GraphicsDevice {
public:
    GraphicsDevice();

    ID3D11Device* D3DDevice() const noexcept { return m_d3dDevice.Get(); }
    ID3D11DeviceContext* D3DContext() const noexcept { return m_d3dContext.Get(); }
    IDXGIDevice* DxgiDevice() const noexcept { return m_dxgiDevice.Get(); }
    ID2D1Factory3* D2DFactory() const noexcept { return m_d2dFactory.Get(); }
    ID2D1Device2* D2DDevice() const noexcept { return m_d2dDevice.Get(); }
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice WinRtDevice() const noexcept { return m_winRtDevice; }

    bool UsingWarp() const noexcept { return m_usingWarp; }
    unsigned long long DedicatedVideoMemory() const noexcept { return m_dedicatedVideoMemory; }

private:
    void CreateD3DDevice();
    void CreateD2DDevice();
    void ReadAdapterInfo();

    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dContext;
    Microsoft::WRL::ComPtr<IDXGIDevice> m_dxgiDevice;
    Microsoft::WRL::ComPtr<ID2D1Factory3> m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device2> m_d2dDevice;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_winRtDevice{ nullptr };
    bool m_usingWarp{ false };
    unsigned long long m_dedicatedVideoMemory{ 0 };
};
