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
    return min(gFrame.renderQuality, 2u);
}

float3 styleFogColor(uint biome) {
    if (biome == 0u) {
        return float3(0.095, 0.070, 0.040);
    }
    if (biome == 1u) {
        return float3(0.026, 0.052, 0.038);
    }
    if (biome == 2u) {
        return float3(0.040, 0.024, 0.060);
    }
    return float3(0.014, 0.018, 0.045);
}

float3 styleLightColor(uint biome) {
    if (biome == 0u) {
        return float3(0.86, 0.54, 0.26);
    }
    if (biome == 1u) {
        return float3(0.76, 0.52, 0.28);
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
    float3 color = payload.hitKind != 0 ? payload.radiance : fog + styleLightColor(biome) * glowWeight * pulse * vignette * 0.012;
    float alpha = payload.hitKind != 0 ? 0.97 : 0.0;
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
    float3 baseFog = styleFogColor(biome) * (0.20 + fogWeight * 0.36 + up * 0.14);
    float3 source = styleLightColor(biome) * (0.018 + glowWeight * 0.035) * shafts * (0.55 + hazeNoise * 0.45);
    float3 abyssTint = float3(0.025, 0.010, 0.020) * descent * (0.40 + fogWeight * 0.50);
    payload.radiance = baseFog + source + abyssTint;
    payload.hitKind = 0;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs) {
    if (payload.depth == 2u) {
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
    uint rtQuality = renderQuality();
    float3 lightDir = normalize(biome == 3u ? float3(-0.18, 0.78, -0.58) : float3(-0.52, 0.86, -0.30));
    float3 fillDir = normalize(float3(0.62, 0.55, 0.38));
    float diffuse = 0.044 + 1.22 * saturate(dot(normal, lightDir));
    float openSky = biome <= 1u ? saturate(normal.y * 0.50 + 0.50) : 0.0;
    float fill = (0.026 + descent * 0.044 + (biome <= 1u ? 0.058 : 0.006)) * saturate(dot(normal, fillDir)) +
        descent * 0.020 + openSky * (0.036 + glowWeight * 0.016);
    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float pulse = 0.5 + 0.5 * sin(gFrame.timeSeconds * 4.0 + dot(hitPos.xz, float2(0.19, 0.13)));
    float lowFloorEmissive = smoothstep(0.13, 0.02, abs(hitPos.y)) * smoothstep(0.20, 0.48, material.emission);
    float glow = material.emission * (0.68 + 0.42 * pulse) * (0.98 + glowWeight * 0.46) * (1.0 - lowFloorEmissive * 0.24);
    float3 emissiveGlow = material.baseColor * glow * (1.02 + material.emission * 0.36);
    float floorMask = (materialIndex == 0u || materialIndex == 9u) ? 1.0 : 0.0;
    float wallMask = materialIndex == 1u ? 1.0 : 0.0;
    float foliageMask = materialIndex == 12u ? 1.0 : 0.0;
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
    float characterMask = saturate(playerMask + bladeMask + bruteMask + casterMask + projectileMask + controlMask + sparkMask + runeMask + foliageMask + boneMask + bossMask);
    float armorMask = saturate(playerMask + bladeMask + bruteMask + projectileMask + boneMask + bossMask);
    float clothMask = saturate(playerMask + casterMask + runeMask + bossMask);
    float emissiveMask = smoothstep(0.18, 0.72, material.emission);
    float nonCharacterEmission = emissiveMask * (1.0 - characterMask);
    float readableSparkEdge = edge * saturate(sparkMask + portalMask + runeMask) * 0.020;
    float wire = edge * (floorMask * 0.0025 + wallMask * 0.0035 + nonCharacterEmission * 0.048) + readableSparkEdge;
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
    float charBumpX = (charGrain - 0.5) * characterMask * (0.075 + armorMask * 0.055);
    float charBumpY = (charRidge - 0.5) * characterMask * (0.022 + clothMask * 0.038);
    normal = normalize(normal + float3(
        floorBumpX * 0.55 + wallBumpA * 0.12 + charBumpX,
        wallBumpB * 0.070 + charBumpY,
        floorBumpZ * 0.55 + wallBumpB * 0.10 + charBumpX * 0.70));
    diffuse = 0.044 + 1.22 * saturate(dot(normal, lightDir));
    openSky = biome <= 1u ? saturate(normal.y * 0.50 + 0.50) : 0.0;
    fill = (0.026 + descent * 0.044 + (biome <= 1u ? 0.058 : 0.006)) * saturate(dot(normal, fillDir)) +
        descent * 0.020 + openSky * (0.036 + glowWeight * 0.016);
    float shadow = 1.0;
    if (payload.depth == 0u && material.emission < 0.62) {
        RayDesc shadowRay;
        shadowRay.Origin = hitPos + normal * 0.035 + lightDir * 0.020;
        shadowRay.Direction = lightDir;
        shadowRay.TMin = 0.035;
        shadowRay.TMax = 18.0;

        RayPayload shadowPayload;
        shadowPayload.radiance = float3(0.0, 0.0, 0.0);
        shadowPayload.hitKind = 0;
        shadowPayload.depth = 2u;
        TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 1, 0, shadowRay, shadowPayload);
        shadow = shadowPayload.hitKind != 0u ? (biome <= 1u ? 0.34 : 0.22) : 1.0;
    }

    float contact = 1.0;
    if (payload.depth == 0u && material.emission < 0.45) {
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
        contactPayload.depth = 2u;
        TraceRay(gScene, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 1, 0, contactRay, contactPayload);
        contact = contactPayload.hitKind != 0u
            ? (floorMask > 0.5 ? (biome <= 1u ? 0.48 : 0.42) : (biome <= 1u ? 0.58 : 0.50))
            : 1.0;
    }

    float3 base = material.baseColor;
    base *= lerp(1.0, 0.58 + stoneGrain * 0.50 + fineGrain * 0.20, floorMask);
    base *= lerp(1.0, 0.64 + wallGrain * 0.40, wallMask);
    float tileCell = valueNoise(floor(stoneP * (0.24 + (float)((gFrame.visualStyleIdentity >> 8u) & 3u) * 0.035)) + float2(variantA, variantB));
    float tileTone = 0.70 + tileCell * 0.52 + fineGrain * 0.12;
    float3 tileWarmth = biome <= 1u ? float3(0.030, 0.026, 0.014) :
        (biome == 2u ? float3(0.024, 0.010, 0.014) : float3(0.012, 0.010, 0.024));
    base = lerp(base, base * tileTone + tileWarmth * (tileCell - 0.36), floorMask * 0.62);
    base = lerp(base, base + styleLightColor(biome) * 0.030, wallMask * wallRibs * (0.25 + glowWeight * 0.30));
    base = lerp(base, base * 0.56, wallMask * wallMortar * (0.20 + decay * 0.18));
    base = lerp(base, base * 0.58 + styleFogColor(biome) * 0.20, wallMask * wallSoot * (0.18 + descent * 0.16));
    float wallRuneA = 1.0 - smoothstep(0.008, 0.046, abs(frac(hitPos.x * 0.34 + hitPos.y * 0.78 + variantA) - 0.50));
    float wallRuneB = 1.0 - smoothstep(0.010, 0.052, abs(frac(hitPos.z * 0.30 - hitPos.y * 0.62 + variantB) - 0.50));
    float wallRuneMask = wallMask * smoothstep(0.44, 0.88, wallSoot + glowWeight * 0.20) * saturate(wallRuneA * 0.42 + wallRuneB * 0.36);
    base += styleLightColor(biome) * wallRuneMask * (0.012 + glowWeight * 0.026 + descent * 0.014);
    base = lerp(base, styleMossColor(biome), floorMask * moss * mossNoise * 0.22);
    base = lerp(base, styleMossColor(biome) * 0.70, wallMask * moss * mossNoise * 0.08);
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
    float3 heroicTint = styleLightColor(biome) * (0.052 + glowWeight * 0.046) + styleFogColor(biome) * 0.050;
    base = lerp(base, base * (0.70 + charGrain * 0.42), characterMask * 0.42);
    base = lerp(base, base + float3(0.070, 0.085, 0.095) * armorFacet - float3(0.040, 0.035, 0.030) * battleWear, armorMask * 0.36);
    base = lerp(base, base * (0.72 + clothWeaveA * 0.12 + clothWeaveB * 0.08), clothMask * 0.34);
    base = lerp(base, base + heroicTint + float3(0.026, 0.030, 0.036) * costumePanel, characterMask * costumePanel * 0.28);
    base = lerp(base, base * 1.16 + styleLightColor(biome) * 0.066, playerMask * saturate(costumePanel + armorFacet) * 0.24);
    base = lerp(base, base * 0.86 + styleCrackColor(biome) * 0.095, saturate(bruteMask + bossMask) * battleWear * 0.24);
    base = lerp(base, float3(0.62, 0.70, 0.72) * (0.72 + charGrain * 0.34) + styleLightColor(biome) * 0.045, boneMask * 0.72);
    base += bladeMask * (styleLightColor(biome) * (0.082 + armorFacet * 0.085) + float3(0.052, 0.110, 0.124));
    base += casterMask * styleLightColor(biome) * runeLines * (0.072 + glowWeight * 0.112);
    base += bossMask * styleCrackColor(biome) * (runeLines * (0.115 + corruption * 0.22) + battleWear * 0.040);
    base += boneMask * boneBands * float3(0.045, 0.060, 0.060);
    emissiveGlow += (casterMask + runeMask + bossMask + sparkMask * 1.34 + bladeMask * 0.78) * styleLightColor(biome) * runeLines * (0.090 + glowWeight * 0.170 + material.emission * 0.21);
    emissiveGlow += styleLightColor(biome) * jewelCell * characterMask * smoothstep(0.12, 0.68, material.emission + glowWeight * 0.22) * 0.130;
    base = lerp(base, base * 0.58 + float3(0.045, 0.135, 0.175), floorMask * wetness * wetNoise * 0.32);
    float tileShardA = smoothstep(0.76, 0.96, fbm2(stoneP * 1.72 + float2(variantB, 12.0)));
    float tileShardB = 1.0 - smoothstep(0.015, 0.060, abs(frac(stoneP.x * 0.41 - stoneP.y * 0.29 + variantA) - 0.5));
    float wornEdges = saturate(slabEdge * (0.38 + fineGrain * 0.44) + tileShardA * tileShardB * 0.28);
    base = lerp(base, base + float3(0.065, 0.060, 0.050), floorMask * wornEdges * (0.18 + wetness * 0.10));
    float seamDark = slabEdge * floorMask * (0.125 + cracks * 0.075 + descent * 0.045);
    base *= 1.0 - seamDark;
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
        ((biome <= 1u) ? (0.085 + glowWeight * 0.082) : (0.052 + glowWeight * 0.075));
    float3 warmShaft = lerp(float3(1.00, 0.74, 0.42), styleLightColor(biome), biome >= 2u ? 0.62 : 0.20) * windowPatch * (1.0 - descent * 0.30);
    float dampSparkle = floorMask * wetness * wetNoise * smoothstep(0.52, 0.95, fineGrain) * (0.020 + glowWeight * 0.025);
    float3 floorBounce = styleLightColor(biome) * lampBloom * (0.026 + glowWeight * 0.046 + descent * 0.018) + warmShaft * (1.16 + wetness * 0.20) +
        styleFogColor(biome) * dampSparkle;
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
        indirectColor = diffusePayload.radiance;
    }

    float qualitySpecScale = rtQuality == 0u ? 0.55 : (rtQuality == 1u ? 0.82 : 1.02);
    float floorSpec = floorMask * (0.145 + wetness * 1.68) * (0.58 + wetNoise * 1.48) * pow(saturate(dot(reflect(WorldRayDirection(), normal), lightDir)), 10.0) * qualitySpecScale;
    float3 viewDir = normalize(-WorldRayDirection());
    float rim = pow(saturate(1.0 - dot(normal, viewDir)), 2.25);
    float propSpec = (wallMask * 0.024 + characterMask * (0.040 + material.emission * 0.115) + foliageMask * 0.018) *
        pow(saturate(dot(reflect(WorldRayDirection(), normal), lightDir)), 17.0) * (0.84 + glowWeight * 0.54);
    float spec = floorSpec + propSpec;
    float fogAmount = saturate(RayTCurrent() * (0.004 + fogWeight * 0.010));
    float3 materialLift = base * floorMask * (0.040 + wetness * 0.050 + descent * 0.060);
    float patternedLight = 1.0 - windowShadow;
    float3 lit = base * ((diffuse * shadow * patternedLight + fill) * contact) + materialLift + floorGrid * (0.12 + shadow * 0.20) + emissiveGlow + crackGlow + portalTint + floorBounce + spec;
    float sunlitWallBounce = wallMask * (biome <= 1u ? 1.0 : 0.0) * (0.045 + moss * 0.022 + glowWeight * 0.014);
    lit += (styleFogColor(biome) * 0.50 + styleLightColor(biome) * 0.080 + float3(0.026, 0.064, 0.040)) * sunlitWallBounce;
    lit += foliageMask * (styleLightColor(biome) * foliageBacklight + styleMossColor(biome) * 0.030);
    lit += styleLightColor(biome) * rim * characterMask * (0.100 + glowWeight * 0.095 + material.emission * 0.085);
    lit += (styleFogColor(biome) * 0.28 + styleLightColor(biome) * 0.110) * pow(rim, 1.38) * characterMask * saturate(0.24 + armorFacet * 0.42 + clothMask * 0.24);
    lit += styleCrackColor(biome) * wallRuneMask * (0.030 + corruption * 0.080 + descent * 0.050);
    lit += indirectColor * (floorMask * (0.045 + wetness * 0.040 + glowWeight * 0.018 + descent * 0.026) + wallMask * (0.030 + glowWeight * 0.016));
    lit = lerp(lit, lit + reflectionColor * (0.64 + glowWeight * 0.30), reflectionStrength);
    lit = lerp(lit, lit * float3(0.72, 0.58, 0.66), descent * 0.14);
    lit += wire * (styleLightColor(biome) * 0.22 + float3(0.14, 0.12, 0.10));
    payload.radiance = lerp(lit, styleFogColor(biome), fogAmount);
    payload.hitKind = 1;
}
