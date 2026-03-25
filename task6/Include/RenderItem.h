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

enum class RenderItemType {
    OpaqueTextured,
    TransparentTextured,
    Skybox,
};

class RenderItem {
public:
    virtual ~RenderItem() = default;

    virtual RenderItemType type() const = 0;
    virtual const std::shared_ptr<Mesh>& mesh() const = 0;
    virtual DirectX::XMMATRIX buildModelMatrix() const = 0;
    virtual DirectX::XMFLOAT4 colorTint() const = 0;
    virtual float shininess() const = 0;
    virtual DirectX::XMFLOAT3 sortPosition() const = 0;
};
