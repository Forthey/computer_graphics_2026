#pragma once

#include <windows.h>

class Dx11Renderer
{
public:
    Dx11Renderer() = default;
    ~Dx11Renderer();

    Dx11Renderer(const Dx11Renderer&) = delete;
    Dx11Renderer& operator=(const Dx11Renderer&) = delete;

    bool Initialize(HWND windowHandle);
    void RenderFrame();
    bool Resize(UINT width, UINT height);
    void Shutdown();

private:
    bool CreateBackBufferRTV();
    void ReleaseRenderTargets();

    struct ID3D11Device* m_device = nullptr;
    struct ID3D11DeviceContext* m_context = nullptr;
    struct IDXGISwapChain* m_swapChain = nullptr;
    struct ID3D11RenderTargetView* m_backBufferRTV = nullptr;
    UINT m_width = 0;
    UINT m_height = 0;
};
