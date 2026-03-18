cbuffer ObjectBuffer : register(b0) {
    row_major float4x4 modelMatrix;
    float4 colorTint;
};

cbuffer SceneBuffer : register(b1) {
    row_major float4x4 viewProjectionMatrix;
    float4 cameraPosition;
};

struct VSInput {
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), modelMatrix), viewProjectionMatrix);
    output.uv = input.uv;
    return output;
}
