#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <DirectXMath.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <cmath>

class Camera {
public:
    void adjustAngles(float deltaYaw, float deltaPitch) {
        m_yaw += deltaYaw;
        m_pitch += deltaPitch;
        m_pitch = std::clamp(m_pitch, kMinPitch, kMaxPitch);
    }

    DirectX::XMMATRIX buildViewMatrix() const {
        const float cosPitch = std::cos(m_pitch);
        const DirectX::XMVECTOR eye = DirectX::XMVectorSet(std::sin(m_yaw) * cosPitch * m_distance,
                                                            std::sin(m_pitch) * m_distance,
                                                            std::cos(m_yaw) * cosPitch * m_distance, 1.0f);
        const DirectX::XMVECTOR at = DirectX::XMVectorZero();
        const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        return DirectX::XMMatrixLookAtLH(eye, at, up);
    }

    DirectX::XMMATRIX buildProjectionMatrix(float aspectRatio) const {
        return DirectX::XMMatrixPerspectiveFovLH(m_fovYRadians, aspectRatio, m_nearPlane, m_farPlane);
    }

private:
    static constexpr float kMinPitch = -1.45f;
    static constexpr float kMaxPitch = 1.45f;
    static constexpr float kDefaultYaw = 0.0f;
    static constexpr float kDefaultPitch = 0.2f;
    static constexpr float kDefaultDistance = 3.0f;
    static constexpr float kDefaultFovYRadians = 1.04719755f;
    static constexpr float kDefaultNearPlane = 0.1f;
    static constexpr float kDefaultFarPlane = 100.0f;

    float m_yaw = kDefaultYaw;
    float m_pitch = kDefaultPitch;
    float m_distance = kDefaultDistance;
    float m_fovYRadians = kDefaultFovYRadians;
    float m_nearPlane = kDefaultNearPlane;
    float m_farPlane = kDefaultFarPlane;
};
