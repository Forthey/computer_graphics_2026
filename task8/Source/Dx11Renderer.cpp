#include "Dx11Renderer.h"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cwchar>
#include <memory>
#include <vector>

#include "RenderItem.h"
#include "SceneObjects/CubeRenderItem.h"
#include "SceneObjects/SkyboxRenderItem.h"
#include "TextureLoader.h"
#include "framework.h"

namespace {
constexpr std::uint32_t kSwapChainBufferCount = 2;
constexpr std::uint32_t kSampleCount = 1;
constexpr std::uint32_t kMaxPointLights = 10;
constexpr std::uint32_t kMaxOpaqueInstances = 64;
constexpr std::uint32_t kComputeCullThreadGroupSize = 64;
constexpr std::uint32_t kCubePrimitiveCount = 12;
constexpr float kClearColor[4] = {0.02f, 0.04f, 0.08f, 1.0f};
constexpr float kViewportMinDepth = 0.0f;
constexpr float kViewportMaxDepth = 1.0f;
constexpr float kClearDepthValue = 1.0f;

struct PointLight {
    DirectX::XMFLOAT4 position;
    DirectX::XMFLOAT4 color;
};

struct ObjectBuffer {
    DirectX::XMFLOAT4X4 modelMatrix;
    DirectX::XMFLOAT4X4 normalMatrix;
    DirectX::XMFLOAT4 colorTint;
    DirectX::XMFLOAT4 materialParams;
};

struct SceneBuffer {
    DirectX::XMFLOAT4X4 viewProjectionMatrix;
    DirectX::XMFLOAT4 cameraPosition;
    DirectX::XMUINT4 lightCount;
    PointLight lights[kMaxPointLights];
    DirectX::XMFLOAT4 ambientColor;
    DirectX::XMFLOAT4 frustum[6];
};

struct OpaqueInstanceData {
    DirectX::XMFLOAT4X4 modelMatrix;
    DirectX::XMFLOAT4X4 normalMatrix;
    DirectX::XMFLOAT4 colorTint;
    DirectX::XMFLOAT4 materialParams;
};

struct PostProcessBuffer {
    DirectX::XMUINT4 mode;
};

struct CullParamsBuffer {
    DirectX::XMUINT4 numShapes;
    DirectX::XMFLOAT4 bbMin[kMaxOpaqueInstances];
    DirectX::XMFLOAT4 bbMax[kMaxOpaqueInstances];
};

constexpr std::uint32_t divideRoundUp(std::uint32_t value, std::uint32_t divisor) {
    return (value + divisor - 1u) / divisor;
}

DirectX::XMMATRIX buildNormalMatrix(const DirectX::XMMATRIX& modelMatrix) {
    return DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, modelMatrix));
}

DirectX::BoundingFrustum buildWorldFrustum(const DirectX::XMMATRIX& viewMatrix,
                                           const DirectX::XMMATRIX& projectionMatrix) {
    DirectX::BoundingFrustum viewFrustum;
    DirectX::BoundingFrustum::CreateFromMatrix(viewFrustum, projectionMatrix);

    DirectX::BoundingFrustum worldFrustum;
    viewFrustum.Transform(worldFrustum, DirectX::XMMatrixInverse(nullptr, viewMatrix));
    return worldFrustum;
}

void fillSceneLighting(SceneBuffer& sceneBuffer) {
    sceneBuffer.lightCount = DirectX::XMUINT4{5u, 0u, 0u, 0u};
    sceneBuffer.ambientColor = DirectX::XMFLOAT4{0.07f, 0.07f, 0.09f, 1.0f};
    sceneBuffer.lights[0] = {DirectX::XMFLOAT4{-4.8f, 1.25f, 3.9f, 1.0f}, DirectX::XMFLOAT4{1.0f, 0.90f, 0.82f, 1.0f}};
    sceneBuffer.lights[1] = {DirectX::XMFLOAT4{4.8f, 1.25f, 3.9f, 1.0f}, DirectX::XMFLOAT4{0.82f, 0.90f, 1.0f, 1.0f}};
    sceneBuffer.lights[2] = {DirectX::XMFLOAT4{-4.8f, 1.25f, 11.75f, 1.0f},
                             DirectX::XMFLOAT4{0.88f, 1.0f, 0.84f, 1.0f}};
    sceneBuffer.lights[3] = {DirectX::XMFLOAT4{4.8f, 1.25f, 11.75f, 1.0f}, DirectX::XMFLOAT4{1.0f, 0.82f, 0.90f, 1.0f}};
    sceneBuffer.lights[4] = {DirectX::XMFLOAT4{0.0f, 1.85f, 8.9f, 1.0f}, DirectX::XMFLOAT4{1.0f, 0.98f, 0.84f, 1.0f}};

    for (std::uint32_t lightIndex = 5; lightIndex < kMaxPointLights; ++lightIndex) {
        sceneBuffer.lights[lightIndex] = {};
    }

    for (DirectX::XMFLOAT4& plane : sceneBuffer.frustum) {
        plane = {};
    }
}

HRESULT compileShaderFromFile(const wchar_t* shaderPath, const char* entryPoint, const char* target,
                              ComPtr<ID3DBlob>& compiledCode) {
    std::uint32_t compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT lastError = E_FAIL;
    for (int candidateIndex = 0; candidateIndex < 2; ++candidateIndex) {
        wchar_t fullPath[MAX_PATH]{};
        if (candidateIndex == 0) {
            wcscpy_s(fullPath, shaderPath);
        } else {
            swprintf_s(fullPath, L"..\\..\\%s", shaderPath);
        }

        ComPtr<ID3DBlob> errorMessages;
        const HRESULT result = D3DCompileFromFile(fullPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint,
                                                  target, static_cast<unsigned int>(compileFlags), 0,
                                                  compiledCode.ReleaseAndGetAddressOf(), errorMessages.GetAddressOf());
        if (SUCCEEDED(result)) {
            return result;
        }

        if (errorMessages) {
            OutputDebugStringA(static_cast<const char*>(errorMessages->GetBufferPointer()));
        }

        lastError = result;
    }

    return lastError;
}

bool isBlockCompressedFormat(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_BC1_UNORM || format == DXGI_FORMAT_BC2_UNORM || format == DXGI_FORMAT_BC3_UNORM;
}

void reportTextureCreationFailure(const TextureDescription& textureDescription, HRESULT result) {
    wchar_t message[256]{};
    swprintf_s(message, L"CreateTexture2D failed: hr=0x%08X format=%u size=%ux%u mips=%u\n",
               static_cast<unsigned int>(result), static_cast<unsigned int>(textureDescription.fmt),
               textureDescription.width, textureDescription.height, textureDescription.mipmapsCount);
    OutputDebugStringW(message);
}

bool createTexture2DResource(ID3D11Device* device, const TextureDescription& textureDescription,
                             ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11ShaderResourceView>& textureView) {
    if (device == nullptr || textureDescription.width == 0u || textureDescription.height == 0u ||
        textureDescription.mipmapsCount == 0u ||
        textureDescription.subresources.size() != textureDescription.mipmapsCount || textureDescription.data.empty()) {
        return false;
    }

    if (isBlockCompressedFormat(textureDescription.fmt) &&
        ((textureDescription.width % 4u) != 0u || (textureDescription.height % 4u) != 0u)) {
        OutputDebugStringW(L"BC-compressed textures require width and height to be multiples of 4.\n");
        return false;
    }

    std::vector<D3D11_SUBRESOURCE_DATA> subresources(textureDescription.mipmapsCount);
    for (std::uint32_t mipIndex = 0; mipIndex < textureDescription.mipmapsCount; ++mipIndex) {
        const TextureSubresourceLayout& layout = textureDescription.subresources[mipIndex];
        if (layout.rowPitch == 0u || layout.slicePitch == 0u ||
            layout.dataOffset + layout.slicePitch > textureDescription.data.size()) {
            return false;
        }

        subresources[mipIndex].pSysMem = textureDescription.data.data() + layout.dataOffset;
        subresources[mipIndex].SysMemPitch = layout.rowPitch;
        subresources[mipIndex].SysMemSlicePitch = layout.slicePitch;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = textureDescription.width;
    desc.Height = textureDescription.height;
    desc.MipLevels = textureDescription.mipmapsCount;
    desc.ArraySize = 1;
    desc.Format = textureDescription.fmt;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    HRESULT result = device->CreateTexture2D(&desc, subresources.data(), texture.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        reportTextureCreationFailure(textureDescription, result);
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
    viewDesc.Format = desc.Format;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MipLevels = textureDescription.mipmapsCount;
    viewDesc.Texture2D.MostDetailedMip = 0;
    result = device->CreateShaderResourceView(texture.Get(), &viewDesc, textureView.ReleaseAndGetAddressOf());
    return SUCCEEDED(result);
}

bool createTexture2DArrayResource(ID3D11Device* device, const std::vector<TextureDescription>& textureDescriptions,
                                  ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11ShaderResourceView>& textureView) {
    if (device == nullptr || textureDescriptions.empty()) {
        return false;
    }

    const TextureDescription& reference = textureDescriptions.front();
    if (reference.width == 0u || reference.height == 0u || reference.mipmapsCount == 0u ||
        reference.subresources.size() != reference.mipmapsCount || reference.data.empty()) {
        return false;
    }

    if (isBlockCompressedFormat(reference.fmt) && ((reference.width % 4u) != 0u || (reference.height % 4u) != 0u)) {
        OutputDebugStringW(L"BC-compressed texture arrays require width and height to be multiples of 4.\n");
        return false;
    }

    std::vector<D3D11_SUBRESOURCE_DATA> subresources;
    subresources.reserve(textureDescriptions.size() * reference.mipmapsCount);

    for (const TextureDescription& textureDescription : textureDescriptions) {
        if (textureDescription.fmt != reference.fmt || textureDescription.width != reference.width ||
            textureDescription.height != reference.height ||
            textureDescription.mipmapsCount != reference.mipmapsCount ||
            textureDescription.subresources.size() != reference.mipmapsCount || textureDescription.data.empty()) {
            return false;
        }

        for (std::uint32_t mipIndex = 0; mipIndex < reference.mipmapsCount; ++mipIndex) {
            const TextureSubresourceLayout& referenceLayout = reference.subresources[mipIndex];
            const TextureSubresourceLayout& layout = textureDescription.subresources[mipIndex];
            if (layout.rowPitch != referenceLayout.rowPitch || layout.slicePitch != referenceLayout.slicePitch ||
                layout.dataOffset + layout.slicePitch > textureDescription.data.size()) {
                return false;
            }

            D3D11_SUBRESOURCE_DATA subresource{};
            subresource.pSysMem = textureDescription.data.data() + layout.dataOffset;
            subresource.SysMemPitch = layout.rowPitch;
            subresource.SysMemSlicePitch = layout.slicePitch;
            subresources.push_back(subresource);
        }
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = reference.width;
    desc.Height = reference.height;
    desc.MipLevels = reference.mipmapsCount;
    desc.ArraySize = static_cast<UINT>(textureDescriptions.size());
    desc.Format = reference.fmt;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    HRESULT result = device->CreateTexture2D(&desc, subresources.data(), texture.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        reportTextureCreationFailure(reference, result);
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
    viewDesc.Format = desc.Format;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    viewDesc.Texture2DArray.ArraySize = desc.ArraySize;
    viewDesc.Texture2DArray.FirstArraySlice = 0;
    viewDesc.Texture2DArray.MipLevels = desc.MipLevels;
    viewDesc.Texture2DArray.MostDetailedMip = 0;
    result = device->CreateShaderResourceView(texture.Get(), &viewDesc, textureView.ReleaseAndGetAddressOf());
    return SUCCEEDED(result);
}

bool createFlatNormalTextureResource(ID3D11Device* device, ComPtr<ID3D11Texture2D>& texture,
                                     ComPtr<ID3D11ShaderResourceView>& textureView) {
    TextureDescription textureDescription{};
    textureDescription.fmt = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDescription.width = 1u;
    textureDescription.height = 1u;
    textureDescription.mipmapsCount = 1u;
    textureDescription.subresources.push_back(TextureSubresourceLayout{1u, 1u, 4u, 4u, 0u});
    textureDescription.data = {
        std::byte{128},
        std::byte{128},
        std::byte{255},
        std::byte{255},
    };

    return createTexture2DResource(device, textureDescription, texture, textureView);
}

bool createCubemapResource(ID3D11Device* device, const std::array<TextureDescription, 6>& faceTextures,
                           ComPtr<ID3D11Texture2D>& cubemapTexture, ComPtr<ID3D11ShaderResourceView>& cubemapView) {
    if (device == nullptr) {
        return false;
    }

    const DXGI_FORMAT format = faceTextures[0].fmt;
    const std::uint32_t width = faceTextures[0].width;
    const std::uint32_t height = faceTextures[0].height;
    const std::uint32_t mipmapsCount = faceTextures[0].mipmapsCount;
    if (width == 0u || height == 0u || mipmapsCount == 0u || faceTextures[0].subresources.size() != mipmapsCount) {
        return false;
    }

    if (isBlockCompressedFormat(format) && ((width % 4u) != 0u || (height % 4u) != 0u)) {
        OutputDebugStringW(L"BC-compressed cubemap faces require width and height to be multiples of 4.\n");
        return false;
    }

    for (const TextureDescription& faceTexture : faceTextures) {
        if (faceTexture.fmt != format || faceTexture.width != width || faceTexture.height != height ||
            faceTexture.mipmapsCount != mipmapsCount || faceTexture.subresources.size() != mipmapsCount) {
            return false;
        }

        for (std::uint32_t mipIndex = 0; mipIndex < mipmapsCount; ++mipIndex) {
            const TextureSubresourceLayout& referenceLayout = faceTextures[0].subresources[mipIndex];
            const TextureSubresourceLayout& layout = faceTexture.subresources[mipIndex];
            if (layout.rowPitch != referenceLayout.rowPitch || layout.slicePitch != referenceLayout.slicePitch ||
                layout.dataOffset + layout.slicePitch > faceTexture.data.size()) {
                return false;
            }
        }
    }

    std::vector<D3D11_SUBRESOURCE_DATA> subresources(static_cast<std::size_t>(faceTextures.size()) * mipmapsCount);
    for (std::size_t faceIndex = 0; faceIndex < faceTextures.size(); ++faceIndex) {
        for (std::uint32_t mipIndex = 0; mipIndex < mipmapsCount; ++mipIndex) {
            const TextureSubresourceLayout& layout = faceTextures[faceIndex].subresources[mipIndex];
            const std::size_t subresourceIndex =
                static_cast<std::size_t>(D3D11CalcSubresource(mipIndex, static_cast<UINT>(faceIndex), mipmapsCount));
            subresources[subresourceIndex].pSysMem = faceTextures[faceIndex].data.data() + layout.dataOffset;
            subresources[subresourceIndex].SysMemPitch = layout.rowPitch;
            subresources[subresourceIndex].SysMemSlicePitch = layout.slicePitch;
        }
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = mipmapsCount;
    desc.ArraySize = static_cast<UINT>(faceTextures.size());
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    HRESULT result = device->CreateTexture2D(&desc, subresources.data(), cubemapTexture.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
    viewDesc.Format = format;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    viewDesc.TextureCube.MipLevels = mipmapsCount;
    viewDesc.TextureCube.MostDetailedMip = 0;
    result = device->CreateShaderResourceView(cubemapTexture.Get(), &viewDesc, cubemapView.ReleaseAndGetAddressOf());
    return SUCCEEDED(result);
}
}  // namespace

Dx11Renderer::~Dx11Renderer() { shutdown(); }

bool Dx11Renderer::initialize(HWND window) {
    shutdown();

    RECT clientArea{};
    GetClientRect(window, &clientArea);
    m_width = static_cast<std::uint32_t>(clientArea.right - clientArea.left);
    m_height = static_cast<std::uint32_t>(clientArea.bottom - clientArea.top);

    ComPtr<IDXGIFactory> factory;
    HRESULT result = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(result)) {
        return false;
    }

    ComPtr<IDXGIAdapter> selectedAdapter;
    std::uint32_t adapterIndex = 0;

    ComPtr<IDXGIAdapter> adapter;
    while (SUCCEEDED(factory->EnumAdapters(adapterIndex, adapter.ReleaseAndGetAddressOf()))) {
        DXGI_ADAPTER_DESC adapterInfo{};
        adapter->GetDesc(&adapterInfo);

        if (std::wcscmp(adapterInfo.Description, L"Microsoft Basic Render Driver") != 0) {
            selectedAdapter = adapter;
            break;
        }

        ++adapterIndex;
    }

    if (!selectedAdapter) {
        return false;
    }

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};

    std::uint32_t flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    result = D3D11CreateDevice(selectedAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                               static_cast<unsigned int>(flags), levels, 1, D3D11_SDK_VERSION, m_device.GetAddressOf(),
                               &featureLevel, m_context.GetAddressOf());

    if (FAILED(result) || featureLevel != D3D_FEATURE_LEVEL_11_0) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC swapChainSetup{};
    swapChainSetup.BufferCount = kSwapChainBufferCount;
    swapChainSetup.BufferDesc.Width = m_width;
    swapChainSetup.BufferDesc.Height = m_height;
    swapChainSetup.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainSetup.BufferDesc.RefreshRate.Numerator = 0;
    swapChainSetup.BufferDesc.RefreshRate.Denominator = 1;
    swapChainSetup.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainSetup.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainSetup.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainSetup.OutputWindow = window;
    swapChainSetup.SampleDesc.Count = kSampleCount;
    swapChainSetup.SampleDesc.Quality = 0;
    swapChainSetup.Windowed = true;
    swapChainSetup.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainSetup.Flags = 0;

    result = factory->CreateSwapChain(m_device.Get(), &swapChainSetup, m_swapChain.GetAddressOf());
    if (FAILED(result)) {
        shutdown();
        return false;
    }

    if (!createBackBufferTarget() || !createDepthResources() || !createSceneColorResources() ||
        !createPipelineResources() || !buildScene()) {
        shutdown();
        return false;
    }

    m_lastFrameTime = std::chrono::steady_clock::now();
    m_hasLastFrameTime = true;
    return true;
}

void Dx11Renderer::renderFrame() {
    if (!m_context || !m_backBufferTarget || !m_sceneColorTarget || !m_depthTarget || !m_swapChain ||
        !m_renderAssets.postProcessTexture.textureView) {
        return;
    }

    ID3D11RenderTargetView* sceneTargets[] = {m_sceneColorTarget.Get()};
    m_context->OMSetRenderTargets(1, sceneTargets, m_depthTarget.Get());

    m_context->ClearRenderTargetView(m_sceneColorTarget.Get(), kClearColor);
    m_context->ClearDepthStencilView(m_depthTarget.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, kClearDepthValue, 0);

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = kViewportMinDepth;
    viewport.MaxDepth = kViewportMaxDepth;
    m_context->RSSetViewports(1, &viewport);
    m_context->RSSetState(m_renderAssets.rasterizerState.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> deltaTime = std::chrono::duration<float>::zero();
    if (!m_hasLastFrameTime) {
        m_lastFrameTime = now;
        m_hasLastFrameTime = true;
    } else {
        deltaTime = now - m_lastFrameTime;
        m_lastFrameTime = now;
    }

    for (const std::shared_ptr<AutoRotatable>& autoRotatable : m_autoRotatables) {
        if (autoRotatable) {
            autoRotatable->updateRotation(deltaTime);
        }
    }

    const DirectX::XMMATRIX viewMatrix = m_camera.buildViewMatrix();
    const float aspect = (m_height != 0) ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    const DirectX::XMMATRIX projectionMatrix = m_camera.buildProjectionMatrix(aspect);
    const DirectX::XMMATRIX viewProjectionMatrix = DirectX::XMMatrixMultiply(viewMatrix, projectionMatrix);
    const DirectX::BoundingFrustum worldFrustum = buildWorldFrustum(viewMatrix, projectionMatrix);
    const DirectX::XMFLOAT3 cameraPosition = m_camera.position();

    DirectX::XMVECTOR nearPlane{};
    DirectX::XMVECTOR farPlane{};
    DirectX::XMVECTOR rightPlane{};
    DirectX::XMVECTOR leftPlane{};
    DirectX::XMVECTOR topPlane{};
    DirectX::XMVECTOR bottomPlane{};
    worldFrustum.GetPlanes(&nearPlane, &farPlane, &rightPlane, &leftPlane, &topPlane, &bottomPlane);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT mapResult = m_context->Map(m_renderAssets.sceneBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    assert(SUCCEEDED(mapResult));
    if (SUCCEEDED(mapResult)) {
        auto* sceneBuffer = reinterpret_cast<SceneBuffer*>(mapped.pData);
        DirectX::XMStoreFloat4x4(&sceneBuffer->viewProjectionMatrix, viewProjectionMatrix);
        sceneBuffer->cameraPosition = DirectX::XMFLOAT4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f);
        fillSceneLighting(*sceneBuffer);
        DirectX::XMStoreFloat4(&sceneBuffer->frustum[0], DirectX::XMPlaneNormalize(nearPlane));
        DirectX::XMStoreFloat4(&sceneBuffer->frustum[1], DirectX::XMPlaneNormalize(farPlane));
        DirectX::XMStoreFloat4(&sceneBuffer->frustum[2], DirectX::XMPlaneNormalize(rightPlane));
        DirectX::XMStoreFloat4(&sceneBuffer->frustum[3], DirectX::XMPlaneNormalize(leftPlane));
        DirectX::XMStoreFloat4(&sceneBuffer->frustum[4], DirectX::XMPlaneNormalize(topPlane));
        DirectX::XMStoreFloat4(&sceneBuffer->frustum[5], DirectX::XMPlaneNormalize(bottomPlane));
        m_context->Unmap(m_renderAssets.sceneBuffer.Get(), 0);
    }

    ID3D11Buffer* constantBuffers[] = {m_renderAssets.objectBuffer.Get(), m_renderAssets.sceneBuffer.Get(),
                                       m_renderAssets.opaqueInstanceBuffer.Get()};
    m_context->VSSetConstantBuffers(0, 3, constantBuffers);
    m_context->PSSetConstantBuffers(0, 3, constantBuffers);

    std::vector<const RenderItem*> transparentItems;
    std::vector<const RenderItem*> skyboxItems;
    transparentItems.reserve(m_renderItems.size());
    skyboxItems.reserve(1);

    for (const std::shared_ptr<RenderItem>& renderItem : m_renderItems) {
        if (!renderItem) {
            continue;
        }

        const std::shared_ptr<Mesh>& mesh = renderItem->mesh();
        if (!mesh || !mesh->isValid()) {
            continue;
        }

        switch (renderItem->type()) {
            case RenderItemType::TransparentTextured:
                transparentItems.push_back(renderItem.get());
                break;
            case RenderItemType::Skybox:
                skyboxItems.push_back(renderItem.get());
                break;
            case RenderItemType::OpaqueTextured:
            default:
                break;
        }
    }

    const auto distanceSquaredToCamera = [&cameraPosition](const RenderItem* renderItem) {
        const DirectX::XMFLOAT3 position = renderItem->sortPosition();
        const float dx = position.x - cameraPosition.x;
        const float dy = position.y - cameraPosition.y;
        const float dz = position.z - cameraPosition.z;
        return dx * dx + dy * dy + dz * dz;
    };

    std::sort(transparentItems.begin(), transparentItems.end(),
              [&distanceSquaredToCamera](const RenderItem* lhs, const RenderItem* rhs) {
                  return distanceSquaredToCamera(lhs) > distanceSquaredToCamera(rhs);
              });

    const auto drawItem = [this](const RenderItem* renderItem) {
        const std::shared_ptr<Mesh>& mesh = renderItem->mesh();

        ID3D11SamplerState* sampler = nullptr;
        ID3D11ShaderResourceView* textureView = nullptr;
        switch (renderItem->type()) {
            case RenderItemType::Skybox:
                m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
                m_context->OMSetDepthStencilState(m_renderAssets.skyboxPass.depthStencilState.Get(), 0);
                m_context->IASetInputLayout(m_renderAssets.skyboxPass.inputLayout.Get());
                m_context->VSSetShader(m_renderAssets.skyboxPass.vertexShader.Get(), nullptr, 0);
                m_context->PSSetShader(m_renderAssets.skyboxPass.pixelShader.Get(), nullptr, 0);
                sampler = m_renderAssets.skyboxTexture.samplerState.Get();
                textureView = m_renderAssets.skyboxTexture.textureView.Get();
                break;
            case RenderItemType::TransparentTextured:
                m_context->OMSetBlendState(m_renderAssets.transparentBlendState.Get(), nullptr, 0xFFFFFFFF);
                m_context->OMSetDepthStencilState(m_renderAssets.transparentDepthState.Get(), 0);
                m_context->IASetInputLayout(m_renderAssets.objectPass.inputLayout.Get());
                m_context->VSSetShader(m_renderAssets.objectPass.vertexShader.Get(), nullptr, 0);
                m_context->PSSetShader(m_renderAssets.objectPass.pixelShader.Get(), nullptr, 0);
                sampler = m_renderAssets.cubeTexture.samplerState.Get();
                textureView = m_renderAssets.cubeTexture.textureView.Get();
                break;
            case RenderItemType::OpaqueTextured:
            default:
                m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
                m_context->OMSetDepthStencilState(nullptr, 0);
                m_context->IASetInputLayout(m_renderAssets.objectPass.inputLayout.Get());
                m_context->VSSetShader(m_renderAssets.objectPass.vertexShader.Get(), nullptr, 0);
                m_context->PSSetShader(m_renderAssets.objectPass.pixelShader.Get(), nullptr, 0);
                sampler = m_renderAssets.cubeTexture.samplerState.Get();
                textureView = m_renderAssets.cubeTexture.textureView.Get();
                break;
        }

        ID3D11SamplerState* samplers[] = {sampler};
        ID3D11ShaderResourceView* textureViews[] = {textureView, nullptr};
        if (renderItem->type() != RenderItemType::Skybox) {
            textureViews[1] = m_renderAssets.cubeNormalTexture.textureView.Get();
        }
        m_context->PSSetSamplers(0, 1, samplers);
        m_context->PSSetShaderResources(0, 2, textureViews);
        ID3D11ShaderResourceView* visibleIdsView[] = {m_renderAssets.visibleInstanceIdsView.Get()};
        m_context->VSSetShaderResources(0, 1, visibleIdsView);

        const DirectX::XMMATRIX modelMatrix = renderItem->buildModelMatrix();
        ObjectBuffer objectBuffer{};
        DirectX::XMStoreFloat4x4(&objectBuffer.modelMatrix, modelMatrix);
        DirectX::XMStoreFloat4x4(&objectBuffer.normalMatrix, buildNormalMatrix(modelMatrix));
        objectBuffer.colorTint = renderItem->colorTint();
        objectBuffer.materialParams =
            DirectX::XMFLOAT4{renderItem->shininess(), renderItem->useNormalMap() ? 1.0f : 0.0f, 0.0f, 0.0f};
        m_context->UpdateSubresource(m_renderAssets.objectBuffer.Get(), 0, nullptr, &objectBuffer, 0, 0);

        const std::uint32_t vertexStride = mesh->vertexStride();
        constexpr std::uint32_t startOffset = 0;
        ID3D11Buffer* geometry[] = {mesh->vertexBuffer()};
        m_context->IASetVertexBuffers(0, 1, geometry, &vertexStride, &startOffset);
        m_context->IASetIndexBuffer(mesh->indexBuffer(), mesh->indexFormat(), 0);
        m_context->DrawIndexed(mesh->indexCount(), 0, 0);
    };

    if (!m_opaqueCubeItems.empty()) {
        const std::uint32_t opaqueInstanceCount = static_cast<std::uint32_t>(
            (std::min)(m_opaqueCubeItems.size(), static_cast<std::size_t>(kMaxOpaqueInstances)));
        const std::shared_ptr<Mesh>& opaqueMesh = m_opaqueCubeItems.front()->mesh();
        if (opaqueMesh && opaqueMesh->isValid()) {
            D3D11_MAPPED_SUBRESOURCE instanceMap{};
            const HRESULT instanceMapResult =
                m_context->Map(m_renderAssets.opaqueInstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &instanceMap);
            assert(SUCCEEDED(instanceMapResult));
            if (SUCCEEDED(instanceMapResult)) {
                auto* instanceBuffer = reinterpret_cast<OpaqueInstanceData*>(instanceMap.pData);
                for (std::uint32_t instanceIndex = 0; instanceIndex < opaqueInstanceCount; ++instanceIndex) {
                    const CubeRenderItem& renderItem = *m_opaqueCubeItems[instanceIndex];
                    const DirectX::XMMATRIX modelMatrix = renderItem.buildModelMatrix();
                    DirectX::XMStoreFloat4x4(&instanceBuffer[instanceIndex].modelMatrix, modelMatrix);
                    DirectX::XMStoreFloat4x4(&instanceBuffer[instanceIndex].normalMatrix,
                                             buildNormalMatrix(modelMatrix));
                    instanceBuffer[instanceIndex].colorTint = renderItem.colorTint();
                    instanceBuffer[instanceIndex].materialParams =
                        DirectX::XMFLOAT4{renderItem.shininess(), renderItem.useNormalMap() ? 1.0f : 0.0f,
                                          static_cast<float>(renderItem.textureIndex()), 0.0f};
                }
                m_context->Unmap(m_renderAssets.opaqueInstanceBuffer.Get(), 0);
            }

            ObjectBuffer opaqueObjectBuffer{};
            DirectX::XMStoreFloat4x4(&opaqueObjectBuffer.modelMatrix, DirectX::XMMatrixIdentity());
            DirectX::XMStoreFloat4x4(&opaqueObjectBuffer.normalMatrix, DirectX::XMMatrixIdentity());
            opaqueObjectBuffer.colorTint = DirectX::XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f};
            opaqueObjectBuffer.materialParams = DirectX::XMFLOAT4{0.0f, 0.0f, 0.0f, 1.0f};
            m_context->UpdateSubresource(m_renderAssets.objectBuffer.Get(), 0, nullptr, &opaqueObjectBuffer, 0, 0);

            D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS indirectArgs{};
            indirectArgs.IndexCountPerInstance = opaqueMesh->indexCount();
            indirectArgs.InstanceCount = 0u;
            indirectArgs.StartIndexLocation = 0u;
            indirectArgs.BaseVertexLocation = 0;
            indirectArgs.StartInstanceLocation = 0u;
            m_context->UpdateSubresource(m_renderAssets.indirectArgsSrcBuffer.Get(), 0, nullptr, &indirectArgs, 0, 0);

            ID3D11Buffer* computeConstantBuffers[] = {m_renderAssets.sceneBuffer.Get(),
                                                      m_renderAssets.cullParamsBuffer.Get()};
            m_context->CSSetConstantBuffers(0, 2, computeConstantBuffers);
            ID3D11UnorderedAccessView* computeUavs[] = {m_renderAssets.indirectArgsUav.Get(),
                                                        m_renderAssets.visibleInstanceIdsUav.Get()};
            m_context->CSSetUnorderedAccessViews(0, 2, computeUavs, nullptr);
            m_context->CSSetShader(m_renderAssets.cullShader.Get(), nullptr, 0);
            m_context->Dispatch(divideRoundUp(opaqueInstanceCount, kComputeCullThreadGroupSize), 1, 1);
            m_context->CopyResource(m_renderAssets.indirectArgsBuffer.Get(),
                                    m_renderAssets.indirectArgsSrcBuffer.Get());

            ID3D11UnorderedAccessView* nullComputeUavs[] = {nullptr, nullptr};
            ID3D11Buffer* nullComputeConstantBuffers[] = {nullptr, nullptr};
            m_context->CSSetUnorderedAccessViews(0, 2, nullComputeUavs, nullptr);
            m_context->CSSetConstantBuffers(0, 2, nullComputeConstantBuffers);
            m_context->CSSetShader(nullptr, nullptr, 0);

            m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
            m_context->OMSetDepthStencilState(nullptr, 0);
            m_context->IASetInputLayout(m_renderAssets.objectPass.inputLayout.Get());
            m_context->VSSetShader(m_renderAssets.objectPass.vertexShader.Get(), nullptr, 0);
            m_context->PSSetShader(m_renderAssets.objectPass.pixelShader.Get(), nullptr, 0);

            ID3D11SamplerState* samplers[] = {m_renderAssets.cubeTexture.samplerState.Get()};
            ID3D11ShaderResourceView* textureViews[] = {m_renderAssets.cubeTexture.textureView.Get(),
                                                        m_renderAssets.cubeNormalTexture.textureView.Get()};
            ID3D11ShaderResourceView* visibleIdsView[] = {m_renderAssets.visibleInstanceIdsView.Get()};
            m_context->PSSetSamplers(0, 1, samplers);
            m_context->PSSetShaderResources(0, 2, textureViews);
            m_context->VSSetShaderResources(0, 1, visibleIdsView);

            const std::uint32_t vertexStride = opaqueMesh->vertexStride();
            constexpr std::uint32_t startOffset = 0;
            ID3D11Buffer* geometry[] = {opaqueMesh->vertexBuffer()};
            m_context->IASetVertexBuffers(0, 1, geometry, &vertexStride, &startOffset);
            m_context->IASetIndexBuffer(opaqueMesh->indexBuffer(), opaqueMesh->indexFormat(), 0);

            ID3D11Query* query = m_pipelineQueries[m_submittedQueryCount % Dx11Renderer::kPipelineQueryCount].Get();
            m_context->Begin(query);
            m_context->DrawIndexedInstancedIndirect(m_renderAssets.indirectArgsBuffer.Get(), 0);
            m_context->End(query);
            ++m_submittedQueryCount;
        }
    }

    for (const RenderItem* renderItem : skyboxItems) {
        drawItem(renderItem);
    }

    for (const RenderItem* renderItem : transparentItems) {
        drawItem(renderItem);
    }

    ID3D11ShaderResourceView* nullSceneViews[] = {nullptr, nullptr};
    ID3D11ShaderResourceView* nullVertexViews[] = {nullptr};
    m_context->PSSetShaderResources(0, 2, nullSceneViews);
    m_context->VSSetShaderResources(0, 1, nullVertexViews);
    m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState(nullptr, 0);

    ID3D11RenderTargetView* backBufferTargets[] = {m_backBufferTarget.Get()};
    m_context->OMSetRenderTargets(1, backBufferTargets, nullptr);
    m_context->ClearRenderTargetView(m_backBufferTarget.Get(), kClearColor);
    m_context->IASetInputLayout(nullptr);
    m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    m_context->VSSetShader(m_renderAssets.postProcessPass.vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_renderAssets.postProcessPass.pixelShader.Get(), nullptr, 0);

    PostProcessBuffer postProcessBuffer{};
    postProcessBuffer.mode = DirectX::XMUINT4{static_cast<std::uint32_t>(m_postProcessMode), 0u, 0u, 0u};
    m_context->UpdateSubresource(m_renderAssets.postProcessBuffer.Get(), 0, nullptr, &postProcessBuffer, 0, 0);
    ID3D11Buffer* postProcessBuffers[] = {m_renderAssets.postProcessBuffer.Get()};
    m_context->PSSetConstantBuffers(0, 1, postProcessBuffers);

    ID3D11SamplerState* postProcessSamplers[] = {m_renderAssets.postProcessTexture.samplerState.Get()};
    ID3D11ShaderResourceView* postProcessViews[] = {m_renderAssets.postProcessTexture.textureView.Get()};
    m_context->PSSetSamplers(0, 1, postProcessSamplers);
    m_context->PSSetShaderResources(0, 1, postProcessViews);
    m_context->Draw(3, 0);

    ID3D11ShaderResourceView* nullPostProcessViews[] = {nullptr};
    m_context->PSSetShaderResources(0, 1, nullPostProcessViews);

    readPipelineQueries();

    const HRESULT result = m_swapChain->Present(0, 0);
    assert(SUCCEEDED(result));
}

bool Dx11Renderer::resize(std::uint32_t width, std::uint32_t height) {
    if (!m_swapChain || width == 0 || height == 0) {
        return true;
    }

    releaseRenderTargets();

    const HRESULT result = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(result)) {
        return false;
    }

    m_width = width;
    m_height = height;
    return createBackBufferTarget() && createDepthResources() && createSceneColorResources();
}

void Dx11Renderer::adjustCamera(float deltaDirection, float deltaTilt) {
    m_camera.adjustAngles(deltaDirection, deltaTilt);
}

void Dx11Renderer::moveCamera(float forwardDelta, float rightDelta) { m_camera.moveLocal(forwardDelta, rightDelta); }

void Dx11Renderer::toggleSceneAutoRotation() {
    for (const std::shared_ptr<AutoRotatable>& autoRotatable : m_autoRotatables) {
        if (autoRotatable) {
            autoRotatable->toggleAutoRotation();
        }
    }
}

void Dx11Renderer::cyclePostProcessMode() {
    switch (m_postProcessMode) {
        case PostProcessMode::Original:
            m_postProcessMode = PostProcessMode::Grayscale;
            break;
        case PostProcessMode::Grayscale:
            m_postProcessMode = PostProcessMode::Sepia;
            break;
        case PostProcessMode::Sepia:
        default:
            m_postProcessMode = PostProcessMode::Original;
            break;
    }
}

std::uint32_t Dx11Renderer::gpuVisibleInstanceCount() const { return m_gpuVisibleInstances; }

void Dx11Renderer::readPipelineQueries() {
    D3D11_QUERY_DATA_PIPELINE_STATISTICS statistics{};
    while (m_completedQueryCount < m_submittedQueryCount) {
        ID3D11Query* query = m_pipelineQueries[m_completedQueryCount % Dx11Renderer::kPipelineQueryCount].Get();
        const HRESULT result = m_context->GetData(query, &statistics, sizeof(statistics), 0);
        if (result != S_OK) {
            break;
        }

        m_gpuVisibleInstances = static_cast<std::uint32_t>(statistics.IAPrimitives / kCubePrimitiveCount);
        ++m_completedQueryCount;
    }
}

const wchar_t* Dx11Renderer::postProcessModeName() const {
    switch (m_postProcessMode) {
        case PostProcessMode::Original:
            return L"Original";
        case PostProcessMode::Grayscale:
            return L"Grayscale";
        case PostProcessMode::Sepia:
        default:
            return L"Sepia";
    }
}

void Dx11Renderer::shutdown() {
    releaseRenderTargets();
    releaseSceneResources();
    m_swapChain.Reset();

    if (m_context) {
        m_context->ClearState();
        m_context->Flush();
    }

    m_context.Reset();

#ifdef _DEBUG
    if (m_device) {
        ComPtr<ID3D11Debug> debugInterface;
        if (SUCCEEDED(m_device.As(&debugInterface))) {
            debugInterface->ReportLiveDeviceObjects(
                static_cast<D3D11_RLDO_FLAGS>(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL));
        }
    }
#endif

    m_device.Reset();

    m_width = 0;
    m_height = 0;
    m_hasLastFrameTime = false;
}

bool Dx11Renderer::createBackBufferTarget() {
    ComPtr<ID3D11Texture2D> surface;
    HRESULT result =
        m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(surface.GetAddressOf()));
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreateRenderTargetView(surface.Get(), nullptr, m_backBufferTarget.GetAddressOf());

    return SUCCEEDED(result);
}

bool Dx11Renderer::createDepthResources() {
    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = m_width;
    depthDesc.Height = m_height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = kSampleCount;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    HRESULT result = m_device->CreateTexture2D(&depthDesc, nullptr, m_depthBuffer.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreateDepthStencilView(m_depthBuffer.Get(), nullptr, m_depthTarget.GetAddressOf());
    return SUCCEEDED(result);
}

bool Dx11Renderer::createSceneColorResources() {
    D3D11_TEXTURE2D_DESC colorDesc{};
    colorDesc.Width = m_width;
    colorDesc.Height = m_height;
    colorDesc.MipLevels = 1;
    colorDesc.ArraySize = 1;
    colorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    colorDesc.SampleDesc.Count = kSampleCount;
    colorDesc.Usage = D3D11_USAGE_DEFAULT;
    colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT result =
        m_device->CreateTexture2D(&colorDesc, nullptr, m_renderAssets.postProcessTexture.texture.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreateRenderTargetView(m_renderAssets.postProcessTexture.texture.Get(), nullptr,
                                              m_sceneColorTarget.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
    viewDesc.Format = colorDesc.Format;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MostDetailedMip = 0;
    viewDesc.Texture2D.MipLevels = 1;
    result = m_device->CreateShaderResourceView(m_renderAssets.postProcessTexture.texture.Get(), &viewDesc,
                                                m_renderAssets.postProcessTexture.textureView.GetAddressOf());
    return SUCCEEDED(result);
}

bool Dx11Renderer::createPipelineResources() {
    ComPtr<ID3DBlob> objectVertexCode;
    ComPtr<ID3DBlob> objectPixelCode;
    ComPtr<ID3DBlob> skyboxVertexCode;
    ComPtr<ID3DBlob> skyboxPixelCode;
    ComPtr<ID3DBlob> postProcessVertexCode;
    ComPtr<ID3DBlob> postProcessPixelCode;
    ComPtr<ID3DBlob> cullComputeCode;

    HRESULT result = compileShaderFromFile(L"Shaders/triangle_vs.hlsl", "main", "vs_5_0", objectVertexCode);
    if (FAILED(result)) {
        return false;
    }

    result = compileShaderFromFile(L"Shaders/triangle_ps.hlsl", "main", "ps_5_0", objectPixelCode);
    if (FAILED(result)) {
        return false;
    }

    result = compileShaderFromFile(L"Shaders/skybox_vs.hlsl", "main", "vs_5_0", skyboxVertexCode);
    if (FAILED(result)) {
        return false;
    }

    result = compileShaderFromFile(L"Shaders/skybox_ps.hlsl", "main", "ps_5_0", skyboxPixelCode);
    if (FAILED(result)) {
        return false;
    }

    result = compileShaderFromFile(L"Shaders/postprocess_vs.hlsl", "main", "vs_5_0", postProcessVertexCode);
    if (FAILED(result)) {
        return false;
    }

    result = compileShaderFromFile(L"Shaders/postprocess_ps.hlsl", "main", "ps_5_0", postProcessPixelCode);
    if (FAILED(result)) {
        return false;
    }

    result = compileShaderFromFile(L"Shaders/cull_cs.hlsl", "main", "cs_5_0", cullComputeCode);
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreateVertexShader(objectVertexCode->GetBufferPointer(), objectVertexCode->GetBufferSize(),
                                          nullptr, m_renderAssets.objectPass.vertexShader.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreatePixelShader(objectPixelCode->GetBufferPointer(), objectPixelCode->GetBufferSize(), nullptr,
                                         m_renderAssets.objectPass.pixelShader.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreateVertexShader(skyboxVertexCode->GetBufferPointer(), skyboxVertexCode->GetBufferSize(),
                                          nullptr, m_renderAssets.skyboxPass.vertexShader.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreatePixelShader(skyboxPixelCode->GetBufferPointer(), skyboxPixelCode->GetBufferSize(), nullptr,
                                         m_renderAssets.skyboxPass.pixelShader.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result =
        m_device->CreateVertexShader(postProcessVertexCode->GetBufferPointer(), postProcessVertexCode->GetBufferSize(),
                                     nullptr, m_renderAssets.postProcessPass.vertexShader.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result =
        m_device->CreatePixelShader(postProcessPixelCode->GetBufferPointer(), postProcessPixelCode->GetBufferSize(),
                                    nullptr, m_renderAssets.postProcessPass.pixelShader.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreateComputeShader(cullComputeCode->GetBufferPointer(), cullComputeCode->GetBufferSize(),
                                           nullptr, m_renderAssets.cullShader.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC objectVertexFormat[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    result = m_device->CreateInputLayout(objectVertexFormat, 4, objectVertexCode->GetBufferPointer(),
                                         objectVertexCode->GetBufferSize(),
                                         m_renderAssets.objectPass.inputLayout.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC skyboxVertexFormat[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    result = m_device->CreateInputLayout(skyboxVertexFormat, 1, skyboxVertexCode->GetBufferPointer(),
                                         skyboxVertexCode->GetBufferSize(),
                                         m_renderAssets.skyboxPass.inputLayout.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_RASTERIZER_DESC rasterDesc{};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable = TRUE;
    result = m_device->CreateRasterizerState(&rasterDesc, m_renderAssets.rasterizerState.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC skyboxDepthDesc{};
    skyboxDepthDesc.DepthEnable = TRUE;
    skyboxDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    skyboxDepthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    result =
        m_device->CreateDepthStencilState(&skyboxDepthDesc, m_renderAssets.skyboxPass.depthStencilState.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_BLEND_DESC transparentBlendDesc{};
    transparentBlendDesc.AlphaToCoverageEnable = FALSE;
    transparentBlendDesc.IndependentBlendEnable = FALSE;
    transparentBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    transparentBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    transparentBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    transparentBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    transparentBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    transparentBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    transparentBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    result = m_device->CreateBlendState(&transparentBlendDesc, m_renderAssets.transparentBlendState.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC transparentDepthDesc{};
    transparentDepthDesc.DepthEnable = TRUE;
    transparentDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    transparentDepthDesc.DepthFunc = D3D11_COMPARISON_LESS;
    result =
        m_device->CreateDepthStencilState(&transparentDepthDesc, m_renderAssets.transparentDepthState.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_BUFFER_DESC objectBufferDesc{};
    objectBufferDesc.ByteWidth = static_cast<unsigned int>(sizeof(ObjectBuffer));
    objectBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    objectBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    result = m_device->CreateBuffer(&objectBufferDesc, nullptr, m_renderAssets.objectBuffer.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_BUFFER_DESC sceneBufferDesc{};
    sceneBufferDesc.ByteWidth = static_cast<unsigned int>(sizeof(SceneBuffer));
    sceneBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    sceneBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    sceneBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    result = m_device->CreateBuffer(&sceneBufferDesc, nullptr, m_renderAssets.sceneBuffer.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_BUFFER_DESC opaqueInstanceBufferDesc{};
    opaqueInstanceBufferDesc.ByteWidth = static_cast<unsigned int>(sizeof(OpaqueInstanceData) * kMaxOpaqueInstances);
    opaqueInstanceBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    opaqueInstanceBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    opaqueInstanceBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    result =
        m_device->CreateBuffer(&opaqueInstanceBufferDesc, nullptr, m_renderAssets.opaqueInstanceBuffer.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_BUFFER_DESC cullParamsBufferDesc{};
    cullParamsBufferDesc.ByteWidth = static_cast<unsigned int>(sizeof(CullParamsBuffer));
    cullParamsBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    cullParamsBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    result = m_device->CreateBuffer(&cullParamsBufferDesc, nullptr, m_renderAssets.cullParamsBuffer.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_BUFFER_DESC postProcessBufferDesc{};
    postProcessBufferDesc.ByteWidth = static_cast<unsigned int>(sizeof(PostProcessBuffer));
    postProcessBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    postProcessBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    result = m_device->CreateBuffer(&postProcessBufferDesc, nullptr, m_renderAssets.postProcessBuffer.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_BUFFER_DESC visibleIdsBufferDesc{};
    visibleIdsBufferDesc.ByteWidth = static_cast<unsigned int>(sizeof(std::uint32_t) * kMaxOpaqueInstances);
    visibleIdsBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    visibleIdsBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    visibleIdsBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    visibleIdsBufferDesc.StructureByteStride = sizeof(std::uint32_t);

    result =
        m_device->CreateBuffer(&visibleIdsBufferDesc, nullptr, m_renderAssets.visibleInstanceIdsBuffer.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreateShaderResourceView(m_renderAssets.visibleInstanceIdsBuffer.Get(), nullptr,
                                                m_renderAssets.visibleInstanceIdsView.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreateUnorderedAccessView(m_renderAssets.visibleInstanceIdsBuffer.Get(), nullptr,
                                                 m_renderAssets.visibleInstanceIdsUav.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_BUFFER_DESC indirectArgsSrcBufferDesc{};
    indirectArgsSrcBufferDesc.ByteWidth = sizeof(D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS);
    indirectArgsSrcBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indirectArgsSrcBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    indirectArgsSrcBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    indirectArgsSrcBufferDesc.StructureByteStride = sizeof(std::uint32_t);

    result = m_device->CreateBuffer(&indirectArgsSrcBufferDesc, nullptr,
                                    m_renderAssets.indirectArgsSrcBuffer.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreateUnorderedAccessView(m_renderAssets.indirectArgsSrcBuffer.Get(), nullptr,
                                                 m_renderAssets.indirectArgsUav.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_BUFFER_DESC indirectArgsBufferDesc{};
    indirectArgsBufferDesc.ByteWidth = sizeof(D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS);
    indirectArgsBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indirectArgsBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

    result = m_device->CreateBuffer(&indirectArgsBufferDesc, nullptr, m_renderAssets.indirectArgsBuffer.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_QUERY_DESC queryDesc{};
    queryDesc.Query = D3D11_QUERY_PIPELINE_STATISTICS;
    queryDesc.MiscFlags = 0;
    for (ComPtr<ID3D11Query>& query : m_pipelineQueries) {
        result = m_device->CreateQuery(&queryDesc, query.GetAddressOf());
        if (FAILED(result)) {
            return false;
        }
    }

    D3D11_SAMPLER_DESC wrapSamplerDesc{};
    wrapSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    wrapSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    wrapSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    wrapSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    wrapSamplerDesc.MinLOD = 0.0f;
    wrapSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    wrapSamplerDesc.MaxAnisotropy = 1;
    wrapSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    result = m_device->CreateSamplerState(&wrapSamplerDesc, m_renderAssets.cubeTexture.samplerState.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }
    m_renderAssets.cubeNormalTexture.samplerState = m_renderAssets.cubeTexture.samplerState;

    D3D11_SAMPLER_DESC clampSamplerDesc = wrapSamplerDesc;
    clampSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    clampSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    clampSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    result = m_device->CreateSamplerState(&clampSamplerDesc, m_renderAssets.skyboxTexture.samplerState.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result =
        m_device->CreateSamplerState(&clampSamplerDesc, m_renderAssets.postProcessTexture.samplerState.GetAddressOf());
    return SUCCEEDED(result);
}

bool Dx11Renderer::createTextureResources() {
    std::vector<TextureDescription> cubeTextureDescriptions(2);
    if (!loadDDS(L"Assets\\cube\\inst_brick.dds", cubeTextureDescriptions[0]) ||
        !loadDDS(L"Assets\\cube\\inst_gravel.dds", cubeTextureDescriptions[1]) ||
        !createTexture2DArrayResource(m_device.Get(), cubeTextureDescriptions, m_renderAssets.cubeTexture.texture,
                                      m_renderAssets.cubeTexture.textureView)) {
        return false;
    }

    TextureDescription cubeNormalTextureDescription;
    if (loadDDS(L"Assets\\cube\\active_normal.dds", cubeNormalTextureDescription)) {
        if (!createTexture2DResource(m_device.Get(), cubeNormalTextureDescription,
                                     m_renderAssets.cubeNormalTexture.texture,
                                     m_renderAssets.cubeNormalTexture.textureView)) {
            return false;
        }
    } else if (!createFlatNormalTextureResource(m_device.Get(), m_renderAssets.cubeNormalTexture.texture,
                                                m_renderAssets.cubeNormalTexture.textureView)) {
        return false;
    }

    const std::array<const wchar_t*, 6> cubemapFacePaths = {
        L"Assets\\sky\\active\\px.dds", L"Assets\\sky\\active\\nx.dds", L"Assets\\sky\\active\\py.dds",
        L"Assets\\sky\\active\\ny.dds", L"Assets\\sky\\active\\pz.dds", L"Assets\\sky\\active\\nz.dds",
    };

    std::array<TextureDescription, 6> cubemapFaces{};
    for (std::size_t faceIndex = 0; faceIndex < cubemapFacePaths.size(); ++faceIndex) {
        if (!loadDDS(cubemapFacePaths[faceIndex], cubemapFaces[faceIndex])) {
            return false;
        }
    }

    return createCubemapResource(m_device.Get(), cubemapFaces, m_renderAssets.skyboxTexture.texture,
                                 m_renderAssets.skyboxTexture.textureView);
}

bool Dx11Renderer::buildScene() {
    m_renderItems.clear();
    m_opaqueCubeItems.clear();
    m_autoRotatables.clear();

    if (!createTextureResources()) {
        return false;
    }

    SkyboxRenderItem::Params skyboxParams{};
    skyboxParams.size = 60.0f;
    auto skybox = std::make_shared<SkyboxRenderItem>(m_device.Get(), skyboxParams);
    if (!skybox->mesh() || !skybox->mesh()->isValid()) {
        return false;
    }

    constexpr int kOpaqueCubeRows = 5;
    constexpr int kOpaqueCubeColumns = 5;
    constexpr float kOpaqueCubeSpacingX = 1.9f;
    constexpr float kOpaqueCubeSpacingZ = 2.15f;

    for (int row = 0; row < kOpaqueCubeRows; ++row) {
        for (int column = 0; column < kOpaqueCubeColumns; ++column) {
            CubeRenderItem::Params opaqueParams{};
            opaqueParams.size = 0.95f;
            opaqueParams.rotationSpeed = ((row + column) % 2 == 0) ? 0.55f : -0.55f;
            opaqueParams.rotationOffset = static_cast<float>(row * kOpaqueCubeColumns + column) * 0.17f;
            opaqueParams.position = DirectX::XMFLOAT3{
                (static_cast<float>(column) - static_cast<float>(kOpaqueCubeColumns - 1) * 0.5f) * kOpaqueCubeSpacingX,
                0.0f, 4.6f + static_cast<float>(row) * kOpaqueCubeSpacingZ};
            opaqueParams.useNormalMap = true;
            opaqueParams.textureIndex = static_cast<std::uint32_t>((row + column) % 2);
            opaqueParams.colorTint = ((row + column) % 2 == 0) ? DirectX::XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f}
                                                               : DirectX::XMFLOAT4{0.92f, 0.98f, 1.0f, 1.0f};

            auto opaqueCube = std::make_shared<CubeRenderItem>(m_device.Get(), opaqueParams);
            if (!opaqueCube->mesh() || !opaqueCube->mesh()->isValid()) {
                return false;
            }

            m_opaqueCubeItems.push_back(opaqueCube);
            m_renderItems.push_back(opaqueCube);
            m_autoRotatables.push_back(opaqueCube);
        }
    }
    m_renderItems.push_back(skybox);

    CullParamsBuffer cullParams{};
    cullParams.numShapes = DirectX::XMUINT4{
        static_cast<std::uint32_t>((std::min)(m_opaqueCubeItems.size(), static_cast<std::size_t>(kMaxOpaqueInstances))),
        0u, 0u, 0u};
    for (std::uint32_t instanceIndex = 0; instanceIndex < cullParams.numShapes.x; ++instanceIndex) {
        const CubeRenderItem& renderItem = *m_opaqueCubeItems[instanceIndex];
        const DirectX::XMFLOAT3 boundsMin = renderItem.boundsMin();
        const DirectX::XMFLOAT3 boundsMax = renderItem.boundsMax();
        cullParams.bbMin[instanceIndex] = DirectX::XMFLOAT4{boundsMin.x, boundsMin.y, boundsMin.z, 1.0f};
        cullParams.bbMax[instanceIndex] = DirectX::XMFLOAT4{boundsMax.x, boundsMax.y, boundsMax.z, 1.0f};
    }
    m_context->UpdateSubresource(m_renderAssets.cullParamsBuffer.Get(), 0, nullptr, &cullParams, 0, 0);

    m_submittedQueryCount = 0u;
    m_completedQueryCount = 0u;
    m_gpuVisibleInstances = 0u;
    return true;
}

void Dx11Renderer::releaseRenderTargets() {
    if (m_context) {
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    m_sceneColorTarget.Reset();
    m_renderAssets.postProcessTexture.textureView.Reset();
    m_renderAssets.postProcessTexture.texture.Reset();
    m_depthTarget.Reset();
    m_depthBuffer.Reset();
    m_backBufferTarget.Reset();
}

void Dx11Renderer::releaseSceneResources() {
    m_renderItems.clear();
    m_opaqueCubeItems.clear();
    m_autoRotatables.clear();

    m_renderAssets.skyboxTexture.textureView.Reset();
    m_renderAssets.skyboxTexture.texture.Reset();
    m_renderAssets.cubeNormalTexture.textureView.Reset();
    m_renderAssets.cubeNormalTexture.texture.Reset();
    m_renderAssets.cubeTexture.textureView.Reset();
    m_renderAssets.cubeTexture.texture.Reset();
    m_renderAssets.postProcessTexture.samplerState.Reset();
    m_renderAssets.skyboxTexture.samplerState.Reset();
    m_renderAssets.cubeNormalTexture.samplerState.Reset();
    m_renderAssets.cubeTexture.samplerState.Reset();
    m_renderAssets.sceneBuffer.Reset();
    m_renderAssets.objectBuffer.Reset();
    m_renderAssets.opaqueInstanceBuffer.Reset();
    m_renderAssets.cullParamsBuffer.Reset();
    m_renderAssets.postProcessBuffer.Reset();
    m_renderAssets.visibleInstanceIdsUav.Reset();
    m_renderAssets.visibleInstanceIdsView.Reset();
    m_renderAssets.visibleInstanceIdsBuffer.Reset();
    m_renderAssets.indirectArgsUav.Reset();
    m_renderAssets.indirectArgsSrcBuffer.Reset();
    m_renderAssets.indirectArgsBuffer.Reset();
    m_renderAssets.cullShader.Reset();
    m_renderAssets.transparentDepthState.Reset();
    m_renderAssets.transparentBlendState.Reset();
    m_renderAssets.skyboxPass.depthStencilState.Reset();
    m_renderAssets.rasterizerState.Reset();
    m_renderAssets.postProcessPass.pixelShader.Reset();
    m_renderAssets.postProcessPass.vertexShader.Reset();
    m_renderAssets.skyboxPass.inputLayout.Reset();
    m_renderAssets.skyboxPass.pixelShader.Reset();
    m_renderAssets.skyboxPass.vertexShader.Reset();
    m_renderAssets.objectPass.inputLayout.Reset();
    m_renderAssets.objectPass.pixelShader.Reset();
    m_renderAssets.objectPass.vertexShader.Reset();

    for (ComPtr<ID3D11Query>& query : m_pipelineQueries) {
        query.Reset();
    }

    m_submittedQueryCount = 0u;
    m_completedQueryCount = 0u;
    m_gpuVisibleInstances = 0u;
}
