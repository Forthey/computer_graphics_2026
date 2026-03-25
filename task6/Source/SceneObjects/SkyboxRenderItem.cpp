#include "SceneObjects/SkyboxRenderItem.h"

#include <algorithm>
#include <array>

namespace {
constexpr float kMinSkyboxSize = 2.0f;

struct Vertex {
    float position[3];
};
}  // namespace

SkyboxRenderItem::SkyboxRenderItem(ID3D11Device* device, const Params& params) {
    m_size = (std::max)(kMinSkyboxSize, params.size);
    constexpr float halfSize = 0.5f;

    const std::array<Vertex, 8> vertices = {
        Vertex{{-halfSize, -halfSize, -halfSize}}, Vertex{{-halfSize, halfSize, -halfSize}},
        Vertex{{halfSize, halfSize, -halfSize}},   Vertex{{halfSize, -halfSize, -halfSize}},
        Vertex{{-halfSize, -halfSize, halfSize}},  Vertex{{-halfSize, halfSize, halfSize}},
        Vertex{{halfSize, halfSize, halfSize}},    Vertex{{halfSize, -halfSize, halfSize}},
    };

    static constexpr std::uint16_t indices[] = {
        0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 4, 5, 1, 4, 1, 0, 3, 2, 6, 3, 6, 7, 1, 5, 6, 1, 6, 2, 4, 0, 3, 4, 3, 7,
    };

    m_mesh = Mesh::createIndexedU16Immutable(device, vertices.data(), static_cast<std::uint32_t>(sizeof(vertices)),
                                             static_cast<std::uint32_t>(sizeof(Vertex)), indices,
                                             static_cast<std::uint32_t>(std::size(indices)));
}

RenderItemType SkyboxRenderItem::type() const { return RenderItemType::Skybox; }

const std::shared_ptr<Mesh>& SkyboxRenderItem::mesh() const { return m_mesh; }

DirectX::XMMATRIX SkyboxRenderItem::buildModelMatrix() const {
    return DirectX::XMMatrixScaling(m_size, m_size, m_size);
}

DirectX::XMFLOAT4 SkyboxRenderItem::colorTint() const { return DirectX::XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f}; }

float SkyboxRenderItem::shininess() const { return 0.0f; }

DirectX::XMFLOAT3 SkyboxRenderItem::sortPosition() const { return DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f}; }


