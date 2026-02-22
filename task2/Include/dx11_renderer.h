#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <cstdint>

using Microsoft::WRL::ComPtr;

class Dx11Renderer {
public:
    Dx11Renderer() = default;
    ~Dx11Renderer();

    Dx11Renderer(const Dx11Renderer&) = delete;
    Dx11Renderer& operator=(const Dx11Renderer&) = delete;

    bool initialize(HWND window);
    void renderFrame();
    bool resize(std::uint32_t width, std::uint32_t height);
    void shutdown();

private:
    bool createBackBufferTarget();
    bool createTriangleResources();
    void releaseRenderTargets();
    void releaseTriangleResources();

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_backBufferTarget;
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_vertexLayout;
    ComPtr<ID3D11Buffer> m_triangleVertices;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
};
