struct RayFrameConstants {
    float4x4 invViewProj;
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

struct RayPayload {
    float3 radiance;
    uint hitKind;
};

RWTexture2D<float4> gOutput : register(u0);
RaytracingAccelerationStructure gScene : register(t0);
StructuredBuffer<EntityMaterial> gMaterials : register(t1);
ConstantBuffer<RayFrameConstants> gFrame : register(b0);

[shader("raygeneration")]
void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dims = DispatchRaysDimensions().xy;
    float2 uv = ((float2(pixel) + 0.5) / max(float2(dims), 1.0)) * 2.0 - 1.0;

    RayDesc ray;
    ray.Origin = gFrame.cameraPosition;
    ray.Direction = normalize(float3(uv.x, -0.85, 1.35 + uv.y * 0.15));
    ray.TMin = 0.001;
    ray.TMax = 1000.0;

    RayPayload payload;
    payload.radiance = float3(0.0, 0.0, 0.0);
    payload.hitKind = 0;
    TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, ray, payload);

    float3 fog = float3(0.018, 0.012, 0.020);
    float pulse = 0.5 + 0.5 * sin(gFrame.timeSeconds * 2.7);
    float3 color = payload.hitKind != 0 ? payload.radiance : fog + float3(0.018, 0.004, 0.010) * pulse;
    gOutput[pixel] = float4(color, 1.0);
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
    uint materialIndex = min(InstanceID(), max(gFrame.materialCount, 1u) - 1u);
    EntityMaterial material = gMaterials[materialIndex];
    payload.radiance = material.baseColor + material.emission + edge * float3(0.7, 0.25, 0.08);
    payload.hitKind = 1;
}
