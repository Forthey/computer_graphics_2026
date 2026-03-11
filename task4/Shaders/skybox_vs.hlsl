cbuffer ObjectBuffer : register(b0) {
    row_major float4x4 modelMatrix;
};

cbuffer SceneBuffer : register(b1) {
    row_major float4x4 viewProjectionMatrix;
    float4 cameraPosition;
};

struct VSInput {
    float3 position : POSITION;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD;
};

VSOutput main(VSInput input) {
    VSOutput output;
    float4 scaledPosition = mul(float4(input.position, 0.0f), modelMatrix);
    float4 worldPosition = float4(cameraPosition.xyz + scaledPosition.xyz, 1.0f);
    output.position = mul(worldPosition, viewProjectionMatrix);
    output.direction = normalize(input.position);
    return output;
}
