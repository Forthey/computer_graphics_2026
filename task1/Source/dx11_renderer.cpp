#include "framework.h"
#include "dx11_renderer.h"

#include <cassert>
#include <cwchar>
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <dxgi.h>

namespace
{
template <typename T>
void SafeRelease(T*& ptr)
{
    if (ptr != nullptr)
    {
        ptr->Release();
        ptr = nullptr;
    }
}
} // namespace

Dx11Renderer::~Dx11Renderer()
{
    Shutdown();
}

bool Dx11Renderer::Initialize(HWND windowHandle)
{
    Shutdown();

    RECT clientRect{};
    GetClientRect(windowHandle, &clientRect);
    m_width = static_cast<UINT>(clientRect.right - clientRect.left);
    m_height = static_cast<UINT>(clientRect.bottom - clientRect.top);

    IDXGIFactory* factory = nullptr;
    HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&factory));
    if (FAILED(hr))
    {
        return false;
    }

    IDXGIAdapter* selectedAdapter = nullptr;
    IDXGIAdapter* adapter = nullptr;
    UINT adapterIndex = 0;

    while (SUCCEEDED(factory->EnumAdapters(adapterIndex, &adapter)))
    {
        DXGI_ADAPTER_DESC desc{};
        adapter->GetDesc(&desc);

        if (std::wcscmp(desc.Description, L"Microsoft Basic Render Driver") != 0)
        {
            selectedAdapter = adapter;
            break;
        }

        SafeRelease(adapter);
        ++adapterIndex;
    }

    if (selectedAdapter == nullptr)
    {
        SafeRelease(factory);
        return false;
    }

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    hr = D3D11CreateDevice(
        selectedAdapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        flags,
        levels,
        1,
        D3D11_SDK_VERSION,
        &m_device,
        &featureLevel,
        &m_context);
    SafeRelease(selectedAdapter);

    if (FAILED(hr) || featureLevel != D3D_FEATURE_LEVEL_11_0)
    {
        SafeRelease(factory);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = m_width;
    swapChainDesc.BufferDesc.Height = m_height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = windowHandle;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = 0;

    hr = factory->CreateSwapChain(m_device, &swapChainDesc, &m_swapChain);
    SafeRelease(factory);
    if (FAILED(hr))
    {
        Shutdown();
        return false;
    }

    if (!CreateBackBufferRTV())
    {
        Shutdown();
        return false;
    }

    return true;
}

void Dx11Renderer::RenderFrame()
{
    if (m_context == nullptr || m_backBufferRTV == nullptr || m_swapChain == nullptr)
    {
        return;
    }

    ID3D11RenderTargetView* views[] = { m_backBufferRTV };
    m_context->OMSetRenderTargets(1, views, nullptr);

    static const FLOAT clearColor[4] = { 0.12f, 0.18f, 0.24f, 1.0f };
    m_context->ClearRenderTargetView(m_backBufferRTV, clearColor);

    const HRESULT hr = m_swapChain->Present(0, 0);
    assert(SUCCEEDED(hr));
}

bool Dx11Renderer::Resize(UINT width, UINT height)
{
    if (m_swapChain == nullptr || width == 0 || height == 0)
    {
        return true;
    }

    ReleaseRenderTargets();

    const HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
    {
        return false;
    }

    m_width = width;
    m_height = height;
    return CreateBackBufferRTV();
}

void Dx11Renderer::Shutdown()
{
    ReleaseRenderTargets();
    SafeRelease(m_swapChain);

    if (m_context != nullptr)
    {
        m_context->ClearState();
        m_context->Flush();
    }

    SafeRelease(m_context);

#ifdef _DEBUG
    if (m_device != nullptr)
    {
        ID3D11Debug* d3dDebug = nullptr;
        if (SUCCEEDED(m_device->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&d3dDebug))))
        {
            d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
            SafeRelease(d3dDebug);
        }
    }
#endif

    SafeRelease(m_device);

    m_width = 0;
    m_height = 0;
}

bool Dx11Renderer::CreateBackBufferRTV()
{
    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    if (FAILED(hr))
    {
        return false;
    }

    hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_backBufferRTV);
    SafeRelease(backBuffer);

    return SUCCEEDED(hr);
}

void Dx11Renderer::ReleaseRenderTargets()
{
    if (m_context != nullptr)
    {
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    SafeRelease(m_backBufferRTV);
}
