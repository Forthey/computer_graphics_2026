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
        float shininess = 32.0f;
        bool useNormalMap = true;
        DirectX::XMFLOAT3 position = {0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT4 colorTint = {1.0f, 1.0f, 1.0f, 1.0f};
        RenderItemType type = RenderItemType::OpaqueTextured;
    };

    CubeRenderItem(ID3D11Device* device, const Params& params);

    RenderItemType type() const override;
    const std::shared_ptr<Mesh>& mesh() const override;
    DirectX::XMMATRIX buildModelMatrix() const override;
    DirectX::XMFLOAT4 colorTint() const override;
    float shininess() const override;
    bool useNormalMap() const override;
    DirectX::XMFLOAT3 sortPosition() const override;
    void rotate(float deltaDirectionRadians, float deltaTiltRadians = 0.0f) override;
    void updateRotation(std::chrono::duration<float> deltaTime) override;
    void toggleAutoRotation() override;

private:
    std::shared_ptr<Mesh> m_mesh;
    DirectX::XMFLOAT3 m_position = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 m_colorTint = {1.0f, 1.0f, 1.0f, 1.0f};
    RenderItemType m_type = RenderItemType::OpaqueTextured;
    float m_rotationSpeed = 0.8f;
    float m_rotationAngle = 0.0f;
    float m_shininess = 32.0f;
    bool m_useNormalMap = true;
    bool m_isAutoRotationEnabled = true;
};
