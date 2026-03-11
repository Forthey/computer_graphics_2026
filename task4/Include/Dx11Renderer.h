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

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "ObjectInterfaces/AutoRotatable.h"
#include "Camera.h"

using Microsoft::WRL::ComPtr;

class RenderItem;

class Dx11Renderer {
public:
    Dx11Renderer() = default;
    ~Dx11Renderer();

    Dx11Renderer(const Dx11Renderer&) = delete;
    Dx11Renderer& operator=(const Dx11Renderer&) = delete;

    bool initialize(HWND window);
    void renderFrame();
    bool resize(std::uint32_t width, std::uint32_t height);
    void adjustCamera(float deltaDirection, float deltaTilt);
    void moveCamera(float forwardDelta, float rightDelta);
    void toggleSceneAutoRotation();
    void shutdown();

private:
    bool createBackBufferTarget();
    bool createDepthResources();
    bool createPipelineResources();
    bool buildScene();
    void releaseRenderTargets();
    void releaseSceneResources();

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_backBufferTarget;
    ComPtr<ID3D11Texture2D> m_depthBuffer;
    ComPtr<ID3D11DepthStencilView> m_depthTarget;
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_vertexLayout;
    ComPtr<ID3D11RasterizerState> m_rasterizerState;
    ComPtr<ID3D11Buffer> m_objectBuffer;
    ComPtr<ID3D11Buffer> m_sceneBuffer;
    std::vector<std::shared_ptr<RenderItem>> m_renderItems;
    std::vector<std::shared_ptr<AutoRotatable>> m_autoRotatables;
    Camera m_camera;
    std::chrono::steady_clock::time_point m_lastFrameTime = {};
    bool m_hasLastFrameTime = false;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
};
