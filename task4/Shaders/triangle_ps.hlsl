Texture2D colorTexture : register(t0);
SamplerState colorSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET {
    return colorTexture.Sample(colorSampler, input.uv);
}
