#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <DirectXMath.h>
#include <d3d11.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <memory>

#include "RenderItem.h"

class SkyboxRenderItem final : public RenderItem {
public:
    struct Params {
        float size = 40.0f;
    };

    SkyboxRenderItem(ID3D11Device* device, const Params& params);

    RenderItemType type() const override;
    const std::shared_ptr<Mesh>& mesh() const override;
    DirectX::XMMATRIX buildModelMatrix() const override;

private:
    std::shared_ptr<Mesh> m_mesh;
    float m_size = 40.0f;
};
