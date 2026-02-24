#include "camera.h"

#include <algorithm>
#include <cmath>

void Camera::adjustAngles(float deltaYaw, float deltaPitch) {
    m_yaw += deltaYaw;
    m_pitch += deltaPitch;
    m_pitch = std::clamp(m_pitch, kMinPitch, kMaxPitch);
}

DirectX::XMMATRIX Camera::buildViewMatrix() const {
    const DirectX::XMVECTOR eye = DirectX::XMVectorSet(m_positionX, m_positionY, m_positionZ, 1.0f);
    const DirectX::XMVECTOR at = DirectX::XMVectorAdd(eye, forwardVector());
    const DirectX::XMVECTOR up = DirectX::XMVectorSet(kAxisX0, kAxisY1, kAxisZ0, kAxisW0);
    return DirectX::XMMatrixLookAtLH(eye, at, up);
}

DirectX::XMMATRIX Camera::buildProjectionMatrix(float aspectRatio) const {
    return DirectX::XMMatrixPerspectiveFovLH(m_fovYRadians, aspectRatio, m_nearPlane, m_farPlane);
}

void Camera::moveLocal(float forwardDelta, float rightDelta) {
    const DirectX::XMVECTOR forward = forwardVector();
    const DirectX::XMVECTOR up = DirectX::XMVectorSet(kAxisX0, kAxisY1, kAxisZ0, kAxisW0);
    const DirectX::XMVECTOR right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, forward));

    const DirectX::XMVECTOR moveForward = DirectX::XMVectorScale(forward, forwardDelta);
    const DirectX::XMVECTOR moveRight = DirectX::XMVectorScale(right, rightDelta);
    const DirectX::XMVECTOR move = DirectX::XMVectorAdd(moveForward, moveRight);

    DirectX::XMFLOAT3 delta{};
    DirectX::XMStoreFloat3(&delta, move);
    m_positionX += delta.x;
    m_positionY += delta.y;
    m_positionZ += delta.z;
}

DirectX::XMVECTOR Camera::forwardVector() const {
    const float cosPitch = std::cos(m_pitch);
    return DirectX::XMVectorSet(std::sin(m_yaw) * cosPitch, std::sin(m_pitch), std::cos(m_yaw) * cosPitch, 0.0f);
}
