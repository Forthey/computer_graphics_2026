#pragma once

class Movable {
public:
    virtual ~Movable() = default;
    virtual void move(float forwardDelta, float rightDelta, float upDelta = 0.0f) = 0;
};
