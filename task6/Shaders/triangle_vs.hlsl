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
    float3 tangent : TANGENT;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 worldTangent : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
    float2 uv : TEXCOORD3;
};

VSOutput main(VSInput input) {
    VSOutput output;
    const float4 worldPosition = mul(float4(input.position, 1.0f), modelMatrix);
    output.position = mul(worldPosition, viewProjectionMatrix);
    output.worldPosition = worldPosition.xyz;
    output.worldTangent = mul(float4(input.tangent, 0.0f), modelMatrix).xyz;
    output.worldNormal = mul(float4(input.normal, 0.0f), modelMatrix).xyz;
    output.uv = input.uv;
    return output;
}
