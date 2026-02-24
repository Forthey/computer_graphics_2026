#include "mesh.h"

#include <utility>

Mesh::Mesh(ComPtr<ID3D11Buffer>&& vertexBuffer, ComPtr<ID3D11Buffer>&& indexBuffer, DXGI_FORMAT indexFormat,
           std::uint32_t indexCount, std::uint32_t vertexStride)
    : m_vertexBuffer(std::move(vertexBuffer)),
      m_indexBuffer(std::move(indexBuffer)),
      m_indexFormat(indexFormat),
      m_indexCount(indexCount),
      m_vertexStride(vertexStride) {}

std::shared_ptr<Mesh> Mesh::createIndexedU16Immutable(ID3D11Device* device, const void* vertexData,
                                                       std::uint32_t vertexDataSize, std::uint32_t vertexStride,
                                                       const std::uint16_t* indexData, std::uint32_t indexCount) {
    if (device == nullptr || vertexData == nullptr || vertexDataSize == 0 || vertexStride == 0 || indexData == nullptr ||
        indexCount == 0) {
        return nullptr;
    }

    D3D11_BUFFER_DESC vertexBufferDesc{};
    vertexBufferDesc.ByteWidth = vertexDataSize;
    vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexDataDesc{};
    vertexDataDesc.pSysMem = vertexData;

    ComPtr<ID3D11Buffer> vertexBuffer;
    HRESULT result = device->CreateBuffer(&vertexBufferDesc, &vertexDataDesc, vertexBuffer.GetAddressOf());
    if (FAILED(result)) {
        return nullptr;
    }

    D3D11_BUFFER_DESC indexBufferDesc{};
    indexBufferDesc.ByteWidth = static_cast<UINT>(indexCount * sizeof(std::uint16_t));
    indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexDataDesc{};
    indexDataDesc.pSysMem = indexData;

    ComPtr<ID3D11Buffer> indexBuffer;
    result = device->CreateBuffer(&indexBufferDesc, &indexDataDesc, indexBuffer.GetAddressOf());
    if (FAILED(result)) {
        return nullptr;
    }

    return std::shared_ptr<Mesh>(
        new Mesh(std::move(vertexBuffer), std::move(indexBuffer), DXGI_FORMAT_R16_UINT, indexCount, vertexStride));
}

bool Mesh::isValid() const { return m_vertexBuffer != nullptr && m_indexBuffer != nullptr && m_indexCount > 0; }

ID3D11Buffer* Mesh::vertexBuffer() const { return m_vertexBuffer.Get(); }

ID3D11Buffer* Mesh::indexBuffer() const { return m_indexBuffer.Get(); }

DXGI_FORMAT Mesh::indexFormat() const { return m_indexFormat; }

std::uint32_t Mesh::indexCount() const { return m_indexCount; }

std::uint32_t Mesh::vertexStride() const { return m_vertexStride; }
