#include "Dx11Renderer.h"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <DirectXMath.h>
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <array>
#include <cassert>
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
constexpr float kClearColor[4] = {0.02f, 0.04f, 0.08f, 1.0f};
constexpr float kViewportMinDepth = 0.0f;
constexpr float kViewportMaxDepth = 1.0f;
constexpr float kClearDepthValue = 1.0f;

struct ObjectBuffer {
    DirectX::XMFLOAT4X4 modelMatrix;
};

struct SceneBuffer {
    DirectX::XMFLOAT4X4 viewProjectionMatrix;
    DirectX::XMFLOAT4 cameraPosition;
};

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

bool createCubemapResource(ID3D11Device* device, const std::array<TextureDescription, 6>& faceTextures,
                           ComPtr<ID3D11Texture2D>& cubemapTexture, ComPtr<ID3D11ShaderResourceView>& cubemapView) {
    if (device == nullptr) {
        return false;
    }

    const DXGI_FORMAT format = faceTextures[0].fmt;
    const std::uint32_t width = faceTextures[0].width;
    const std::uint32_t height = faceTextures[0].height;
    const std::uint32_t mipmapsCount = faceTextures[0].mipmapsCount;
    if (width == 0u || height == 0u || mipmapsCount == 0u ||
        faceTextures[0].subresources.size() != mipmapsCount) {
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
            const std::size_t subresourceIndex = static_cast<std::size_t>(D3D11CalcSubresource(
                mipIndex, static_cast<UINT>(faceIndex), mipmapsCount));
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
}}  // namespace

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

    if (!createBackBufferTarget() || !createDepthResources() || !createPipelineResources() || !buildScene()) {
        shutdown();
        return false;
    }

    m_lastFrameTime = std::chrono::steady_clock::now();
    m_hasLastFrameTime = true;
    return true;
}

void Dx11Renderer::renderFrame() {
    if (!m_context || !m_backBufferTarget || !m_depthTarget || !m_swapChain) {
        return;
    }

    ID3D11RenderTargetView* targets[] = {m_backBufferTarget.Get()};
    m_context->OMSetRenderTargets(1, targets, m_depthTarget.Get());

    m_context->ClearRenderTargetView(m_backBufferTarget.Get(), kClearColor);
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
    const DirectX::XMFLOAT3 cameraPosition = m_camera.position();

    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT mapResult = m_context->Map(m_renderAssets.sceneBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    assert(SUCCEEDED(mapResult));
    if (SUCCEEDED(mapResult)) {
        auto* sceneBuffer = reinterpret_cast<SceneBuffer*>(mapped.pData);
        DirectX::XMStoreFloat4x4(&sceneBuffer->viewProjectionMatrix, viewProjectionMatrix);
        sceneBuffer->cameraPosition = DirectX::XMFLOAT4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f);
        m_context->Unmap(m_renderAssets.sceneBuffer.Get(), 0);
    }

    ID3D11Buffer* constantBuffers[] = {m_renderAssets.objectBuffer.Get(), m_renderAssets.sceneBuffer.Get()};
    m_context->VSSetConstantBuffers(0, 2, constantBuffers);

    for (const std::shared_ptr<RenderItem>& renderItem : m_renderItems) {
        if (!renderItem) {
            continue;
        }

        const std::shared_ptr<Mesh>& mesh = renderItem->mesh();
        if (!mesh || !mesh->isValid()) {
            continue;
        }

        ID3D11SamplerState* sampler = nullptr;
        ID3D11ShaderResourceView* textureView = nullptr;
        switch (renderItem->type()) {
            case RenderItemType::Skybox:
                m_context->OMSetDepthStencilState(m_renderAssets.skyboxPass.depthStencilState.Get(), 0);
                m_context->IASetInputLayout(m_renderAssets.skyboxPass.inputLayout.Get());
                m_context->VSSetShader(m_renderAssets.skyboxPass.vertexShader.Get(), nullptr, 0);
                m_context->PSSetShader(m_renderAssets.skyboxPass.pixelShader.Get(), nullptr, 0);
                sampler = m_renderAssets.skyboxTexture.samplerState.Get();
                textureView = m_renderAssets.skyboxTexture.textureView.Get();
                break;
            case RenderItemType::OpaqueTextured:
            default:
                m_context->OMSetDepthStencilState(nullptr, 0);
                m_context->IASetInputLayout(m_renderAssets.objectPass.inputLayout.Get());
                m_context->VSSetShader(m_renderAssets.objectPass.vertexShader.Get(), nullptr, 0);
                m_context->PSSetShader(m_renderAssets.objectPass.pixelShader.Get(), nullptr, 0);
                sampler = m_renderAssets.cubeTexture.samplerState.Get();
                textureView = m_renderAssets.cubeTexture.textureView.Get();
                break;
        }

        ID3D11SamplerState* samplers[] = {sampler};
        ID3D11ShaderResourceView* textureViews[] = {textureView};
        m_context->PSSetSamplers(0, 1, samplers);
        m_context->PSSetShaderResources(0, 1, textureViews);

        ObjectBuffer objectBuffer{};
        DirectX::XMStoreFloat4x4(&objectBuffer.modelMatrix, renderItem->buildModelMatrix());
        m_context->UpdateSubresource(m_renderAssets.objectBuffer.Get(), 0, nullptr, &objectBuffer, 0, 0);

        const std::uint32_t vertexStride = mesh->vertexStride();
        constexpr std::uint32_t startOffset = 0;
        ID3D11Buffer* geometry[] = {mesh->vertexBuffer()};
        m_context->IASetVertexBuffers(0, 1, geometry, &vertexStride, &startOffset);
        m_context->IASetIndexBuffer(mesh->indexBuffer(), mesh->indexFormat(), 0);

        m_context->DrawIndexed(mesh->indexCount(), 0, 0);
    }

    ID3D11ShaderResourceView* nullViews[] = {nullptr};
    m_context->PSSetShaderResources(0, 1, nullViews);
    m_context->OMSetDepthStencilState(nullptr, 0);

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
    return createBackBufferTarget() && createDepthResources();
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

bool Dx11Renderer::createPipelineResources() {
    ComPtr<ID3DBlob> objectVertexCode;
    ComPtr<ID3DBlob> objectPixelCode;
    ComPtr<ID3DBlob> skyboxVertexCode;
    ComPtr<ID3DBlob> skyboxPixelCode;

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

    D3D11_INPUT_ELEMENT_DESC objectVertexFormat[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    result = m_device->CreateInputLayout(objectVertexFormat, 2, objectVertexCode->GetBufferPointer(),
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
    skyboxDepthDesc.DepthEnable = FALSE;
    skyboxDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    skyboxDepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    result =
        m_device->CreateDepthStencilState(&skyboxDepthDesc, m_renderAssets.skyboxPass.depthStencilState.GetAddressOf());
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

    D3D11_SAMPLER_DESC clampSamplerDesc = wrapSamplerDesc;
    clampSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    clampSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    clampSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    result = m_device->CreateSamplerState(&clampSamplerDesc, m_renderAssets.skyboxTexture.samplerState.GetAddressOf());
    return SUCCEEDED(result);
}

bool Dx11Renderer::createTextureResources() {
    TextureDescription cubeTextureDescription;
    if (!loadDDS(L"Assets\\cube\\active.dds", cubeTextureDescription) ||
        !createTexture2DResource(m_device.Get(), cubeTextureDescription, m_renderAssets.cubeTexture.texture,
                                 m_renderAssets.cubeTexture.textureView)) {
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

    CubeRenderItem::Params leftCubeParams{};
    leftCubeParams.size = 1.0f;
    leftCubeParams.rotationSpeed = 0.8f;
    leftCubeParams.rotationOffset = 0.0f;
    leftCubeParams.position = DirectX::XMFLOAT3{-1.35f, 0.0f, 2.5f};

    auto leftCube = std::make_shared<CubeRenderItem>(m_device.Get(), leftCubeParams);
    if (!leftCube->mesh() || !leftCube->mesh()->isValid()) {
        return false;
    }

    CubeRenderItem::Params rightCubeParams{};
    rightCubeParams.size = 1.0f;
    rightCubeParams.rotationSpeed = -0.55f;
    rightCubeParams.rotationOffset = 0.65f;
    rightCubeParams.position = DirectX::XMFLOAT3{1.35f, 0.0f, 5.5f};

    auto rightCube = std::make_shared<CubeRenderItem>(m_device.Get(), rightCubeParams);
    if (!rightCube->mesh() || !rightCube->mesh()->isValid()) {
        return false;
    }

    m_renderItems.push_back(skybox);
    m_renderItems.push_back(leftCube);
    m_renderItems.push_back(rightCube);
    m_autoRotatables.push_back(leftCube);
    m_autoRotatables.push_back(rightCube);
    return true;
}

void Dx11Renderer::releaseRenderTargets() {
    if (m_context) {
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    m_depthTarget.Reset();
    m_depthBuffer.Reset();
    m_backBufferTarget.Reset();
}

void Dx11Renderer::releaseSceneResources() {
    m_renderItems.clear();
    m_autoRotatables.clear();

    m_renderAssets.skyboxTexture.textureView.Reset();
    m_renderAssets.skyboxTexture.texture.Reset();
    m_renderAssets.cubeTexture.textureView.Reset();
    m_renderAssets.cubeTexture.texture.Reset();
    m_renderAssets.skyboxTexture.samplerState.Reset();
    m_renderAssets.cubeTexture.samplerState.Reset();
    m_renderAssets.sceneBuffer.Reset();
    m_renderAssets.objectBuffer.Reset();
    m_renderAssets.skyboxPass.depthStencilState.Reset();
    m_renderAssets.rasterizerState.Reset();
    m_renderAssets.skyboxPass.inputLayout.Reset();
    m_renderAssets.skyboxPass.pixelShader.Reset();
    m_renderAssets.skyboxPass.vertexShader.Reset();
    m_renderAssets.objectPass.inputLayout.Reset();
    m_renderAssets.objectPass.pixelShader.Reset();
    m_renderAssets.objectPass.vertexShader.Reset();
}



