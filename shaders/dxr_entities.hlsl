struct RayFrameConstants {
    row_major float4x4 invViewProj;
    float3 cameraPosition;
    float timeSeconds;
    uint frameIndex;
    uint materialCount;
    uint outputWidth;
    uint outputHeight;
    uint displayWidth;
    uint displayHeight;
    uint visualStyleIdentity;
    uint visualStyleSurface;
    uint visualStyleAtmosphere;
    uint visualStyleVariant;
    uint shotLayoutIdentity;
    uint shotLayoutWeights;
    uint renderQuality;
    uint reserved0;
};

struct EntityMaterial {
    float3 baseColor;
    float emission;
};

struct TriangleMetadata {
    float3 normal;
    uint materialId;
    uint styleTag;
    uint reserved0;
    uint reserved1;
    uint reserved2;
};

struct RayPayload {
    float3 radiance;
    uint hitKind;
    uint depth;
};

RWTexture2D<float4> gOutput : register(u0);
RaytracingAccelerationStructure gScene : register(t0);
StructuredBuffer<EntityMaterial> gMaterials : register(t1);
StructuredBuffer<TriangleMetadata> gTriangles : register(t2);
ConstantBuffer<RayFrameConstants> gFrame : register(b0);

float hash21(float2 p) {
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float valueNoise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + float2(1.0, 0.0));
    float c = hash21(i + float2(0.0, 1.0));
    float d = hash21(i + float2(1.0, 1.0));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float fbm2(float2 p) {
    float v = 0.0;
    float a = 0.52;
    for (int i = 0; i < 4; ++i) {
        v += valueNoise(p) * a;
        p = p * 2.03 + float2(17.7, 9.2);
        a *= 0.50;
    }
    return saturate(v);
}

float3 orthogonalUnit(float3 n) {
    return normalize(abs(n.y) < 0.82 ? cross(n, float3(0.0, 1.0, 0.0)) : cross(n, float3(1.0, 0.0, 0.0)));
}

float styleWeight(uint packedValue, uint shift) {
    return (float)((packedValue >> shift) & 15u) / 15.0;
}

uint styleBiome() {
    return gFrame.visualStyleIdentity & 3u;
}

uint renderQuality() {
    return min(gFrame.renderQuality, 5u);
}

float shotWeight(uint shift) {
    return (float)((gFrame.shotLayoutWeights >> shift) & 15u) / 15.0;
}

uint styleTagNibble(uint tag, uint shift) {
    return (tag >> shift) & 15u;
}

float3 styleFogColor(uint biome) {
    if (biome == 0u) {
        return float3(0.034, 0.038, 0.034);
    }
    if (biome == 1u) {
        return float3(0.018, 0.038, 0.030);
    }
    if (biome == 2u) {
        return float3(0.040, 0.024, 0.060);
    }
    return float3(0.014, 0.018, 0.045);
}

float3 styleLightColor(uint biome) {
    if (biome == 0u) {
        return float3(1.00, 0.78, 0.48);
    }
    if (biome == 1u) {
        return float3(0.92, 0.76, 0.52);
    }
    if (biome == 2u) {
        return float3(1.00, 0.45, 0.18);
    }
    return float3(0.54, 0.20, 1.00);
}

float3 styleMossColor(uint biome) {
    return biome == 1u ? float3(0.038, 0.150, 0.062) : float3(0.040, 0.118, 0.052);
}

float3 styleCrackColor(uint biome) {
    if (biome == 3u) {
        return float3(1.00, 0.08, 0.16);
    }
    if (biome == 2u) {
        return float3(0.70, 0.24, 1.00);
    }
    return float3(0.10, 0.07, 0.05);
}

float2 pathRand2(float3 p, uint depth, uint sampleIndex) {
    float salt = (float)((gFrame.visualStyleVariant & 1023u) + depth * 37u + sampleIndex * 131u);
    float a = hash21(p.xz * (0.119 + 0.011 * (float)(sampleIndex + 1u)) + float2(p.y * 0.271 + salt, salt * 0.618));
    float b = hash21(float2(p.z * 0.173 + p.y * 0.097, p.x * 0.151 - salt * 0.347) + a);
    return frac(float2(a, b) + float2(0.071, 0.611) * (float)sampleIndex);
}

float3 cosineHemisphere(float3 normal, float2 u) {
    float phi = u.x * 6.2831853;
    float sinTheta = sqrt(saturate(u.y));
    float cosTheta = sqrt(saturate(1.0 - u.y));
    float3 tangent = orthogonalUnit(normal);
    float3 bitangent = normalize(cross(normal, tangent));
    return normalize(tangent * cos(phi) * sinTheta + bitangent * sin(phi) * sinTheta + normal * cosTheta);
}

float3 jitteredDirection(float3 direction, float3 normal, float2 u, float spread) {
    float3 tangent = orthogonalUnit(direction);
    float3 bitangent = normalize(cross(direction, tangent));
    return normalize(direction +
        tangent * (u.x - 0.5) * spread +
        bitangent * (u.y - 0.5) * spread +
        normal * spread * 0.10);
}

float3 tracePathRadiance(float3 origin, float3 direction, float tMax, uint depth) {
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = normalize(direction);
    ray.TMin = 0.035;
    ray.TMax = tMax;

    RayPayload tracedPayload;
    tracedPayload.radiance = float3(0.0, 0.0, 0.0);
    tracedPayload.hitKind = 0;
    tracedPayload.depth = depth;
    TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, ray, tracedPayload);
    return tracedPayload.radiance;
}

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
    payload.depth = 0;
    TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, ray, payload);

    uint biome = styleBiome();
    float fogWeight = styleWeight(gFrame.visualStyleAtmosphere, 8u);
    float glowWeight = styleWeight(gFrame.visualStyleAtmosphere, 4u);
    float3 fog = styleFogColor(biome) * (0.10 + fogWeight * 0.28);
    float pulse = 0.5 + 0.5 * sin(gFrame.timeSeconds * 2.7);
    float vignette = saturate(1.0 - length(screenUv * 2.0 - 1.0) * 0.65);
    float3 missAtmosphere = payload.radiance + fog * 0.78 + styleLightColor(biome) * (0.010 + glowWeight * pulse * vignette * 0.024);
    float3 color = payload.hitKind != 0 ? payload.radiance : missAtmosphere;
    float alpha = payload.hitKind != 0 ? 0.97 : 0.68;
    gOutput[pixel] = float4(color, alpha);
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    uint biome = styleBiome();
    float glowWeight = styleWeight(gFrame.visualStyleAtmosphere, 4u);
    float fogWeight = styleWeight(gFrame.visualStyleAtmosphere, 8u);
    float descent = styleWeight(gFrame.visualStyleAtmosphere, 12u);
    float3 rd = normalize(WorldRayDirection());
    float up = saturate(rd.y * 0.55 + 0.45);
    float shafts = smoothstep(0.90, 0.20, abs(rd.x * 0.62 + rd.z * 0.38 + rd.y * 0.20));
    float hazeNoise = fbm2(rd.xz * 2.4 + float2(gFrame.timeSeconds * 0.015, (float)(gFrame.visualStyleVariant & 31u)));
    float3 baseFog = styleFogColor(biome) * (0.32 + fogWeight * 0.46 + up * 0.20);
    float3 source = styleLightColor(biome) * (0.028 + glowWeight * 0.048) * shafts * (0.55 + hazeNoise * 0.45);
    float3 abyssTint = float3(0.025, 0.010, 0.020) * descent * (0.40 + fogWeight * 0.50);
    float ptLift = renderQuality() >= 5u ? 1.0 : 0.0;
    payload.radiance = baseFog * (1.0 + ptLift * 0.34) + source * (1.0 + ptLift * 2.20) + abyssTint;
    payload.hitKind = 0;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs) {
    if (payload.depth >= 16u) {
        payload.radiance = float3(0.0, 0.0, 0.0);
        payload.hitKind = 1;
        return;
    }

    float edge = saturate(1.0 - min(min(attribs.barycentrics.x, attribs.barycentrics.y),
                                    1.0 - attribs.barycentrics.x - attribs.barycentrics.y) * 18.0);
    TriangleMetadata triMeta = gTriangles[PrimitiveIndex()];
    uint materialIndex = min(triMeta.materialId, max(gFrame.materialCount, 1u) - 1u);
    EntityMaterial material = gMaterials[materialIndex];
    float3 normal = normalize(triMeta.normal);
    if (dot(normal, WorldRayDirection()) > 0.0) {
        normal = -normal;
    }
    uint biome = styleBiome();
    float moss = styleWeight(gFrame.visualStyleSurface, 0u);
    float wetness = styleWeight(gFrame.visualStyleSurface, 4u);
    float cracks = styleWeight(gFrame.visualStyleSurface, 8u);
    float decay = styleWeight(gFrame.visualStyleSurface, 12u);
    float corruption = styleWeight(gFrame.visualStyleAtmosphere, 0u);
    float glowWeight = styleWeight(gFrame.visualStyleAtmosphere, 4u);
    float fogWeight = styleWeight(gFrame.visualStyleAtmosphere, 8u);
    float descent = styleWeight(gFrame.visualStyleAtmosphere, 12u);
    float shotClear = shotWeight(0u);
    float shotEdge = shotWeight(4u);
    float shotFoliage = shotWeight(8u);
    float shotHero = shotWeight(12u);
    float shotContrast = shotWeight(16u);
    uint tagRole = styleTagNibble(triMeta.styleTag, 0u);
    uint tagElement = styleTagNibble(triMeta.styleTag, 4u);
    uint tagSurface = styleTagNibble(triMeta.styleTag, 8u);
    float tagEmissive = styleTagNibble(triMeta.styleTag, 12u) != 0u ? 1.0 : 0.0;
    uint rtQuality = renderQuality();
    float3 lightDir = normalize(biome == 3u ? float3(-0.18, 0.78, -0.58) : float3(-0.52 - shotContrast * 0.08, 0.86, -0.30 - shotEdge * 0.06));
    float3 fillDir = normalize(float3(0.62, 0.55 + shotClear * 0.05, 0.38));
    float diffuse = 0.045 + 1.24 * saturate(dot(normal, lightDir));
    float openSky = biome <= 1u ? saturate(normal.y * 0.50 + 0.50) : 0.0;
    float fill = (0.020 + descent * 0.038 + (biome <= 1u ? 0.028 : 0.006)) * saturate(dot(normal, fillDir)) +
        descent * 0.018 + openSky * (0.022 + glowWeight * 0.010);
    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float pulse = 0.5 + 0.5 * sin(gFrame.timeSeconds * 4.0 + dot(hitPos.xz, float2(0.19, 0.13)));
    float lowFloorEmissive = smoothstep(0.13, 0.02, abs(hitPos.y)) * smoothstep(0.20, 0.48, material.emission);
    float glow = material.emission * (0.68 + 0.42 * pulse) * (0.98 + glowWeight * 0.46) * (1.0 - lowFloorEmissive * 0.24);
    float3 emissiveGlow = material.baseColor * glow * (1.02 + material.emission * 0.36 + tagEmissive * shotHero * 0.18);
    float floorMask = (materialIndex == 0u || materialIndex == 9u) ? 1.0 : 0.0;
    float wallMask = materialIndex == 1u ? 1.0 : 0.0;
    float foliageMask = max(materialIndex == 12u ? 1.0 : 0.0, tagRole == 2u && tagElement == 6u && tagSurface == 1u ? shotFoliage * 0.12 : 0.0);
    float portalMask = materialIndex == 7u ? 1.0 : 0.0;
    float playerMask = materialIndex == 2u ? 1.0 : 0.0;
    float bladeMask = materialIndex == 3u ? 1.0 : 0.0;
    float bruteMask = materialIndex == 4u ? 1.0 : 0.0;
    float casterMask = materialIndex == 5u ? 1.0 : 0.0;
    float projectileMask = materialIndex == 6u ? 1.0 : 0.0;
    float controlMask = materialIndex == 8u ? 1.0 : 0.0;
    float sparkMask = materialIndex == 10u ? 1.0 : 0.0;
    float runeMask = materialIndex == 11u ? 1.0 : 0.0;
    float boneMask = materialIndex == 13u ? 1.0 : 0.0;
    float bossMask = materialIndex == 14u ? 1.0 : 0.0;
    float characterMask = saturate(playerMask + bladeMask + bruteMask + casterMask + projectileMask + controlMask + sparkMask + runeMask + foliageMask + boneMask + bossMask + (tagRole == 4u ? 1.0 : 0.0));
    float nonPlayerCharacterMask = saturate(characterMask - playerMask);
    float armorMask = saturate(playerMask + bladeMask + bruteMask + projectileMask + boneMask + bossMask);
    float clothMask = saturate(playerMask + casterMask + runeMask + bossMask);
    float emissiveMask = max(smoothstep(0.18, 0.72, material.emission), tagEmissive * 0.55);
    float nonCharacterEmission = emissiveMask * (1.0 - characterMask);
    float readableSparkEdge = edge * saturate(sparkMask + portalMask + runeMask) * 0.006;
    float wire = edge * (floorMask * 0.0012 + wallMask * 0.0015 + nonCharacterEmission * 0.014) + readableSparkEdge;
    float variantA = (float)(gFrame.visualStyleVariant & 255u) * 0.017;
    float variantB = (float)((gFrame.visualStyleVariant >> 8u) & 255u) * 0.013;
    float2 stoneP = hitPos.xz;
    float2 slab = stoneP * (0.45 + (float)((gFrame.visualStyleIdentity >> 8u) & 3u) * 0.045) + variantA;
    float2 slabLocal = frac(slab);
    float2 slabEdgeDist = min(slabLocal, 1.0 - slabLocal);
    float slabEdge = 1.0 - smoothstep(0.010, 0.050, min(slabEdgeDist.x, slabEdgeDist.y));
    float stoneGrain = fbm2(stoneP * 0.86 + variantA);
    float fineGrain = fbm2(stoneP * 2.65 + variantB);
    float wallGrain = fbm2(float2(hitPos.x * 0.74 + hitPos.y * 1.80, hitPos.z * 0.66) + variantB);
    float wallRibs = 1.0 - smoothstep(0.015, 0.060, min(frac(hitPos.x * 0.42 + variantA), 1.0 - frac(hitPos.x * 0.42 + variantA)));
    float wallMortar = 1.0 - smoothstep(0.018, 0.075, min(frac(hitPos.y * 1.72 + variantB), 1.0 - frac(hitPos.y * 1.72 + variantB)));
    float wallSoot = smoothstep(0.48, 0.90, fbm2(float2(hitPos.x * 0.36 + hitPos.z * 0.24, hitPos.y * 0.86) + variantA));
    float mossNoise = smoothstep(0.50, 0.86, fbm2(stoneP * 0.42 + float2(variantA, 31.0)));
    float wetNoise = smoothstep(0.54, 0.92, fbm2(stoneP * 0.58 + float2(19.0, variantB)));
    float veinA = abs(frac(stoneP.x * 0.18 + stoneP.y * 0.31 + variantB) - 0.5);
    float veinB = abs(frac(stoneP.x * -0.27 + stoneP.y * 0.16 + variantA) - 0.5);
    float crackNoise = (1.0 - smoothstep(0.010, 0.040 + cracks * 0.018, min(veinA, veinB))) *
        smoothstep(0.44, 0.84, fbm2(stoneP * 1.15 + variantB));
    float floorBumpX = (fbm2(stoneP * 1.05 + float2(0.19 + variantA, 7.0)) - stoneGrain) * floorMask;
    float floorBumpZ = (fbm2(stoneP * 1.05 + float2(5.0, 0.23 + variantB)) - stoneGrain) * floorMask;
    float wallBumpA = (wallGrain - 0.5) * wallMask;
    float wallBumpB = (fbm2(float2(hitPos.z * 0.58 + hitPos.y * 1.22, hitPos.x * 0.32) + variantA) - 0.5) * wallMask;
    float charGrain = fbm2(float2(hitPos.x * 3.2 + hitPos.y * 1.4, hitPos.z * 3.6 + variantA));
    float charRidge = 1.0 - smoothstep(0.012, 0.070, abs(frac(hitPos.y * (4.4 + armorMask * 2.0) + hitPos.x * 0.20 + variantB) - 0.5));
    float charBumpX = (charGrain - 0.5) * characterMask * (0.042 + armorMask * 0.032);
    float charBumpY = (charRidge - 0.5) * characterMask * (0.012 + clothMask * 0.022);
    normal = normalize(normal + float3(
        floorBumpX * 0.55 + wallBumpA * 0.12 + charBumpX,
        wallBumpB * 0.070 + charBumpY,
        floorBumpZ * 0.55 + wallBumpB * 0.10 + charBumpX * 0.70));
    diffuse = 0.045 + 1.24 * saturate(dot(normal, lightDir));
    openSky = biome <= 1u ? saturate(normal.y * 0.50 + 0.50) : 0.0;
    fill = (0.020 + descent * 0.038 + (biome <= 1u ? 0.028 : 0.006)) * saturate(dot(normal, fillDir)) +
        descent * 0.018 + openSky * (0.022 + glowWeight * 0.010);
    float shadow = 1.0;
    if (payload.depth == 0u && rtQuality < 5u && material.emission < 0.62) {
        float3 lightTangent = orthogonalUnit(lightDir);
        float3 lightBitangent = normalize(cross(lightDir, lightTangent));
        float stableSeed = hash21(hitPos.xz * 0.137 + float2(hitPos.y * 0.41 + variantA, variantB));
        uint shadowSamples = rtQuality >= 5u ? 6u : (rtQuality >= 4u ? 4u : (rtQuality >= 3u ? 3u : 1u));
        float shadowSum = 0.0;
        for (uint si = 0u; si < 6u; ++si) {
            if (si >= shadowSamples) {
                break;
            }
            float sampleAngle = (stableSeed + (float)si * 0.381966) * 6.2831853;
            float sampleRadius = rtQuality >= 4u
                ? (0.038 + glowWeight * 0.026 + (float)si * 0.012)
                : (rtQuality >= 3u ? (0.026 + glowWeight * 0.018 + (float)si * 0.016) : 0.0);
            float3 sampleLightDir = normalize(
                lightDir +
                lightTangent * cos(sampleAngle) * sampleRadius +
                lightBitangent * sin(sampleAngle) * sampleRadius);

            RayDesc shadowRay;
            shadowRay.Origin = hitPos + normal * 0.035 + sampleLightDir * 0.020;
            shadowRay.Direction = sampleLightDir;
            shadowRay.TMin = 0.035;
            shadowRay.TMax = rtQuality >= 5u ? 42.0 : (rtQuality >= 4u ? 34.0 : (rtQuality >= 3u ? 24.0 : 18.0));

            RayPayload shadowPayload;
            shadowPayload.radiance = float3(0.0, 0.0, 0.0);
            shadowPayload.hitKind = 0;
            shadowPayload.depth = 16u;
            TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 1, 0, shadowRay, shadowPayload);
            shadowSum += shadowPayload.hitKind != 0u ? (biome <= 1u ? 0.42 : 0.30) : 1.0;
        }
        shadow = shadowSum / max((float)shadowSamples, 1.0);
    }

    float contact = 1.0;
    if (payload.depth == 0u && rtQuality < 5u && material.emission < 0.45) {
        float3 tangent = orthogonalUnit(normal);
        float3 aoDir = floorMask > 0.5
            ? normalize(normal * 0.48 - lightDir * 0.46 + fillDir * 0.18)
            : normalize(normal * 0.58 + tangent * 0.28 + fillDir * 0.24);

        RayDesc contactRay;
        contactRay.Origin = hitPos + normal * 0.045;
        contactRay.Direction = aoDir;
        contactRay.TMin = 0.040;
        contactRay.TMax = floorMask > 0.5 ? 1.45 : 1.10;

        RayPayload contactPayload;
        contactPayload.radiance = float3(0.0, 0.0, 0.0);
        contactPayload.hitKind = 0;
        contactPayload.depth = 16u;
        TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 1, 0, contactRay, contactPayload);
        contact = contactPayload.hitKind != 0u
            ? (floorMask > 0.5 ? (biome <= 1u ? 0.48 : 0.42) : (biome <= 1u ? 0.56 : 0.50))
            : 1.0;
    }

    float3 base = material.baseColor;
    base *= lerp(1.0, 0.56 + stoneGrain * 0.46 + fineGrain * 0.20, floorMask);
    base *= lerp(1.0, 0.46 + wallGrain * 0.28, wallMask);
    float tileCell = valueNoise(floor(stoneP * (0.24 + (float)((gFrame.visualStyleIdentity >> 8u) & 3u) * 0.035)) + float2(variantA, variantB));
    float tileTone = 0.58 + tileCell * 0.52 + fineGrain * 0.14;
    float3 tileWarmth = biome <= 1u ? float3(0.030, 0.026, 0.014) :
        (biome == 2u ? float3(0.024, 0.010, 0.014) : float3(0.012, 0.010, 0.024));
    base = lerp(base, base * tileTone + tileWarmth * (tileCell - 0.30), floorMask * 0.68);
    base = lerp(base, base + styleLightColor(biome) * 0.014, wallMask * wallRibs * (0.18 + glowWeight * 0.18));
    base = lerp(base, base * 0.56, wallMask * wallMortar * (0.20 + decay * 0.18));
    base = lerp(base, base * 0.58 + styleFogColor(biome) * 0.20, wallMask * wallSoot * (0.18 + descent * 0.16));
    float wallRuneA = 1.0 - smoothstep(0.008, 0.046, abs(frac(hitPos.x * 0.34 + hitPos.y * 0.78 + variantA) - 0.50));
    float wallRuneB = 1.0 - smoothstep(0.010, 0.052, abs(frac(hitPos.z * 0.30 - hitPos.y * 0.62 + variantB) - 0.50));
    float wallRuneMask = wallMask * smoothstep(0.44, 0.88, wallSoot + glowWeight * 0.20) * saturate(wallRuneA * 0.42 + wallRuneB * 0.36);
    base += styleLightColor(biome) * wallRuneMask * (0.012 + glowWeight * 0.026 + descent * 0.014);
    base = lerp(base, styleMossColor(biome) * 1.14 + float3(0.004, 0.020, 0.004), floorMask * moss * mossNoise * 0.38);
    base = lerp(base, styleMossColor(biome) * 0.72, wallMask * moss * mossNoise * 0.10);
    float leafFine = fbm2(float2(hitPos.x * 3.1 + hitPos.y * 0.8, hitPos.z * 3.4 + variantA));
    float leafVein = 1.0 - smoothstep(0.010, 0.060, abs(frac(hitPos.x * 1.72 - hitPos.z * 1.16 + variantB) - 0.5));
    float3 leafColor = lerp(styleMossColor(biome) * 0.72, styleMossColor(biome) * 1.10 + float3(0.012, 0.026, 0.010), leafFine);
    base = lerp(base, leafColor, foliageMask * (0.62 + moss * 0.14));
    base += foliageMask * leafVein * float3(0.006, 0.018, 0.007) * (0.13 + moss * 0.22);
    float armorFacet = smoothstep(0.56, 0.94, fbm2(float2(hitPos.x * 5.4 + hitPos.y * 1.6, hitPos.z * 4.6 + variantB)));
    float clothWeaveA = 1.0 - smoothstep(0.010, 0.050, abs(frac(hitPos.y * 6.8 + hitPos.x * 0.42 + variantA) - 0.5));
    float clothWeaveB = 1.0 - smoothstep(0.012, 0.056, abs(frac(hitPos.z * 5.2 - hitPos.x * 0.28 + variantB) - 0.5));
    float boneBands = 1.0 - smoothstep(0.016, 0.082, abs(frac(hitPos.y * 5.8 + hitPos.z * 0.23 + variantA) - 0.5));
    float runeLines = smoothstep(0.48, 0.94, charGrain) *
        (1.0 - smoothstep(0.006, 0.050, abs(frac(hitPos.x * 1.05 - hitPos.z * 0.72 + hitPos.y * 0.36 + variantB) - 0.5)));
    float battleWear = smoothstep(0.62, 0.98, fbm2(float2(hitPos.x * 7.0 - hitPos.y * 0.6, hitPos.z * 6.2 + variantA)));
    float costumePanel = smoothstep(0.50, 0.92, fbm2(float2(hitPos.x * 2.4 + hitPos.y * 3.2, hitPos.z * 2.8 + variantA * 0.7))) *
        (1.0 - smoothstep(0.020, 0.110, abs(frac(hitPos.y * 3.4 + hitPos.x * 0.20 + hitPos.z * 0.12 + variantB) - 0.5)));
    float jewelCell = smoothstep(0.76, 0.98, fbm2(float2(hitPos.x * 9.6 + hitPos.y * 1.7, hitPos.z * 8.4 + variantB)));
    float3 heroicTint = styleLightColor(biome) * (0.036 + glowWeight * 0.034) + styleFogColor(biome) * 0.044;
    base = lerp(base, base * (0.94 + charGrain * 0.18), characterMask * 0.26);
    base = lerp(base, base + float3(0.090, 0.105, 0.120) * armorFacet - float3(0.032, 0.028, 0.026) * battleWear, armorMask * 0.42);
    base = lerp(base, base * (0.80 + clothWeaveA * 0.12 + clothWeaveB * 0.08), clothMask * 0.28);
    base = lerp(base, base + heroicTint * 0.78 + float3(0.026, 0.034, 0.044) * costumePanel, nonPlayerCharacterMask * costumePanel * 0.28);
    base = lerp(base, base * 0.24 + float3(0.001, 0.012, 0.033), playerMask * 0.96);
    base = lerp(base, base * 1.02 + float3(0.004, 0.060, 0.082), playerMask * saturate(costumePanel + armorFacet) * 0.055);
    base = lerp(base, base * 0.86 + styleCrackColor(biome) * 0.095, saturate(bruteMask + bossMask) * battleWear * 0.24);
    base = lerp(base, float3(0.62, 0.70, 0.72) * (0.72 + charGrain * 0.34) + styleLightColor(biome) * 0.045, boneMask * 0.72);
    base += bladeMask * (styleLightColor(biome) * (0.082 + armorFacet * 0.085) + float3(0.052, 0.110, 0.124));
    base += casterMask * styleLightColor(biome) * runeLines * (0.072 + glowWeight * 0.112);
    base += bossMask * styleCrackColor(biome) * (runeLines * (0.115 + corruption * 0.22) + battleWear * 0.040);
    base += boneMask * boneBands * float3(0.045, 0.060, 0.060);
    emissiveGlow += (casterMask + runeMask + bossMask + sparkMask * 1.34 + bladeMask * 0.78) * styleLightColor(biome) * runeLines * (0.090 + glowWeight * 0.170 + material.emission * 0.21);
    emissiveGlow += styleLightColor(biome) * jewelCell * nonPlayerCharacterMask * smoothstep(0.12, 0.68, material.emission + glowWeight * 0.22) * 0.110;
    emissiveGlow += float3(0.08, 0.72, 0.92) * jewelCell * playerMask * smoothstep(0.10, 0.52, glowWeight * 0.22 + material.emission) * 0.014;
    base = lerp(base, base * 0.58 + float3(0.045, 0.135, 0.175), floorMask * wetness * wetNoise * 0.32);
    float tileShardA = smoothstep(0.76, 0.96, fbm2(stoneP * 1.72 + float2(variantB, 12.0)));
    float tileShardB = 1.0 - smoothstep(0.015, 0.060, abs(frac(stoneP.x * 0.41 - stoneP.y * 0.29 + variantA) - 0.5));
    float wornEdges = saturate(slabEdge * (0.38 + fineGrain * 0.44) + tileShardA * tileShardB * 0.28);
    base = lerp(base, base + float3(0.065, 0.060, 0.050), floorMask * wornEdges * (0.18 + wetness * 0.10));
    float seamDark = slabEdge * floorMask * (0.220 + cracks * 0.105 + descent * 0.055);
    base *= 1.0 - seamDark;
    base *= 1.0 - crackNoise * floorMask * (0.22 + cracks * 0.20 + descent * 0.08);
    float ageStain = smoothstep(0.58, 0.92, fbm2(stoneP * 0.22 + float2(variantB, variantA * 0.5)));
    float3 stainColor = biome <= 1u ? float3(0.10, 0.13, 0.055) :
        (biome == 2u ? float3(0.105, 0.045, 0.045) : float3(0.070, 0.020, 0.040));
    base = lerp(base, stainColor, floorMask * ageStain * (0.055 + decay * 0.085));
    float broadWear = smoothstep(0.48, 0.88, fbm2(stoneP * 0.135 + float2(variantA * 0.17, variantB * 0.21)));
    base *= 1.0 - floorMask * broadWear * (0.035 + decay * 0.050 + descent * 0.025);
    float3 floorGrid = slabEdge * floorMask * (styleLightColor(biome) * 0.003 + float3(0.004, 0.004, 0.004));
    float3 crackGlow = crackNoise * floorMask * lerp(float3(0.020, 0.017, 0.014), styleCrackColor(biome), corruption * 0.72) * (0.045 + corruption * 0.30 + descent * 0.12);
    float3 portalTint = portalMask * pulse * styleLightColor(biome) * 0.30;
    float lampCell = fbm2(floor(hitPos.xz * 0.19 + float2(variantA, variantB)) * 1.7);
    float lampBloom = floorMask * smoothstep(0.84, 0.98, lampCell) * smoothstep(1.0, 0.0, abs(hitPos.y + 0.03) * 10.0);
    float sunBand = smoothstep(0.24, 0.0, abs(frac(hitPos.x * 0.105 - hitPos.z * 0.070 + variantA) - 0.50));
    float latticeA = 1.0 - smoothstep(0.008, 0.038, abs(frac(hitPos.x * 0.30 - hitPos.z * 0.17 + variantA) - 0.50));
    float latticeB = 1.0 - smoothstep(0.010, 0.046, abs(frac(hitPos.x * 0.14 + hitPos.z * 0.31 + variantB) - 0.50));
    float windowShadow = floorMask * (biome <= 1u ? 1.0 : 0.0) * saturate(latticeA * 0.55 + latticeB * 0.34) *
        smoothstep(0.28, 0.92, sunBand) * (0.12 + decay * 0.11 + moss * 0.055) * (1.0 - descent * 0.45);
    float windowPatch = floorMask * sunBand * smoothstep(0.25, 1.0, saturate(dot(normal, float3(0.0, 1.0, 0.0)))) *
        ((biome <= 1u) ? (0.110 + glowWeight * 0.090 + shotContrast * 0.030) : (0.052 + glowWeight * 0.075));
    float3 warmShaft = lerp(float3(1.00, 0.80, 0.54), styleLightColor(biome), biome >= 2u ? 0.62 : 0.20) * windowPatch * (1.0 - descent * 0.30);
    float dampSparkle = floorMask * wetness * wetNoise * smoothstep(0.52, 0.95, fineGrain) * (0.020 + glowWeight * 0.025);
    float3 floorBounce = styleLightColor(biome) * lampBloom * (0.026 + glowWeight * 0.046 + descent * 0.018) + warmShaft * (0.82 + wetness * 0.14) +
        styleFogColor(biome) * dampSparkle;

    if (rtQuality >= 5u) {
        const uint kMaxPathDepth = 3u;
        float3 viewDirPt = normalize(-WorldRayDirection());
        float mirrorWetnessPt = floorMask * saturate(0.155 + wetness * (0.82 + wetNoise * 1.36) + descent * 0.38);
        float specularProb = saturate(
            mirrorWetnessPt * 0.72 +
            bladeMask * 0.28 +
            armorMask * 0.12 +
            wallMask * wetness * 0.055 +
            material.emission * 0.045);
        float3 surfaceEmission = emissiveGlow + crackGlow * 0.78 + portalTint + floorGrid * 0.10;
        float3 pathRadiance = surfaceEmission + floorBounce * (floorMask * 0.10 + wallMask * 0.035);
        float rimPt = pow(saturate(1.0 - dot(normal, viewDirPt)), 2.0);
        float ptFoliageBacklight = saturate(dot(normal, -lightDir) * 0.50 + 0.50) *
            (biome <= 1u ? (0.060 + moss * 0.055) : (0.018 + glowWeight * 0.026));
        float ptPatternedLight = 1.0 - windowShadow * (biome <= 1u ? 0.58 : 0.34);
        float3 ptDirect = base * ((diffuse * ptPatternedLight + fill) * (0.56 + floorMask * 0.34 + characterMask * 0.22 + wallMask * 0.12));
        ptDirect += base * floorMask * (0.050 + wetness * 0.055 + shotClear * 0.030);
        ptDirect += floorGrid * 0.18 + crackGlow * 0.22 + portalTint * 0.34 + floorBounce * (0.70 + wetness * 0.18);
        ptDirect += styleLightColor(biome) * floorMask * (windowPatch * (0.18 + shotContrast * 0.08) + dampSparkle * 0.60);
        ptDirect += foliageMask * (styleLightColor(biome) * ptFoliageBacklight + styleMossColor(biome) * 0.030);
        ptDirect += styleLightColor(biome) * nonPlayerCharacterMask * rimPt * (0.085 + material.emission * 0.075);
        ptDirect += float3(0.05, 0.54, 0.72) * playerMask * rimPt * (0.022 + glowWeight * 0.014);
        pathRadiance += ptDirect;

        if (payload.depth < kMaxPathDepth && material.emission < 0.92) {
            float3 tangentPt = orthogonalUnit(normal);
            float3 bitangentPt = normalize(cross(normal, tangentPt));
            uint sampleCount = payload.depth == 0u ? 4u : (payload.depth == 1u ? 2u : 1u);
            float3 sampleSum = float3(0.0, 0.0, 0.0);
            float sampleWeightSum = 0.0;

            [loop]
            for (uint si = 0u; si < 4u; ++si) {
                if (si >= sampleCount) {
                    break;
                }

                float2 rnd = pathRand2(hitPos + normal * 0.13, payload.depth, si);
                bool specularSample = specularProb > 0.08 && (si == 0u || (mirrorWetnessPt > 0.62 && si == 2u));
                float3 bounceDir = normal;
                float3 throughput = base;
                float sampleWeight = 1.0;

                if (specularSample) {
                    float roughness = floorMask * (0.035 + (1.0 - wetNoise) * 0.115 + descent * 0.020) +
                        characterMask * 0.060 +
                        wallMask * wetness * 0.070 +
                        0.012;
                    bounceDir = jitteredDirection(reflect(WorldRayDirection(), normal), normal, rnd, roughness);
                    throughput = lerp(base, float3(1.0, 0.96, 0.88), 0.66 + mirrorWetnessPt * 0.20);
                    sampleWeight = 0.40 + specularProb * 1.12;
                } else if (si == 1u || (payload.depth == 0u && si == 3u)) {
                    float3 windowDir = biome <= 1u ? normalize(float3(-0.46, 0.77, -0.30)) : lightDir;
                    float windowSpread = biome <= 1u ? 0.42 : 0.34;
                    bounceDir = jitteredDirection(normalize(windowDir + normal * 0.20), normal, rnd, windowSpread);
                    throughput = base * lerp(float3(0.94, 0.86, 0.72), styleLightColor(biome), biome >= 2u ? 0.62 : 0.24);
                    sampleWeight = 0.50 + saturate(dot(normal, bounceDir)) * 1.18 + glowWeight * 0.14;
                } else {
                    bounceDir = cosineHemisphere(normal, rnd);
                    throughput = base * (0.72 + charGrain * characterMask * 0.12 + wetness * floorMask * 0.08);
                    sampleWeight = 0.36 + saturate(dot(normal, bounceDir)) * 1.04;
                }

                float3 incoming = tracePathRadiance(
                    hitPos + normal * (specularSample ? 0.024 : 0.050),
                    bounceDir,
                    specularSample ? 52.0 : 38.0,
                    payload.depth + 1u);
                sampleSum += incoming * throughput * sampleWeight;
                sampleWeightSum += sampleWeight;
            }

            float3 bounced = sampleSum / max(sampleWeightSum, 0.001);
            float diffuseEnergy = 0.46 + floorMask * 0.28 + wallMask * 0.18 + characterMask * 0.34 + foliageMask * 0.22;
            pathRadiance += bounced * diffuseEnergy * (1.0 - specularProb * 0.32);
        }

        float3 viewRefl = reflect(WorldRayDirection(), normal);
        float pathSpec = pow(saturate(dot(viewRefl, lightDir)), 22.0) *
            (floorMask * (0.18 + wetness * 0.90) + characterMask * 0.050 + wallMask * wetness * 0.034);
        pathRadiance += styleLightColor(biome) * pathSpec * (0.20 + glowWeight * 0.22);
        pathRadiance += styleLightColor(biome) * rimPt * nonPlayerCharacterMask * (0.055 + material.emission * 0.060);
        pathRadiance += float3(0.04, 0.44, 0.62) * rimPt * playerMask * 0.018;
        pathRadiance += foliageMask * styleMossColor(biome) * saturate(dot(normal, -lightDir) * 0.5 + 0.5) * 0.026;

        float fogAmountPt = saturate(RayTCurrent() * (0.0035 + fogWeight * 0.0085));
        payload.radiance = lerp(pathRadiance, styleFogColor(biome) * (0.42 + fogWeight * 0.24), fogAmountPt);
        payload.hitKind = 1;
        return;
    }

    float3 pathAreaLight = float3(0.0, 0.0, 0.0);
    if (payload.depth == 0u && rtQuality >= 3u && material.emission < 0.62) {
        float3 areaBaseDir = normalize(biome <= 1u ? float3(-0.44, 0.76, -0.28) : lightDir);
        float3 areaTangent = orthogonalUnit(areaBaseDir);
        float3 areaBitangent = normalize(cross(areaBaseDir, areaTangent));
        float pathSeed = hash21(hitPos.xz * 0.091 + float2(variantB, hitPos.y * 0.27 + variantA));
        float areaVisibility = 0.0;
        float areaNdotL = 0.0;
        uint areaSamples = rtQuality >= 5u ? 5u : (rtQuality >= 4u ? 4u : 2u);
        for (uint ai = 0u; ai < 5u; ++ai) {
            if (ai >= areaSamples) {
                break;
            }
            float areaAngle = (pathSeed + (float)ai * 0.5) * 6.2831853;
            float areaRadius = (rtQuality >= 4u ? 0.105 : 0.070) + glowWeight * 0.048 + (float)ai * (rtQuality >= 4u ? 0.023 : 0.030);
            float3 areaDir = normalize(
                areaBaseDir +
                areaTangent * cos(areaAngle) * areaRadius +
                areaBitangent * sin(areaAngle) * areaRadius);

            RayDesc areaRay;
            areaRay.Origin = hitPos + normal * 0.044 + areaDir * 0.018;
            areaRay.Direction = areaDir;
            areaRay.TMin = 0.040;
            areaRay.TMax = biome <= 1u ? 30.0 : 18.0;

            RayPayload areaPayload;
            areaPayload.radiance = float3(0.0, 0.0, 0.0);
            areaPayload.hitKind = 0;
            areaPayload.depth = 16u;
            TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 1, 0, areaRay, areaPayload);
            float visible = areaPayload.hitKind == 0u ? 1.0 : 0.0;
            areaVisibility += visible;
            areaNdotL += saturate(dot(normal, areaDir)) * visible;
        }
        float invAreaSamples = 1.0 / max((float)areaSamples, 1.0);
        areaVisibility *= invAreaSamples;
        areaNdotL *= invAreaSamples;
        float windowBias = biome <= 1u ? (0.26 + glowWeight * 0.14 + moss * 0.08) : (0.17 + glowWeight * 0.20 + descent * 0.12);
        float3 areaTint = biome <= 1u ? float3(1.00, 0.80, 0.54) : styleLightColor(biome);
        pathAreaLight = (base * areaTint * areaNdotL + areaTint * areaVisibility * (floorMask * 0.032 + wallMask * 0.006 + nonPlayerCharacterMask * 0.026 + playerMask * 0.003)) *
            windowBias * (1.0 - descent * (biome <= 1u ? 0.38 : 0.05));
    }
    float foliageBacklight = foliageMask * saturate(dot(normal, -lightDir) * 0.50 + 0.50) *
        (biome <= 1u ? (0.060 + moss * 0.055) : (0.018 + glowWeight * 0.026));
    float3 reflectionColor = float3(0.0, 0.0, 0.0);
    float mirrorWetness = floorMask * saturate(0.155 + wetness * (0.82 + wetNoise * 1.36) + descent * 0.38);
    float emissiveReflection = saturate(material.emission * 0.08 + portalMask * 0.18);
    float reflectionStrength = saturate(mirrorWetness + emissiveReflection) * (rtQuality >= 1u ? 1.0 : 0.0);
    if (payload.depth == 0u && rtQuality >= 1u && reflectionStrength > 0.025) {
        RayDesc reflectionRay;
        reflectionRay.Origin = hitPos + normal * 0.018;
        reflectionRay.Direction = normalize(reflect(WorldRayDirection(), normal));
        reflectionRay.TMin = 0.025;
        reflectionRay.TMax = 34.0;

        RayPayload reflectionPayload;
        reflectionPayload.radiance = float3(0.0, 0.0, 0.0);
        reflectionPayload.hitKind = 0;
        reflectionPayload.depth = payload.depth + 1u;
        TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, reflectionRay, reflectionPayload);
        reflectionColor = reflectionPayload.radiance;
        if (rtQuality >= 3u && reflectionStrength > 0.16) {
            float3 reflTangent = orthogonalUnit(normal);
            float3 reflBitangent = normalize(cross(normal, reflTangent));
            float reflSeed = hash21(hitPos.xz * 0.173 + float2(variantA, variantB + hitPos.y * 0.19));
            float reflAngle = reflSeed * 6.2831853;
            float roughness = floorMask * (0.030 + wetNoise * 0.070 + descent * 0.020) + characterMask * 0.020;

            RayDesc roughReflectionRay;
            roughReflectionRay.Origin = hitPos + normal * 0.020;
            roughReflectionRay.Direction = normalize(reflect(WorldRayDirection(), normal) +
                reflTangent * cos(reflAngle) * roughness +
                reflBitangent * sin(reflAngle) * roughness);
            roughReflectionRay.TMin = 0.025;
            roughReflectionRay.TMax = 24.0;

            RayPayload roughReflectionPayload;
            roughReflectionPayload.radiance = float3(0.0, 0.0, 0.0);
            roughReflectionPayload.hitKind = 0;
            roughReflectionPayload.depth = payload.depth + 1u;
            TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, roughReflectionRay, roughReflectionPayload);
            reflectionColor = lerp(reflectionColor, roughReflectionPayload.radiance, floorMask * 0.34 + characterMask * 0.12);

            if (rtQuality >= 5u) {
                float reflAngleB = (reflSeed + 0.3183099) * 6.2831853;
                RayDesc roughReflectionRayB;
                roughReflectionRayB.Origin = hitPos + normal * 0.024;
                roughReflectionRayB.Direction = normalize(reflect(WorldRayDirection(), normal) +
                    reflTangent * cos(reflAngleB) * roughness * 1.55 +
                    reflBitangent * sin(reflAngleB) * roughness * 1.55);
                roughReflectionRayB.TMin = 0.025;
                roughReflectionRayB.TMax = 28.0;

                RayPayload roughReflectionPayloadB;
                roughReflectionPayloadB.radiance = float3(0.0, 0.0, 0.0);
                roughReflectionPayloadB.hitKind = 0;
                roughReflectionPayloadB.depth = payload.depth + 1u;
                TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, roughReflectionRayB, roughReflectionPayloadB);
                reflectionColor = lerp(reflectionColor, roughReflectionPayloadB.radiance, floorMask * 0.18 + characterMask * 0.07);
            }
        }
    }

    float3 indirectColor = float3(0.0, 0.0, 0.0);
    if (payload.depth == 0u && rtQuality >= 2u && material.emission < 0.42) {
        float3 giTangent = orthogonalUnit(normal);
        float3 giBitangent = normalize(cross(normal, giTangent));
        float giSeed = valueNoise(floor(hitPos.xz * 0.73 + float2(variantA, variantB)));
        float giAngle = giSeed * 6.2831853;
        float3 giDir = normalize(normal * (0.56 + wetness * 0.10) + giTangent * cos(giAngle) * 0.48 + giBitangent * sin(giAngle) * 0.48);

        RayDesc diffuseRay;
        diffuseRay.Origin = hitPos + normal * 0.045;
        diffuseRay.Direction = giDir;
        diffuseRay.TMin = 0.050;
        diffuseRay.TMax = 6.0 + glowWeight * 3.0;

        RayPayload diffusePayload;
        diffusePayload.radiance = float3(0.0, 0.0, 0.0);
        diffusePayload.hitKind = 0;
        diffusePayload.depth = payload.depth + 1u;
        TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, diffuseRay, diffusePayload);
        indirectColor = diffusePayload.radiance * (rtQuality >= 3u ? 0.70 : 1.0);
        if (rtQuality >= 3u) {
            float giAngleB = (giSeed + 0.4375) * 6.2831853;
            float3 giDirB = normalize(normal * (0.68 + wetness * 0.08) + giTangent * cos(giAngleB) * 0.36 + giBitangent * sin(giAngleB) * 0.36);

            RayDesc diffuseRayB;
            diffuseRayB.Origin = hitPos + normal * 0.050;
            diffuseRayB.Direction = giDirB;
            diffuseRayB.TMin = 0.050;
            diffuseRayB.TMax = 5.2 + glowWeight * 2.4;

            RayPayload diffusePayloadB;
            diffusePayloadB.radiance = float3(0.0, 0.0, 0.0);
            diffusePayloadB.hitKind = 0;
            diffusePayloadB.depth = payload.depth + 1u;
            TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, diffuseRayB, diffusePayloadB);
            indirectColor += diffusePayloadB.radiance * 0.52;
        }
        if (rtQuality >= 4u) {
            float giAngleC = (giSeed + 0.1875) * 6.2831853;
            float3 giDirC = normalize(normal * (0.60 + wetness * 0.08) + giTangent * cos(giAngleC) * 0.42 + giBitangent * sin(giAngleC) * 0.42);

            RayDesc diffuseRayC;
            diffuseRayC.Origin = hitPos + normal * 0.052;
            diffuseRayC.Direction = giDirC;
            diffuseRayC.TMin = 0.050;
            diffuseRayC.TMax = 7.0 + glowWeight * 3.1;

            RayPayload diffusePayloadC;
            diffusePayloadC.radiance = float3(0.0, 0.0, 0.0);
            diffusePayloadC.hitKind = 0;
            diffusePayloadC.depth = payload.depth + 1u;
            TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, diffuseRayC, diffusePayloadC);
            indirectColor += diffusePayloadC.radiance * 0.36;
        }
        if (rtQuality >= 5u) {
            float giAngleD = (giSeed + 0.6875) * 6.2831853;
            float3 giDirD = normalize(normal * (0.72 + wetness * 0.06) + giTangent * cos(giAngleD) * 0.30 + giBitangent * sin(giAngleD) * 0.30);

            RayDesc diffuseRayD;
            diffuseRayD.Origin = hitPos + normal * 0.056;
            diffuseRayD.Direction = giDirD;
            diffuseRayD.TMin = 0.050;
            diffuseRayD.TMax = 5.8 + glowWeight * 2.5;

            RayPayload diffusePayloadD;
            diffusePayloadD.radiance = float3(0.0, 0.0, 0.0);
            diffusePayloadD.hitKind = 0;
            diffusePayloadD.depth = payload.depth + 1u;
            TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, diffuseRayD, diffusePayloadD);
            indirectColor += diffusePayloadD.radiance * 0.26;
        }
    }

    float qualitySpecScale = rtQuality == 0u ? 0.55 : (rtQuality == 1u ? 0.82 : (rtQuality == 2u ? 1.02 : (rtQuality >= 5u ? 1.34 : 1.22)));
    float floorSpec = floorMask * (0.185 + wetness * 1.92) * (0.64 + wetNoise * 1.60) * pow(saturate(dot(reflect(WorldRayDirection(), normal), lightDir)), 9.0) * qualitySpecScale;
    float3 viewDir = normalize(-WorldRayDirection());
    float rim = pow(saturate(1.0 - dot(normal, viewDir)), 2.25);
    float propSpec = (wallMask * 0.036 + characterMask * (0.062 + material.emission * 0.150) + foliageMask * 0.024) *
        pow(saturate(dot(reflect(WorldRayDirection(), normal), lightDir)), 15.0) * (0.94 + glowWeight * 0.62);
    float spec = floorSpec + propSpec;
    float fogAmount = saturate(RayTCurrent() * (0.004 + fogWeight * 0.010));
    float3 materialLift = base * floorMask * (0.024 + wetness * 0.040 + descent * 0.036);
    float patternedLight = 1.0 - windowShadow;
    float3 lit = base * ((diffuse * shadow * patternedLight + fill) * contact) + materialLift + floorGrid * (0.12 + shadow * 0.20) + emissiveGlow + crackGlow + portalTint + floorBounce + pathAreaLight + spec;
    lit += styleLightColor(biome) * floorMask * shotEdge * shotContrast * smoothstep(0.38, 0.92, wetNoise) * 0.018;
    float sunlitWallBounce = wallMask * (biome <= 1u ? 1.0 : 0.0) * (0.024 + moss * 0.016 + glowWeight * 0.012);
    lit += (styleFogColor(biome) * 0.32 + styleLightColor(biome) * 0.044 + float3(0.018, 0.038, 0.026)) * sunlitWallBounce;
    lit += foliageMask * (styleLightColor(biome) * foliageBacklight + styleMossColor(biome) * 0.030);
    lit += styleLightColor(biome) * rim * nonPlayerCharacterMask * (0.155 + glowWeight * 0.110 + material.emission * 0.100);
    lit += float3(0.05, 0.54, 0.72) * rim * playerMask * (0.034 + glowWeight * 0.016);
    lit += (styleFogColor(biome) * 0.24 + styleLightColor(biome) * 0.120) * pow(rim, 1.28) * nonPlayerCharacterMask * saturate(0.30 + armorFacet * 0.48 + clothMask * 0.22);
    lit += styleCrackColor(biome) * wallRuneMask * (0.030 + corruption * 0.080 + descent * 0.050);
    lit += indirectColor * (floorMask * (0.060 + wetness * 0.058 + glowWeight * 0.026 + descent * 0.030) + wallMask * (0.050 + glowWeight * 0.026) + characterMask * (0.030 + glowWeight * 0.018));
    lit = lerp(lit, lit + reflectionColor * (0.64 + glowWeight * 0.30), reflectionStrength);
    lit = lerp(lit, lit * float3(0.72, 0.58, 0.66), descent * 0.14);
    lit += wire * (styleLightColor(biome) * 0.22 + float3(0.14, 0.12, 0.10));
    payload.radiance = lerp(lit, styleFogColor(biome), fogAmount);
    payload.hitKind = 1;
}
