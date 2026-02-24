#pragma once

class IRotationControllable {
public:
    virtual ~IRotationControllable() = default;
    virtual void toggleRotation(float elapsedSec) = 0;
};
