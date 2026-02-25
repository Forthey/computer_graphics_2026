#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <DirectXMath.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "ObjectInterfaces/Movable.h"
#include "ObjectInterfaces/Rotatable.h"

class Camera : public Rotatable, public Movable {
public:
    void adjustAngles(float deltaDirection, float deltaTilt);
    void rotate(float deltaDirectionRadians, float deltaTiltRadians = 0.0f) override;
    DirectX::XMMATRIX buildViewMatrix() const;
    DirectX::XMMATRIX buildProjectionMatrix(float aspectRatio) const;
    void moveLocal(float forwardDelta, float rightDelta);
    void move(float forwardDelta, float rightDelta, float upDelta = 0.0f) override;

private:
    static constexpr float kMinTilt = -1.45f;
    static constexpr float kMaxTilt = 1.45f;
    static constexpr float kAxisX0 = 0.0f;
    static constexpr float kAxisY1 = 1.0f;
    static constexpr float kAxisZ0 = 0.0f;
    static constexpr float kAxisW0 = 0.0f;

    DirectX::XMVECTOR forwardVector() const;

    float m_direction = 0.0f;
    float m_tilt = 0.0f;
    float m_positionX = 0.0f;
    float m_positionY = 0.0f;
    float m_positionZ = -3.0f;
    float m_fovYRadians = 1.04719755f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 100.0f;
};
