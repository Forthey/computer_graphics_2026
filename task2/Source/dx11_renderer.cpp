#include "dx11_renderer.h"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <cassert>
#include <cwchar>

#include "framework.h"

namespace {
struct Vertex {
    float position[3];
    float color[3];
};

HRESULT compileShaderFromFile(const wchar_t* shaderPath, const char* entryPoint, const char* target,
                              ComPtr<ID3DBlob>& compiledCode) {
    std::uint32_t compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    static constexpr const wchar_t* kPathCandidates[] = {
        L"%s",
        L"..\\..\\%s",
    };

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
                                                  target, static_cast<UINT>(compileFlags), 0,
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
        D3D11CreateDevice(selectedAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, static_cast<UINT>(flags), levels, 1,
                          D3D11_SDK_VERSION, m_device.GetAddressOf(), &featureLevel, m_context.GetAddressOf());

    if (FAILED(result) || featureLevel != D3D_FEATURE_LEVEL_11_0) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC swapChainSetup{};
    swapChainSetup.BufferCount = 2;
    swapChainSetup.BufferDesc.Width = m_width;
    swapChainSetup.BufferDesc.Height = m_height;
    swapChainSetup.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainSetup.BufferDesc.RefreshRate.Numerator = 0;
    swapChainSetup.BufferDesc.RefreshRate.Denominator = 1;
    swapChainSetup.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainSetup.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainSetup.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainSetup.OutputWindow = window;
    swapChainSetup.SampleDesc.Count = 1;
    swapChainSetup.SampleDesc.Quality = 0;
    swapChainSetup.Windowed = true;
    swapChainSetup.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainSetup.Flags = 0;

    result = factory->CreateSwapChain(m_device.Get(), &swapChainSetup, m_swapChain.GetAddressOf());
    if (FAILED(result)) {
        shutdown();
        return false;
    }

    if (!createBackBufferTarget()) {
        shutdown();
        return false;
    }

    if (!createTriangleResources()) {
        shutdown();
        return false;
    }

    return true;
}

void Dx11Renderer::renderFrame() {
    if (!m_context || !m_backBufferTarget || !m_swapChain) {
        return;
    }

    ID3D11RenderTargetView* targets[] = {m_backBufferTarget.Get()};
    m_context->OMSetRenderTargets(1, targets, nullptr);

    static const FLOAT clearColor[4] = {0.12f, 0.18f, 0.24f, 1.0f};
    m_context->ClearRenderTargetView(m_backBufferTarget.Get(), clearColor);

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<FLOAT>(m_width);
    viewport.Height = static_cast<FLOAT>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    m_context->IASetInputLayout(m_vertexLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    constexpr std::uint32_t vertexStep = sizeof(Vertex);
    constexpr std::uint32_t startOffset = 0;
    ID3D11Buffer* geometry[] = {m_triangleVertices.Get()};
    m_context->IASetVertexBuffers(0, 1, geometry, &vertexStep, &startOffset);

    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    m_context->Draw(3, 0);

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
    return createBackBufferTarget();
}

void Dx11Renderer::shutdown() {
    releaseRenderTargets();
    releaseTriangleResources();
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

bool Dx11Renderer::createTriangleResources() {
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

    static constexpr Vertex vertices[] = {
        {{0.0f, 0.6f, 0.0f}, {1.0f, 0.1f, 0.1f}},
        {{0.6f, -0.6f, 0.0f}, {0.1f, 1.0f, 0.1f}},
        {{-0.6f, -0.6f, 0.0f}, {0.1f, 0.1f, 1.0f}},
    };

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(vertices));
    bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA bufferData{};
    bufferData.pSysMem = vertices;

    result = m_device->CreateBuffer(&bufferDesc, &bufferData, m_triangleVertices.GetAddressOf());
    return SUCCEEDED(result);
}

void Dx11Renderer::releaseRenderTargets() {
    if (m_context) {
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    m_backBufferTarget.Reset();
}

void Dx11Renderer::releaseTriangleResources() {
    m_triangleVertices.Reset();
    m_vertexLayout.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
}
