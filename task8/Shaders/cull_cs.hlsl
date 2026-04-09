struct PointLight {
    float4 position;
    float4 color;
};

cbuffer SceneBuffer : register(b0) {
    row_major float4x4 viewProjectionMatrix;
    float4 cameraPosition;
    uint4 lightCount;
    PointLight lights[10];
    float4 ambientColor;
    float4 frustum[6];
};

cbuffer CullParams : register(b1) {
    uint4 numShapes;
    float4 bbMin[64];
    float4 bbMax[64];
};

RWStructuredBuffer<uint> indirectArgs : register(u0);
RWStructuredBuffer<uint> objectIds : register(u1);

bool isBoxInside(float4 planes[6], float3 boxMin, float3 boxMax) {
    [unroll]
    for (int planeIndex = 0; planeIndex < 6; ++planeIndex) {
        const float3 normal = planes[planeIndex].xyz;
        const float3 rejectVertex = float3(
            normal.x < 0.0f ? boxMax.x : boxMin.x,
            normal.y < 0.0f ? boxMax.y : boxMin.y,
            normal.z < 0.0f ? boxMax.z : boxMin.z);
        if (dot(float4(rejectVertex, 1.0f), planes[planeIndex]) > 0.0f) {
            return false;
        }
    }

    return true;
}

[numthreads(64, 1, 1)]
void main(uint3 globalThreadId : SV_DispatchThreadID) {
    if (globalThreadId.x >= numShapes.x) {
        return;
    }

    if (!isBoxInside(frustum, bbMin[globalThreadId.x].xyz, bbMax[globalThreadId.x].xyz)) {
        return;
    }

    uint visibleIndex = 0u;
    InterlockedAdd(indirectArgs[1], 1u, visibleIndex);
    objectIds[visibleIndex] = globalThreadId.x;
}
