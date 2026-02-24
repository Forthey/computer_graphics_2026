#include "SceneObjects/CubeRenderItem.h"

#include <algorithm>
#include <array>

namespace {
constexpr float kMinCubeSize = 0.001f;

struct Vertex {
    float position[3];
    float color[3];
};
}  // namespace

CubeRenderItem::CubeRenderItem(ID3D11Device* device, const Params& params) {
    m_rotationSpeed = params.rotationSpeed;
    m_rotationAngle = params.rotationOffset;

    const float halfSize = (std::max)(kMinCubeSize, params.size * 0.5f);

    const std::array<Vertex, 8> vertices = {
        Vertex{{-halfSize, -halfSize, -halfSize}, {1.0f, 0.2f, 0.2f}},
        Vertex{{-halfSize, halfSize, -halfSize}, {0.2f, 1.0f, 0.2f}},
        Vertex{{halfSize, halfSize, -halfSize}, {0.2f, 0.2f, 1.0f}},
        Vertex{{halfSize, -halfSize, -halfSize}, {1.0f, 1.0f, 0.2f}},
        Vertex{{-halfSize, -halfSize, halfSize}, {1.0f, 0.2f, 1.0f}},
        Vertex{{-halfSize, halfSize, halfSize}, {0.2f, 1.0f, 1.0f}},
        Vertex{{halfSize, halfSize, halfSize}, {1.0f, 0.6f, 0.2f}},
        Vertex{{halfSize, -halfSize, halfSize}, {0.6f, 0.6f, 1.0f}},
    };

    static constexpr std::uint16_t indices[] = {
        0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 4, 5, 1, 4, 1, 0, 3, 2, 6, 3, 6, 7, 1, 5, 6, 1, 6, 2, 4, 0, 3, 4, 3, 7,
    };

    m_mesh = Mesh::createIndexedU16Immutable(device, vertices.data(), static_cast<std::uint32_t>(sizeof(vertices)),
                                             static_cast<std::uint32_t>(sizeof(Vertex)), indices,
                                             static_cast<std::uint32_t>(std::size(indices)));
}

const std::shared_ptr<Mesh>& CubeRenderItem::mesh() const { return m_mesh; }

DirectX::XMMATRIX CubeRenderItem::buildModelMatrix() const { return DirectX::XMMatrixRotationY(m_rotationAngle); }

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
