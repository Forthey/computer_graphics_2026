cbuffer ObjectBuffer : register(b0) {
    row_major float4x4 modelMatrix;
};

cbuffer SceneBuffer : register(b1) {
    row_major float4x4 viewProjectionMatrix;
};

struct VSInput {
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), modelMatrix), viewProjectionMatrix);
    output.color = input.color;
    return output;
}
