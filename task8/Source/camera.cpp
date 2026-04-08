#include "Camera.h"

#include <algorithm>
#include <cmath>

void Camera::adjustAngles(float deltaDirection, float deltaTilt) { rotate(deltaDirection, deltaTilt); }

void Camera::rotate(float deltaDirectionRadians, float deltaTiltRadians) {
    m_direction += deltaDirectionRadians;
    m_tilt += deltaTiltRadians;
    m_tilt = std::clamp(m_tilt, kMinTilt, kMaxTilt);
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

DirectX::XMFLOAT3 Camera::position() const { return DirectX::XMFLOAT3{m_positionX, m_positionY, m_positionZ}; }

void Camera::moveLocal(float forwardDelta, float rightDelta) { move(forwardDelta, rightDelta, 0.0f); }

void Camera::move(float forwardDelta, float rightDelta, float upDelta) {
    const DirectX::XMVECTOR forward = forwardVector();
    const DirectX::XMVECTOR up = DirectX::XMVectorSet(kAxisX0, kAxisY1, kAxisZ0, kAxisW0);
    const DirectX::XMVECTOR right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, forward));

    const DirectX::XMVECTOR moveForward = DirectX::XMVectorScale(forward, forwardDelta);
    const DirectX::XMVECTOR moveRight = DirectX::XMVectorScale(right, rightDelta);
    const DirectX::XMVECTOR moveUp = DirectX::XMVectorScale(up, upDelta);
    const DirectX::XMVECTOR move = DirectX::XMVectorAdd(DirectX::XMVectorAdd(moveForward, moveRight), moveUp);

    DirectX::XMFLOAT3 delta{};
    DirectX::XMStoreFloat3(&delta, move);
    m_positionX += delta.x;
    m_positionY += delta.y;
    m_positionZ += delta.z;
}

DirectX::XMVECTOR Camera::forwardVector() const {
    const float cosTilt = std::cos(m_tilt);
    return DirectX::XMVectorSet(std::sin(m_direction) * cosTilt, std::sin(m_tilt), std::cos(m_direction) * cosTilt,
                                0.0f);
}
