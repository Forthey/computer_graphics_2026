#pragma once

#include <chrono>

#include "ObjectInterfaces/Rotatable.h"

class AutoRotatable : public Rotatable {
public:
    virtual ~AutoRotatable() = default;
    virtual void updateRotation(std::chrono::duration<float> deltaTime) = 0;
    virtual void toggleAutoRotation() = 0;
};
