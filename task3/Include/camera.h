#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <DirectXMath.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

class Camera {
public:
    void adjustAngles(float deltaYaw, float deltaPitch);
    DirectX::XMMATRIX buildViewMatrix() const;
    DirectX::XMMATRIX buildProjectionMatrix(float aspectRatio) const;
    void moveLocal(float forwardDelta, float rightDelta);

private:
    static constexpr float kMinPitch = -1.45f;
    static constexpr float kMaxPitch = 1.45f;
    static constexpr float kDefaultYaw = 0.0f;
    static constexpr float kDefaultPitch = 0.0f;
    static constexpr float kDefaultPositionX = 0.0f;
    static constexpr float kDefaultPositionY = 0.0f;
    static constexpr float kDefaultPositionZ = -3.0f;
    static constexpr float kDefaultFovYRadians = 1.04719755f;
    static constexpr float kDefaultNearPlane = 0.1f;
    static constexpr float kDefaultFarPlane = 100.0f;
    static constexpr float kAxisX0 = 0.0f;
    static constexpr float kAxisY1 = 1.0f;
    static constexpr float kAxisZ0 = 0.0f;
    static constexpr float kAxisW0 = 0.0f;

    DirectX::XMVECTOR forwardVector() const;

    float m_yaw = kDefaultYaw;
    float m_pitch = kDefaultPitch;
    float m_positionX = kDefaultPositionX;
    float m_positionY = kDefaultPositionY;
    float m_positionZ = kDefaultPositionZ;
    float m_fovYRadians = kDefaultFovYRadians;
    float m_nearPlane = kDefaultNearPlane;
    float m_farPlane = kDefaultFarPlane;
};
