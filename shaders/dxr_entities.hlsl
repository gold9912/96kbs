struct RayFrameConstants {
    row_major float4x4 invViewProj;
    float3 cameraPosition;
    float timeSeconds;
    uint frameIndex;
    uint materialCount;
    uint outputWidth;
    uint outputHeight;
};

struct EntityMaterial {
    float3 baseColor;
    float emission;
};

struct TriangleMetadata {
    float3 normal;
    uint materialId;
};

struct RayPayload {
    float3 radiance;
    uint hitKind;
};

RWTexture2D<float4> gOutput : register(u0);
RaytracingAccelerationStructure gScene : register(t0);
StructuredBuffer<EntityMaterial> gMaterials : register(t1);
StructuredBuffer<TriangleMetadata> gTriangles : register(t2);
ConstantBuffer<RayFrameConstants> gFrame : register(b0);

[shader("raygeneration")]
void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dims = DispatchRaysDimensions().xy;
    float2 screenUv = (float2(pixel) + 0.5) / max(float2(dims), 1.0);
    float2 ndc = float2(screenUv.x * 2.0 - 1.0, 1.0 - screenUv.y * 2.0);
    float4 nearH = mul(float4(ndc, 0.0, 1.0), gFrame.invViewProj);
    float4 farH = mul(float4(ndc, 1.0, 1.0), gFrame.invViewProj);
    float3 nearWorld = nearH.xyz / nearH.w;
    float3 farWorld = farH.xyz / farH.w;

    RayDesc ray;
    ray.Origin = gFrame.cameraPosition;
    ray.Direction = normalize(farWorld - gFrame.cameraPosition);
    ray.TMin = 0.001;
    ray.TMax = 1000.0;

    RayPayload payload;
    payload.radiance = float3(0.0, 0.0, 0.0);
    payload.hitKind = 0;
    TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, ray, payload);

    float3 fog = float3(0.014, 0.010, 0.016);
    float pulse = 0.5 + 0.5 * sin(gFrame.timeSeconds * 2.7);
    float vignette = saturate(1.0 - length(screenUv * 2.0 - 1.0) * 0.65);
    float3 color = payload.hitKind != 0 ? payload.radiance : fog + float3(0.014, 0.004, 0.009) * pulse * vignette;
    float alpha = payload.hitKind != 0 ? 0.86 : 0.0;
    gOutput[pixel] = float4(color, alpha);
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    payload.radiance = float3(0.0, 0.0, 0.0);
    payload.hitKind = 0;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs) {
    float edge = saturate(1.0 - min(min(attribs.barycentrics.x, attribs.barycentrics.y),
                                    1.0 - attribs.barycentrics.x - attribs.barycentrics.y) * 18.0);
    TriangleMetadata triMeta = gTriangles[PrimitiveIndex()];
    uint materialIndex = min(triMeta.materialId, max(gFrame.materialCount, 1u) - 1u);
    EntityMaterial material = gMaterials[materialIndex];
    float3 normal = normalize(triMeta.normal);
    float3 lightDir = normalize(float3(-0.35, 0.92, -0.28));
    float diffuse = 0.35 + 0.65 * saturate(dot(normal, lightDir));
    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float pulse = 0.5 + 0.5 * sin(gFrame.timeSeconds * 4.0 + dot(hitPos.xz, float2(0.19, 0.13)));
    float glow = material.emission * (0.72 + 0.28 * pulse);
    float wire = edge * (0.18 + material.emission * 0.75);
    float2 tile = abs(frac(hitPos.xz * 0.55) - 0.5);
    float grid = 1.0 - smoothstep(0.018, 0.055, min(tile.x, tile.y));
    float floorMask = (materialIndex == 0u || materialIndex == 9u) ? 1.0 : 0.0;
    float portalMask = materialIndex == 7u ? 1.0 : 0.0;
    float3 floorGrid = grid * floorMask * float3(0.15, 0.075, 0.065);
    float3 portalTint = portalMask * pulse * float3(0.00, 0.28, 0.20);
    payload.radiance = material.baseColor * diffuse + glow + floorGrid + portalTint + wire * float3(1.0, 0.54, 0.22);
    payload.hitKind = 1;
}
