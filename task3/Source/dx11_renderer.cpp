#include "dx11_renderer.h"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <cassert>
#include <cwchar>
#include <memory>

#include "cube_render_item.h"
#include "framework.h"
#include "render_item.h"

namespace {
constexpr std::uint32_t kSwapChainBufferCount = 2;
constexpr std::uint32_t kSampleCount = 1;
constexpr float kClearColor[4] = {0.12f, 0.18f, 0.24f, 1.0f};
constexpr float kViewportMinDepth = 0.0f;
constexpr float kViewportMaxDepth = 1.0f;
constexpr float kClearDepthValue = 1.0f;

struct ObjectBuffer {
    DirectX::XMFLOAT4X4 modelMatrix;
};

struct SceneBuffer {
    DirectX::XMFLOAT4X4 viewProjectionMatrix;
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

    result =
        D3D11CreateDevice(selectedAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, static_cast<unsigned int>(flags), levels, 1,
                          D3D11_SDK_VERSION, m_device.GetAddressOf(), &featureLevel, m_context.GetAddressOf());

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

    m_startTime = std::chrono::steady_clock::now();
    m_hasStartTime = true;
    return true;
}

void Dx11Renderer::renderFrame() {
    if (!m_context || !m_backBufferTarget || !m_depthTarget || !m_swapChain) {
        return;
    }

    ID3D11RenderTargetView* targets[] = {m_backBufferTarget.Get()};
    m_context->OMSetRenderTargets(1, targets, m_depthTarget.Get());

    m_context->ClearRenderTargetView(m_backBufferTarget.Get(), kClearColor);
    m_context->ClearDepthStencilView(m_depthTarget.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, kClearDepthValue,
                                     0);

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = kViewportMinDepth;
    viewport.MaxDepth = kViewportMaxDepth;
    m_context->RSSetViewports(1, &viewport);
    m_context->RSSetState(m_rasterizerState.Get());

    m_context->IASetInputLayout(m_vertexLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    const auto now = std::chrono::steady_clock::now();
    if (!m_hasStartTime) {
        m_startTime = now;
        m_hasStartTime = true;
    }
    const float elapsedSec = std::chrono::duration<float>(now - m_startTime).count();

    const DirectX::XMMATRIX viewMatrix = m_camera.buildViewMatrix();
    const float aspect = (m_height != 0) ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    const DirectX::XMMATRIX projectionMatrix = m_camera.buildProjectionMatrix(aspect);
    const DirectX::XMMATRIX viewProjectionMatrix = DirectX::XMMatrixMultiply(viewMatrix, projectionMatrix);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT mapResult = m_context->Map(m_sceneBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    assert(SUCCEEDED(mapResult));
    if (SUCCEEDED(mapResult)) {
        auto* sceneBuffer = reinterpret_cast<SceneBuffer*>(mapped.pData);
        DirectX::XMStoreFloat4x4(&sceneBuffer->viewProjectionMatrix, viewProjectionMatrix);
        m_context->Unmap(m_sceneBuffer.Get(), 0);
    }

    ID3D11Buffer* constantBuffers[] = {m_objectBuffer.Get(), m_sceneBuffer.Get()};
    m_context->VSSetConstantBuffers(0, 2, constantBuffers);

    for (const std::shared_ptr<RenderItem>& renderItem : m_renderItems) {
        if (!renderItem) {
            continue;
        }

        const std::shared_ptr<Mesh>& mesh = renderItem->mesh();
        if (!mesh || !mesh->isValid()) {
            continue;
        }

        const DirectX::XMMATRIX modelMatrix = renderItem->buildModelMatrix(elapsedSec);

        ObjectBuffer objectBuffer{};
        DirectX::XMStoreFloat4x4(&objectBuffer.modelMatrix, modelMatrix);
        m_context->UpdateSubresource(m_objectBuffer.Get(), 0, nullptr, &objectBuffer, 0, 0);

        const std::uint32_t vertexStride = mesh->vertexStride();
        constexpr std::uint32_t startOffset = 0;
        ID3D11Buffer* geometry[] = {mesh->vertexBuffer()};
        m_context->IASetVertexBuffers(0, 1, geometry, &vertexStride, &startOffset);
        m_context->IASetIndexBuffer(mesh->indexBuffer(), mesh->indexFormat(), 0);

        m_context->DrawIndexed(mesh->indexCount(), 0, 0);
    }

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

void Dx11Renderer::adjustCamera(float deltaYaw, float deltaPitch) { m_camera.adjustAngles(deltaYaw, deltaPitch); }

void Dx11Renderer::moveCamera(float forwardDelta, float rightDelta) { m_camera.moveLocal(forwardDelta, rightDelta); }

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
    m_hasStartTime = false;
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
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
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
    ComPtr<ID3DBlob> vertexCode;
    ComPtr<ID3DBlob> pixelCode;
    HRESULT result = compileShaderFromFile(L"Shaders/triangle_vs.hlsl", "main", "vs_5_0", vertexCode);
    if (FAILED(result)) {
        return false;
    }

    result = compileShaderFromFile(L"Shaders/triangle_ps.hlsl", "main", "ps_5_0", pixelCode);
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreateVertexShader(vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(), nullptr,
                                          m_vertexShader.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = m_device->CreatePixelShader(pixelCode->GetBufferPointer(), pixelCode->GetBufferSize(), nullptr,
                                         m_pixelShader.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC vertexFormat[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    result = m_device->CreateInputLayout(vertexFormat, 2, vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(),
                                         m_vertexLayout.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_RASTERIZER_DESC rasterDesc{};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable = TRUE;
    result = m_device->CreateRasterizerState(&rasterDesc, m_rasterizerState.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_BUFFER_DESC objectBufferDesc{};
    objectBufferDesc.ByteWidth = static_cast<unsigned int>(sizeof(ObjectBuffer));
    objectBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    objectBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    result = m_device->CreateBuffer(&objectBufferDesc, nullptr, m_objectBuffer.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D3D11_BUFFER_DESC sceneBufferDesc{};
    sceneBufferDesc.ByteWidth = static_cast<unsigned int>(sizeof(SceneBuffer));
    sceneBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    sceneBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    sceneBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    result = m_device->CreateBuffer(&sceneBufferDesc, nullptr, m_sceneBuffer.GetAddressOf());
    return SUCCEEDED(result);
}

bool Dx11Renderer::buildScene() {
    m_renderItems.clear();

    CubeRenderItem::Params cubeParams{};
    cubeParams.size = CubeRenderItem::kDefaultSize;

    auto cube = std::make_shared<CubeRenderItem>(m_device.Get(), cubeParams);
    if (!cube->mesh() || !cube->mesh()->isValid()) {
        return false;
    }

    m_renderItems.push_back(cube);
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

    m_sceneBuffer.Reset();
    m_objectBuffer.Reset();
    m_rasterizerState.Reset();
    m_vertexLayout.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
}
