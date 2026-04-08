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

#include "Camera.h"
#include "ObjectInterfaces/AutoRotatable.h"

using Microsoft::WRL::ComPtr;

class CubeRenderItem;
class RenderItem;

class Dx11Renderer {
public:
    enum class PostProcessMode : std::uint32_t {
        Original = 0u,
        Grayscale = 1u,
        Sepia = 2u,
    };

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
    void cyclePostProcessMode();
    const wchar_t* postProcessModeName() const;
    void shutdown();

private:
    struct RenderAssets {
        struct ShaderPass {
            ComPtr<ID3D11VertexShader> vertexShader;
            ComPtr<ID3D11PixelShader> pixelShader;
            ComPtr<ID3D11InputLayout> inputLayout;
            ComPtr<ID3D11DepthStencilState> depthStencilState;
        };

        struct TextureBinding {
            ComPtr<ID3D11SamplerState> samplerState;
            ComPtr<ID3D11Texture2D> texture;
            ComPtr<ID3D11ShaderResourceView> textureView;
        };

        ShaderPass objectPass;
        ShaderPass skyboxPass;
        ShaderPass postProcessPass;
        TextureBinding cubeTexture;
        TextureBinding cubeNormalTexture;
        TextureBinding skyboxTexture;
        TextureBinding postProcessTexture;
        ComPtr<ID3D11RasterizerState> rasterizerState;
        ComPtr<ID3D11BlendState> transparentBlendState;
        ComPtr<ID3D11DepthStencilState> transparentDepthState;
        ComPtr<ID3D11Buffer> objectBuffer;
        ComPtr<ID3D11Buffer> sceneBuffer;
        ComPtr<ID3D11Buffer> opaqueInstanceBuffer;
        ComPtr<ID3D11Buffer> postProcessBuffer;
    };

    bool createBackBufferTarget();
    bool createDepthResources();
    bool createSceneColorResources();
    bool createPipelineResources();
    bool createTextureResources();
    bool buildScene();
    void releaseRenderTargets();
    void releaseSceneResources();

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_backBufferTarget;
    ComPtr<ID3D11RenderTargetView> m_sceneColorTarget;
    ComPtr<ID3D11Texture2D> m_depthBuffer;
    ComPtr<ID3D11DepthStencilView> m_depthTarget;
    RenderAssets m_renderAssets;
    std::vector<std::shared_ptr<RenderItem>> m_renderItems;
    std::vector<std::shared_ptr<CubeRenderItem>> m_opaqueCubeItems;
    std::vector<std::shared_ptr<AutoRotatable>> m_autoRotatables;
    Camera m_camera;
    std::chrono::steady_clock::time_point m_lastFrameTime = {};
    bool m_hasLastFrameTime = false;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    PostProcessMode m_postProcessMode = PostProcessMode::Sepia;
};


