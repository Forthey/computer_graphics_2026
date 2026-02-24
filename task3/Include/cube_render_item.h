#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <d3d11.h>
#include <DirectXMath.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <memory>

#include "render_item.h"

class CubeRenderItem final : public RenderItem {
public:
    static constexpr float kDefaultSize = 1.0f;

    struct Params {
        float size = kDefaultSize;
    };

    CubeRenderItem(ID3D11Device* device, const Params& params);

    const std::shared_ptr<Mesh>& mesh() const override;
    DirectX::XMMATRIX buildModelMatrix(float elapsedSec) const override;

private:
    std::shared_ptr<Mesh> m_mesh;
};
