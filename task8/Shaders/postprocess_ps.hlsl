cbuffer PostProcessBuffer : register(b0) {
    uint4 postProcessMode;
};

Texture2D sceneTexture : register(t0);
SamplerState sceneSampler : register(s0);

struct PSInput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target {
    const float3 color = sceneTexture.Sample(sceneSampler, input.uv).rgb;

    if (postProcessMode.x == 0u) {
        return float4(color, 1.0f);
    }

    if (postProcessMode.x == 1u) {
        const float grayscale = dot(color, float3(0.299f, 0.587f, 0.114f));
        return float4(grayscale.xxx, 1.0f);
    }

    const float3 sepia = float3(
        dot(color, float3(0.393f, 0.769f, 0.189f)),
        dot(color, float3(0.349f, 0.686f, 0.168f)),
        dot(color, float3(0.272f, 0.534f, 0.131f)));
    return float4(saturate(sepia), 1.0f);
}
