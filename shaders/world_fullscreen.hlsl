struct PSIn {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

cbuffer WorldConstants : register(b0) {
    uint gOverlayEnabled;
    uint gWeaponId;
    uint gElementId;
    uint gActiveSlot;
    uint gQReadyPercent;
    uint gEReadyPercent;
    uint gQShapeId;
    uint gEShapeId;
    uint gLoadoutSlot0;
    uint gLoadoutSlot1;
    uint gLoadoutSlot2;
    uint gHp;
    uint gCurrentRoom;
    uint gRoomCount;
    uint gActiveEnemies;
    uint gBossHpPercent;
    uint gBossPhase;
    uint gObjectiveKind;
    uint gObjectiveProgressPercent;
    uint gPlayerStatusMask;
    uint gReactionKind;
    uint gReactionFlashPercent;
    uint gPhase;
    uint gRunStatus;
    uint gOutputWidth;
    uint gOutputHeight;
    uint gReserved0;
    uint gDamageFlashPercent;
    uint gDamageElementId;
    uint gRewardOptionCount;
    uint gRewardOption0;
    uint gRewardOption1;
    uint gRewardOption2;
    uint gRewardAppliedFlashPercent;
    uint gRewardAppliedOption;
    uint gVisualStyleIdentity;
    uint gVisualStyleSurface;
    uint gVisualStyleAtmosphere;
    uint gVisualStyleVariant;
    uint gRenderWidth;
    uint gRenderHeight;
    uint gFloorIndex;
    uint gDescentPercent;
    uint gSpriteCount;
    uint gShotLayoutIdentity;
    uint gShotLayoutWeights;
    uint gRenderQuality;
};

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
        p = p * 2.03 + float2(13.1, 7.7);
        a *= 0.50;
    }
    return saturate(v);
}

float styleWeight(uint packedValue, uint shift) {
    return (float)((packedValue >> shift) & 15u) / 15.0;
}

uint styleBiome() {
    return gVisualStyleIdentity & 3u;
}

float3 styleFloorColor(uint biome) {
    if (biome == 0u) {
        return float3(0.18, 0.15, 0.10);
    }
    if (biome == 1u) {
        return float3(0.08, 0.18, 0.10);
    }
    if (biome == 2u) {
        return float3(0.075, 0.055, 0.070);
    }
    return float3(0.025, 0.030, 0.050);
}

float3 styleLightColor(uint biome) {
    if (biome == 0u) {
        return float3(1.00, 0.70, 0.24);
    }
    if (biome == 1u) {
        return float3(0.70, 0.86, 0.38);
    }
    if (biome == 2u) {
        return float3(0.92, 0.32, 0.92);
    }
    return float3(0.45, 0.18, 1.00);
}

float3 styleCrackColor(uint biome) {
    if (biome == 3u) {
        return float3(1.00, 0.08, 0.14);
    }
    if (biome == 2u) {
        return float3(0.68, 0.24, 1.00);
    }
    return float3(0.20, 0.11, 0.07);
}

float roomSdf(float2 p, float2 halfSize) {
    float2 d = abs(p) - halfSize;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float4 MainPS(PSIn input) : SV_Target0 {
    uint biome = styleBiome();
    float moss = styleWeight(gVisualStyleSurface, 0u);
    float wetness = styleWeight(gVisualStyleSurface, 4u);
    float cracks = styleWeight(gVisualStyleSurface, 8u);
    float corruption = styleWeight(gVisualStyleAtmosphere, 0u);
    float glow = styleWeight(gVisualStyleAtmosphere, 4u);
    float fogWeight = styleWeight(gVisualStyleAtmosphere, 8u);
    float descent = styleWeight(gVisualStyleAtmosphere, 12u);
    float2 uv = input.uv * 2.0 - 1.0;
    float2 roomP = uv * float2(9.0, 5.0);
    float room = roomSdf(roomP, float2(7.25, 3.75));
    float wall = smoothstep(0.08, -0.04, room);
    float variantA = (float)(gVisualStyleVariant & 127u) * 0.019;
    float slabEdge = 1.0 - smoothstep(0.016, 0.065, min(min(frac(roomP.x * 0.55 + variantA), 1.0 - frac(roomP.x * 0.55 + variantA)),
        min(frac(roomP.y * 0.64 + variantA), 1.0 - frac(roomP.y * 0.64 + variantA))));
    float runes = smoothstep(0.940, 0.990, fbm2(roomP * (2.8 + glow * 0.7) + variantA)) * glow;
    float mossCells = smoothstep(0.45, 0.84, fbm2(roomP * 0.42 + 31.0));
    float wetCells = smoothstep(0.58, 0.92, fbm2(roomP * 0.56 + 73.0));
    float vein = abs(frac(roomP.x * 0.16 + roomP.y * 0.26 + variantA) - 0.5);
    float crackCells = (1.0 - smoothstep(0.012, 0.055, vein)) * smoothstep(0.55, 0.88, fbm2(roomP * 1.25 + 117.0));
    float fog = saturate(1.0 - length(uv) * 0.85);
    float3 floorCol = lerp(styleFloorColor(biome) * 0.55, styleFloorColor(biome) * (1.25 + fogWeight * 0.25), fog);
    floorCol *= 0.84 + fbm2(roomP * 0.74 + variantA) * 0.18;
    floorCol = lerp(floorCol, float3(0.06, 0.24, 0.10), moss * mossCells * 0.12);
    floorCol = lerp(floorCol, float3(0.04, 0.12, 0.18), wetness * wetCells * 0.13);
    float3 runeCol = styleLightColor(biome) * runes * wall * (0.035 + glow * 0.12);
    float3 crackCol = styleCrackColor(biome) * crackCells * wall * (0.015 + corruption * 0.055);
    float3 col = floorCol * wall + runeCol;
    col += crackCol;
    col += slabEdge * wall * (styleLightColor(biome) * 0.018 + float3(0.024, 0.020, 0.018));
    col += styleLightColor(biome) * (0.04 + glow * 0.05) * smoothstep(-0.5, 0.2, -room);
    col = lerp(col, styleFloorColor(biome) * 0.20, saturate(length(uv) * (0.22 + fogWeight * 0.18 + descent * 0.18)));
    col = lerp(col, col * float3(0.55, 0.42, 0.58), saturate(length(uv) * descent * 0.30));
    return float4(col, 1.0);
}

