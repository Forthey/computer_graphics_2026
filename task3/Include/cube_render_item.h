#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <d3d11.h>
#include <DirectXMath.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <memory>

#include "render_item.h"
#include "rotation_controllable.h"

class CubeRenderItem final : public RenderItem, public IRotationControllable {
public:
    static constexpr float kDefaultSize = 1.0f;
    static constexpr float kDefaultRotationSpeed = 0.8f;
    static constexpr float kDefaultRotationOffset = 0.0f;

    struct Params {
        float size = kDefaultSize;
        float rotationSpeed = kDefaultRotationSpeed;
        float rotationOffset = kDefaultRotationOffset;
    };

    CubeRenderItem(ID3D11Device* device, const Params& params);

    const std::shared_ptr<Mesh>& mesh() const override;
    DirectX::XMMATRIX buildModelMatrix(float elapsedSec) const override;
    void toggleRotation(float elapsedSec) override;

private:
    std::shared_ptr<Mesh> m_mesh;
    float m_rotationSpeed = kDefaultRotationSpeed;
    float m_rotationOffset = kDefaultRotationOffset;
    bool m_isRotationPaused = false;
    float m_pauseStartedAtSec = kDefaultRotationOffset;
    float m_accumulatedPausedSec = kDefaultRotationOffset;
};
