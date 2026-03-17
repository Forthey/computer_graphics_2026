#pragma once

class Rotatable {
public:
    virtual ~Rotatable() = default;
    virtual void rotate(float deltaDirectionRadians, float deltaTiltRadians = 0.0f) = 0;
};
