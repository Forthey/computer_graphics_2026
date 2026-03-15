#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <DirectXMath.h>
#include <d3d11.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <memory>

#include "ObjectInterfaces/AutoRotatable.h"
#include "RenderItem.h"

class CubeRenderItem final : public RenderItem, public AutoRotatable {
public:
    struct Params {
        float size = 1.0f;
        float rotationSpeed = 0.8f;
        float rotationOffset = 0.0f;
    };

    CubeRenderItem(ID3D11Device* device, const Params& params);

    RenderItemType type() const override;
    const std::shared_ptr<Mesh>& mesh() const override;
    DirectX::XMMATRIX buildModelMatrix() const override;
    void rotate(float deltaDirectionRadians, float deltaTiltRadians = 0.0f) override;
    void updateRotation(std::chrono::duration<float> deltaTime) override;
    void toggleAutoRotation() override;

private:
    std::shared_ptr<Mesh> m_mesh;
    float m_rotationSpeed = 0.8f;
    float m_rotationAngle = 0.0f;
    bool m_isAutoRotationEnabled = true;
};
