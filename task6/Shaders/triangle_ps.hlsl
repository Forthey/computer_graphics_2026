struct PointLight {
    float4 position;
    float4 color;
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
};

Texture2D colorTexture : register(t0);
Texture2D normalMapTexture : register(t1);
SamplerState colorSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 worldTangent : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
    float2 uv : TEXCOORD3;
};

float3 computePointLight(float3 albedo, float3 normal, float3 worldPosition) {
    float3 litColor = ambientColor.xyz * albedo;
    const float3 viewDirection = normalize(cameraPosition.xyz - worldPosition);

    [loop]
    for (uint lightIndex = 0; lightIndex < lightCount.x; ++lightIndex) {
        float3 lightVector = lights[lightIndex].position.xyz - worldPosition;
        const float lightDistanceSquared = max(dot(lightVector, lightVector), 0.0001f);
        const float lightDistance = sqrt(lightDistanceSquared);
        lightVector /= lightDistance;

        const float attenuation = 1.0f / lightDistanceSquared;
        const float diffuseFactor = max(dot(lightVector, normal), 0.0f);
        const float3 reflectedLight = reflect(-lightVector, normal);
        const float specularFactor =
            materialParams.x > 0.0f ? pow(max(dot(viewDirection, reflectedLight), 0.0f), materialParams.x) : 0.0f;

        litColor += albedo * diffuseFactor * attenuation * lights[lightIndex].color.xyz;
        litColor += albedo * specularFactor * attenuation * lights[lightIndex].color.xyz;
    }

    return litColor;
}

float3 sampleWorldNormal(PSInput input) {
    const float3 normal = normalize(input.worldNormal);
    if (materialParams.y < 0.5f) {
        return normal;
    }

    const float3 tangent = normalize(input.worldTangent);
    const float3 binormal = normalize(cross(normal, tangent));
    const float3 localNormal = normalMapTexture.Sample(colorSampler, input.uv).xyz * 2.0f - float3(1.0f, 1.0f, 1.0f);
    return normalize(localNormal.x * tangent + localNormal.y * binormal + localNormal.z * normal);
}

float4 main(PSInput input) : SV_TARGET {
    const float4 texel = colorTexture.Sample(colorSampler, input.uv);
    const float4 baseColor = texel * colorTint;
    const float3 normal = sampleWorldNormal(input);
    const float3 finalColor = computePointLight(baseColor.xyz, normal, input.worldPosition);
    return float4(finalColor, baseColor.w);
}
