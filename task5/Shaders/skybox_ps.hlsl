TextureCube colorTexture : register(t0);
SamplerState colorSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET {
    return colorTexture.Sample(colorSampler, normalize(input.direction));
}
