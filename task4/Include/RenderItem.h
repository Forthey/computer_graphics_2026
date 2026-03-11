#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <DirectXMath.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <memory>

#include "Mesh.h"

class RenderItem {
public:
    virtual ~RenderItem() = default;

    virtual const std::shared_ptr<Mesh>& mesh() const = 0;
    virtual DirectX::XMMATRIX buildModelMatrix() const = 0;
};
