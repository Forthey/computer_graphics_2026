struct PointLight {
    float4 position;
    float4 color;
};

struct OpaqueInstanceData {
    row_major float4x4 modelMatrix;
    row_major float4x4 normalMatrix;
    float4 colorTint;
    float4 materialParams;
};

cbuffer ObjectBuffer : register(b0) {
    row_major float4x4 modelMatrix;
    row_major float4x4 normalMatrix;
    float4 colorTint;
    float4 materialParams;
};

cbuffer SceneBuffer : register(b1) {
    row_major float4x4 viewProjectionMatrix;
    float4 cameraPosition;
    uint4 lightCount;
    PointLight lights[10];
    float4 ambientColor;
    float4 frustum[6];
};

cbuffer OpaqueInstanceBuffer : register(b2) {
    OpaqueInstanceData opaqueInstances[64];
};

StructuredBuffer<uint> visibleObjectIds : register(t0);

struct VSInput {
    float3 position : POSITION;
    float3 tangent : TANGENT;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    uint instanceId : SV_InstanceID;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 worldTangent : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
    float2 uv : TEXCOORD3;
    nointerpolation uint instanceId : TEXCOORD4;
};

VSOutput main(VSInput input) {
    VSOutput output;
    const bool useInstancing = materialParams.w > 0.5f;
    const uint objectId = useInstancing ? visibleObjectIds[input.instanceId] : 0u;
    const row_major float4x4 activeModelMatrix = useInstancing ? opaqueInstances[objectId].modelMatrix : modelMatrix;
    const row_major float4x4 activeNormalMatrix = useInstancing ? opaqueInstances[objectId].normalMatrix : normalMatrix;

    const float4 worldPosition = mul(float4(input.position, 1.0f), activeModelMatrix);
    output.position = mul(worldPosition, viewProjectionMatrix);
    output.worldPosition = worldPosition.xyz;
    output.worldTangent = mul(float4(input.tangent, 0.0f), activeNormalMatrix).xyz;
    output.worldNormal = mul(float4(input.normal, 0.0f), activeNormalMatrix).xyz;
    output.uv = input.uv;
    output.instanceId = useInstancing ? objectId : input.instanceId;
    return output;
}
