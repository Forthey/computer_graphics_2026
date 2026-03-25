cbuffer ObjectBuffer : register(b0) {
    row_major float4x4 modelMatrix;
    float4 colorTint;
};

Texture2D colorTexture : register(t0);
SamplerState colorSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 worldTangent : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
    float2 uv : TEXCOORD3;
};

float4 main(PSInput input) : SV_TARGET {
    return colorTexture.Sample(colorSampler, input.uv) * colorTint;
}
