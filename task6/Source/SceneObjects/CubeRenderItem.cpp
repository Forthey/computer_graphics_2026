#include "SceneObjects/CubeRenderItem.h"

#include <algorithm>
#include <array>

namespace {
constexpr float kMinCubeSize = 0.001f;

struct Vertex {
    float position[3];
    float tangent[3];
    float normal[3];
    float uv[2];
};
}  // namespace

CubeRenderItem::CubeRenderItem(ID3D11Device* device, const Params& params) {
    m_position = params.position;
    m_colorTint = params.colorTint;
    m_type = params.type;
    m_rotationSpeed = params.rotationSpeed;
    m_rotationAngle = params.rotationOffset;
    m_shininess = params.shininess;

    const float halfSize = (std::max)(kMinCubeSize, params.size * 0.5f);

    const std::array<Vertex, 24> vertices = {
        Vertex{{-halfSize, -halfSize, halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        Vertex{{halfSize, -halfSize, halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        Vertex{{halfSize, halfSize, halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        Vertex{{-halfSize, halfSize, halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        Vertex{{halfSize, -halfSize, -halfSize}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        Vertex{{-halfSize, -halfSize, -halfSize}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        Vertex{{-halfSize, halfSize, -halfSize}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        Vertex{{halfSize, halfSize, -halfSize}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        Vertex{{-halfSize, -halfSize, -halfSize}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        Vertex{{-halfSize, -halfSize, halfSize}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        Vertex{{-halfSize, halfSize, halfSize}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        Vertex{{-halfSize, halfSize, -halfSize}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        Vertex{{halfSize, -halfSize, halfSize}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        Vertex{{halfSize, -halfSize, -halfSize}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        Vertex{{halfSize, halfSize, -halfSize}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        Vertex{{halfSize, halfSize, halfSize}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        Vertex{{-halfSize, halfSize, halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        Vertex{{halfSize, halfSize, halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        Vertex{{halfSize, halfSize, -halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        Vertex{{-halfSize, halfSize, -halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        Vertex{{-halfSize, -halfSize, -halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        Vertex{{halfSize, -halfSize, -halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        Vertex{{halfSize, -halfSize, halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        Vertex{{-halfSize, -halfSize, halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
    };

    static constexpr std::uint16_t indices[] = {
        0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
        12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    };

    m_mesh = Mesh::createIndexedU16Immutable(device, vertices.data(), static_cast<std::uint32_t>(sizeof(vertices)),
                                             static_cast<std::uint32_t>(sizeof(Vertex)), indices,
                                             static_cast<std::uint32_t>(std::size(indices)));
}

RenderItemType CubeRenderItem::type() const { return m_type; }

const std::shared_ptr<Mesh>& CubeRenderItem::mesh() const { return m_mesh; }

DirectX::XMMATRIX CubeRenderItem::buildModelMatrix() const {
    const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationY(m_rotationAngle);
    const DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
    return DirectX::XMMatrixMultiply(rotation, translation);
}

DirectX::XMFLOAT4 CubeRenderItem::colorTint() const { return m_colorTint; }

float CubeRenderItem::shininess() const { return m_shininess; }

DirectX::XMFLOAT3 CubeRenderItem::sortPosition() const { return m_position; }

void CubeRenderItem::rotate(float deltaDirectionRadians, float /*deltaTiltRadians*/) {
    m_rotationAngle += deltaDirectionRadians;
}

void CubeRenderItem::updateRotation(std::chrono::duration<float> deltaTime) {
    if (!m_isAutoRotationEnabled) {
        return;
    }

    rotate(m_rotationSpeed * deltaTime.count(), 0.0f);
}

void CubeRenderItem::toggleAutoRotation() { m_isAutoRotationEnabled = !m_isAutoRotationEnabled; }


