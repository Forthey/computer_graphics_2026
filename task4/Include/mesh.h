#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <d3d11.h>
#include <dxgiformat.h>
#include <wrl/client.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <cstdint>
#include <memory>

using Microsoft::WRL::ComPtr;

class Mesh final {
public:
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) = default;
    Mesh& operator=(Mesh&&) = default;
    ~Mesh() = default;

    static std::shared_ptr<Mesh> createIndexedU16Immutable(ID3D11Device* device, const void* vertexData,
                                                           std::uint32_t vertexDataSize, std::uint32_t vertexStride,
                                                           const std::uint16_t* indexData, std::uint32_t indexCount);

    bool isValid() const;
    ID3D11Buffer* vertexBuffer() const;
    ID3D11Buffer* indexBuffer() const;
    DXGI_FORMAT indexFormat() const;
    std::uint32_t indexCount() const;
    std::uint32_t vertexStride() const;

private:
    Mesh(ComPtr<ID3D11Buffer>&& vertexBuffer, ComPtr<ID3D11Buffer>&& indexBuffer, DXGI_FORMAT indexFormat,
         std::uint32_t indexCount, std::uint32_t vertexStride);

    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    DXGI_FORMAT m_indexFormat = DXGI_FORMAT_R16_UINT;
    std::uint32_t m_indexCount = 0;
    std::uint32_t m_vertexStride = 0;
};
