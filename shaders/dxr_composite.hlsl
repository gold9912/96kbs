struct RenderSprite {
    float4 positionSize;
    float4 colorAlpha;
    uint atlasIndex;
    uint flags;
    uint seed;
    uint reserved;
};

Texture2D<float4> gDxrOutput : register(t0);
StructuredBuffer<RenderSprite> gSprites : register(t1);
Texture2D<float> gVfxAtlas : register(t2);
SamplerState gLinearClamp : register(s0);

cbuffer OverlayConstants : register(b0) {
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

struct PSIn {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

bool ReferenceMode() {
    return (gReserved0 & 0x80000000u) != 0u;
}

#define CH_0 48u
#define CH_1 49u
#define CH_2 50u
#define CH_3 51u
#define CH_4 52u
#define CH_5 53u
#define CH_6 54u
#define CH_7 55u
#define CH_8 56u
#define CH_9 57u
#define CH_A 65u
#define CH_B 66u
#define CH_C 67u
#define CH_D 68u
#define CH_E 69u
#define CH_F 70u
#define CH_G 71u
#define CH_H 72u
#define CH_I 73u
#define CH_J 74u
#define CH_K 75u
#define CH_L 76u
#define CH_M 77u
#define CH_N 78u
#define CH_O 79u
#define CH_P 80u
#define CH_Q 81u
#define CH_R 82u
#define CH_S 83u
#define CH_T 84u
#define CH_U 85u
#define CH_V 86u
#define CH_W 87u
#define CH_X 88u
#define CH_Y 89u
#define CH_Z 90u
#define CH_PCT 37u
#define CH_SLASH 47u

uint Pack4(uint a, uint b, uint c, uint d) {
    return a | (b << 8u) | (c << 16u) | (d << 24u);
}

uint WordChar(uint4 word, uint index) {
    uint packed = index < 4u ? word.x : (index < 8u ? word.y : (index < 12u ? word.z : word.w));
    return (packed >> ((index & 3u) * 8u)) & 255u;
}

uint4 Word1(uint a) {
    return uint4(Pack4(a, 0u, 0u, 0u), 0u, 0u, 0u);
}

uint4 Word2(uint a, uint b) {
    return uint4(Pack4(a, b, 0u, 0u), 0u, 0u, 0u);
}

uint4 Word3(uint a, uint b, uint c) {
    return uint4(Pack4(a, b, c, 0u), 0u, 0u, 0u);
}

uint4 Word4(uint a, uint b, uint c, uint d) {
    return uint4(Pack4(a, b, c, d), 0u, 0u, 0u);
}

uint4 WeaponWord(uint id) {
    switch (id) {
    case 0u:
        return uint4(Pack4(CH_H, CH_A, CH_M, CH_M), Pack4(CH_E, CH_R, 0u, 0u), 0u, 0u);
    case 1u:
        return uint4(Pack4(CH_S, CH_P, CH_E, CH_A), Pack4(CH_R, 0u, 0u, 0u), 0u, 0u);
    case 2u:
        return uint4(Pack4(CH_K, CH_A, CH_T, CH_A), Pack4(CH_N, CH_A, 0u, 0u), 0u, 0u);
    case 3u:
        return uint4(Pack4(CH_P, CH_I, CH_S, CH_T), Pack4(CH_O, CH_L, 0u, 0u), 0u, 0u);
    case 4u:
        return uint4(Pack4(CH_R, CH_I, CH_F, CH_L), Pack4(CH_E, 0u, 0u, 0u), 0u, 0u);
    case 5u:
        return uint4(Pack4(CH_M, CH_A, CH_C, CH_H), Pack4(CH_I, CH_N, CH_E, CH_G), Pack4(CH_U, CH_N, 0u, 0u), 0u);
    case 6u:
        return uint4(Pack4(CH_S, CH_H, CH_O, CH_T), Pack4(CH_G, CH_U, CH_N, 0u), 0u, 0u);
    case 7u:
        return uint4(Pack4(CH_S, CH_T, CH_A, CH_F), Pack4(CH_F, 0u, 0u, 0u), 0u, 0u);
    case 8u:
        return uint4(Pack4(CH_S, CH_C, CH_E, CH_P), Pack4(CH_T, CH_E, CH_R, 0u), 0u, 0u);
    case 9u:
        return uint4(Pack4(CH_G, CH_L, CH_O, CH_V), Pack4(CH_E, CH_S, 0u, 0u), 0u, 0u);
    }
    return uint4(Pack4(CH_P, CH_I, CH_S, CH_T), Pack4(CH_O, CH_L, 0u, 0u), 0u, 0u);
}

uint4 WeaponCodeWord(uint id) {
    switch (id) {
    case 0u:
        return Word2(CH_H, CH_M);
    case 1u:
        return Word2(CH_S, CH_P);
    case 2u:
        return Word2(CH_K, CH_T);
    case 3u:
        return Word2(CH_P, CH_T);
    case 4u:
        return Word2(CH_R, CH_F);
    case 5u:
        return Word2(CH_M, CH_G);
    case 6u:
        return Word2(CH_S, CH_G);
    case 7u:
        return Word2(CH_S, CH_T);
    case 8u:
        return Word2(CH_S, CH_C);
    case 9u:
        return Word2(CH_G, CH_V);
    }
    return Word2(CH_P, CH_T);
}

uint4 ElementWord(uint id) {
    switch (id) {
    case 0u:
        return uint4(Pack4(CH_W, CH_A, CH_T, CH_E), Pack4(CH_R, 0u, 0u, 0u), 0u, 0u);
    case 1u:
        return uint4(Pack4(CH_F, CH_I, CH_R, CH_E), 0u, 0u, 0u);
    case 2u:
        return uint4(Pack4(CH_S, CH_T, CH_O, CH_N), Pack4(CH_E, 0u, 0u, 0u), 0u, 0u);
    case 3u:
        return uint4(Pack4(CH_V, CH_O, CH_L, CH_T), 0u, 0u, 0u);
    case 4u:
        return uint4(Pack4(CH_I, CH_C, CH_E, 0u), 0u, 0u, 0u);
    case 5u:
        return uint4(Pack4(CH_A, CH_I, CH_R, 0u), 0u, 0u, 0u);
    }
    return uint4(Pack4(CH_N, CH_O, CH_N, CH_E), 0u, 0u, 0u);
}

uint4 UpgradeWord(uint id) {
    switch (id) {
    case 0u:
        return uint4(Pack4(CH_D, CH_A, CH_M, CH_A), Pack4(CH_G, CH_E, 0u, 0u), 0u, 0u);
    case 1u:
        return uint4(Pack4(CH_C, CH_O, CH_O, CH_L), Pack4(CH_D, CH_O, CH_W, CH_N), 0u, 0u);
    case 2u:
        return uint4(Pack4(CH_S, CH_P, CH_E, CH_E), Pack4(CH_D, 0u, 0u, 0u), 0u, 0u);
    case 3u:
        return uint4(Pack4(CH_A, CH_R, CH_E, CH_A), 0u, 0u, 0u);
    case 4u:
        return uint4(Pack4(CH_M, CH_A, CH_X, CH_H), Pack4(CH_P, 0u, 0u, 0u), 0u, 0u);
    case 5u:
        return uint4(Pack4(CH_H, CH_E, CH_A, CH_L), 0u, 0u, 0u);
    }
    return uint4(Pack4(CH_U, CH_P, CH_G, 0u), 0u, 0u, 0u);
}

uint4 ObjectiveWord(uint id) {
    if (id == 0u) {
        return uint4(Pack4(CH_K, CH_I, CH_L, CH_L), 0u, 0u, 0u);
    }
    if (id == 1u) {
        return uint4(Pack4(CH_S, CH_U, CH_R, CH_V), 0u, 0u, 0u);
    }
    return uint4(Pack4(CH_C, CH_T, CH_R, CH_L), 0u, 0u, 0u);
}

uint4 ReactionWord(uint id) {
    switch (id) {
    case 1u:
        return uint4(Pack4(CH_W, CH_F, CH_I, CH_R), 0u, 0u, 0u);
    case 2u:
        return uint4(Pack4(CH_B, CH_W, CH_A, CH_T), 0u, 0u, 0u);
    case 3u:
        return uint4(Pack4(CH_W, CH_V, CH_L, CH_T), 0u, 0u, 0u);
    case 4u:
        return uint4(Pack4(CH_W, CH_I, CH_C, CH_E), 0u, 0u, 0u);
    case 5u:
        return uint4(Pack4(CH_W, CH_A, CH_I, CH_R), 0u, 0u, 0u);
    case 6u:
        return uint4(Pack4(CH_B, CH_S, CH_T, CH_N), 0u, 0u, 0u);
    case 7u:
        return uint4(Pack4(CH_B, CH_V, CH_L, CH_T), 0u, 0u, 0u);
    case 8u:
        return uint4(Pack4(CH_B, CH_I, CH_C, CH_E), 0u, 0u, 0u);
    case 9u:
        return uint4(Pack4(CH_B, CH_A, CH_I, CH_R), 0u, 0u, 0u);
    case 10u:
        return uint4(Pack4(CH_C, CH_W, CH_A, CH_T), 0u, 0u, 0u);
    case 11u:
        return uint4(Pack4(CH_C, CH_F, CH_I, CH_R), 0u, 0u, 0u);
    case 12u:
        return uint4(Pack4(CH_C, CH_V, CH_L, CH_T), 0u, 0u, 0u);
    case 13u:
        return uint4(Pack4(CH_C, CH_I, CH_C, CH_E), 0u, 0u, 0u);
    case 14u:
        return uint4(Pack4(CH_I, CH_W, CH_A, CH_T), 0u, 0u, 0u);
    case 15u:
        return uint4(Pack4(CH_I, CH_F, CH_I, CH_R), 0u, 0u, 0u);
    case 16u:
        return uint4(Pack4(CH_I, CH_S, CH_T, CH_N), 0u, 0u, 0u);
    case 17u:
        return uint4(Pack4(CH_I, CH_V, CH_L, CH_T), 0u, 0u, 0u);
    case 18u:
        return uint4(Pack4(CH_I, CH_I, CH_C, CH_E), 0u, 0u, 0u);
    case 19u:
        return uint4(Pack4(CH_C, CH_A, CH_I, CH_R), 0u, 0u, 0u);
    case 20u:
        return uint4(Pack4(CH_I, CH_A, CH_I, CH_R), 0u, 0u, 0u);
    case 21u:
        return uint4(Pack4(CH_A, CH_R, CH_C, 0u), 0u, 0u, 0u);
    }
    return uint4(Pack4(CH_N, CH_O, CH_N, CH_E), 0u, 0u, 0u);
}

uint4 ActionWord(uint weaponId, uint actionId) {
    if (weaponId == 0u) {
        return actionId == 0u ? uint4(Pack4(CH_S, CH_M, CH_A, CH_S), Pack4(CH_H, 0u, 0u, 0u), 0u, 0u)
                              : uint4(Pack4(CH_S, CH_P, CH_I, CH_N), 0u, 0u, 0u);
    }
    if (weaponId == 1u) {
        return actionId == 0u ? uint4(Pack4(CH_T, CH_H, CH_R, CH_U), Pack4(CH_S, CH_T, 0u, 0u), 0u, 0u)
                              : uint4(Pack4(CH_D, CH_A, CH_S, CH_H), 0u, 0u, 0u);
    }
    if (weaponId == 2u) {
        return actionId == 0u ? uint4(Pack4(CH_S, CH_L, CH_A, CH_S), Pack4(CH_H, 0u, 0u, 0u), 0u, 0u)
                              : uint4(Pack4(CH_W, CH_A, CH_V, CH_E), 0u, 0u, 0u);
    }
    if (weaponId == 3u) {
        return actionId == 0u ? uint4(Pack4(CH_S, CH_H, CH_O, CH_T), 0u, 0u, 0u)
                              : uint4(Pack4(CH_C, CH_H, CH_A, CH_R), Pack4(CH_G, CH_E, 0u, 0u), 0u, 0u);
    }
    if (weaponId == 4u) {
        return actionId == 0u ? uint4(Pack4(CH_P, CH_I, CH_E, CH_R), Pack4(CH_C, CH_E, 0u, 0u), 0u, 0u)
                              : uint4(Pack4(CH_B, CH_U, CH_R, CH_S), Pack4(CH_T, 0u, 0u, 0u), 0u, 0u);
    }
    if (weaponId == 5u) {
        return actionId == 0u ? uint4(Pack4(CH_S, CH_P, CH_R, CH_A), Pack4(CH_Y, 0u, 0u, 0u), 0u, 0u)
                              : uint4(Pack4(CH_O, CH_V, CH_E, CH_R), Pack4(CH_D, CH_R, CH_I, CH_V), Pack4(CH_E, 0u, 0u, 0u), 0u);
    }
    if (weaponId == 6u) {
        return actionId == 0u ? uint4(Pack4(CH_S, CH_C, CH_A, CH_T), Pack4(CH_T, CH_E, CH_R, 0u), 0u, 0u)
                              : uint4(Pack4(CH_S, CH_H, CH_O, CH_C), Pack4(CH_K, 0u, 0u, 0u), 0u, 0u);
    }
    if (weaponId == 7u) {
        return actionId == 0u ? uint4(Pack4(CH_O, CH_R, CH_B, CH_I), Pack4(CH_T, 0u, 0u, 0u), 0u, 0u)
                              : uint4(Pack4(CH_O, CH_R, CH_B, 0u), 0u, 0u, 0u);
    }
    if (weaponId == 8u) {
        return actionId == 0u ? uint4(Pack4(CH_M, CH_A, CH_R, CH_K), 0u, 0u, 0u)
                              : uint4(Pack4(CH_B, CH_O, CH_L, CH_T), 0u, 0u, 0u);
    }
    if (weaponId == 9u) {
        return actionId == 0u ? uint4(Pack4(CH_P, CH_U, CH_L, CH_S), Pack4(CH_E, 0u, 0u, 0u), 0u, 0u)
                              : uint4(Pack4(CH_F, CH_I, CH_E, CH_L), Pack4(CH_D, 0u, 0u, 0u), 0u, 0u);
    }
    return actionId == 0u ? uint4(Pack4(CH_S, CH_H, CH_O, CH_T), 0u, 0u, 0u)
                          : uint4(Pack4(CH_C, CH_H, CH_A, CH_R), Pack4(CH_G, CH_E, 0u, 0u), 0u, 0u);
}

uint4 ShapeWord(uint shapeId) {
    switch (shapeId) {
    case 0u:
        return uint4(Pack4(CH_P, CH_U, CH_L, CH_S), Pack4(CH_E, 0u, 0u, 0u), 0u, 0u);
    case 1u:
        return uint4(Pack4(CH_C, CH_O, CH_N, CH_E), 0u, 0u, 0u);
    case 2u:
        return uint4(Pack4(CH_S, CH_H, CH_O, CH_T), 0u, 0u, 0u);
    case 3u:
        return uint4(Pack4(CH_B, CH_U, CH_R, CH_S), Pack4(CH_T, 0u, 0u, 0u), 0u, 0u);
    case 4u:
        return uint4(Pack4(CH_D, CH_A, CH_S, CH_H), 0u, 0u, 0u);
    case 5u:
        return uint4(Pack4(CH_W, CH_A, CH_V, CH_E), 0u, 0u, 0u);
    case 6u:
        return uint4(Pack4(CH_O, CH_R, CH_B, CH_I), Pack4(CH_T, 0u, 0u, 0u), 0u, 0u);
    case 7u:
        return uint4(Pack4(CH_M, CH_A, CH_R, CH_K), 0u, 0u, 0u);
    }
    return uint4(Pack4(CH_S, CH_H, CH_A, CH_P), Pack4(CH_E, 0u, 0u, 0u), 0u, 0u);
}

uint ShapeWordChars(uint shapeId) {
    switch (shapeId) {
    case 0u:
    case 3u:
    case 6u:
        return 5u;
    case 1u:
    case 2u:
    case 4u:
    case 5u:
    case 7u:
        return 4u;
    }
    return 5u;
}

uint GlyphRow(uint ch, uint row) {
    switch (ch) {
    case CH_0: return row == 0u ? 14u : (row == 6u ? 14u : 17u);
    case CH_1: return row == 0u ? 4u : (row == 1u ? 12u : (row == 6u ? 14u : 4u));
    case CH_2: return row == 0u ? 14u : (row == 1u ? 17u : (row == 2u ? 1u : (row == 3u ? 2u : (row == 4u ? 4u : (row == 5u ? 8u : 31u)))));
    case CH_3: return row == 0u ? 30u : (row == 1u ? 1u : (row == 2u ? 1u : (row == 3u ? 14u : (row == 4u ? 1u : (row == 5u ? 1u : 30u)))));
    case CH_4: return row == 0u ? 2u : (row == 1u ? 6u : (row == 2u ? 10u : (row == 3u ? 18u : (row == 4u ? 31u : 2u))));
    case CH_5: return row == 0u ? 31u : (row == 1u ? 16u : (row == 2u ? 30u : (row == 3u ? 1u : (row == 4u ? 1u : (row == 5u ? 17u : 14u)))));
    case CH_6: return row == 0u ? 6u : (row == 1u ? 8u : (row == 2u ? 16u : (row == 3u ? 30u : (row == 4u ? 17u : (row == 5u ? 17u : 14u)))));
    case CH_7: return row == 0u ? 31u : (row == 1u ? 1u : (row == 2u ? 2u : (row == 3u ? 4u : (row == 4u ? 8u : 8u))));
    case CH_8: return row == 0u ? 14u : (row == 3u ? 14u : (row == 6u ? 14u : 17u));
    case CH_9: return row == 0u ? 14u : (row == 1u ? 17u : (row == 2u ? 17u : (row == 3u ? 15u : (row == 4u ? 1u : (row == 5u ? 2u : 12u)))));
    case CH_A: return row == 0u ? 14u : (row == 3u ? 31u : 17u);
    case CH_B: return row == 0u ? 30u : (row == 3u ? 30u : (row == 6u ? 30u : 17u));
    case CH_C: return row == 0u ? 14u : (row == 6u ? 14u : (row == 1u || row == 5u ? 17u : 16u));
    case CH_D: return row == 0u ? 30u : (row == 6u ? 30u : 17u);
    case CH_E: return row == 0u ? 31u : (row == 3u ? 30u : (row == 6u ? 31u : 16u));
    case CH_F: return row == 0u ? 31u : (row == 3u ? 30u : 16u);
    case CH_G: return row == 0u ? 14u : (row == 1u ? 17u : (row == 2u ? 16u : (row == 3u ? 23u : (row == 6u ? 14u : 17u))));
    case CH_H: return row == 3u ? 31u : 17u;
    case CH_I: return row == 0u ? 14u : (row == 6u ? 14u : 4u);
    case CH_J: return row == 0u ? 7u : (row == 5u ? 18u : (row == 6u ? 12u : 2u));
    case CH_K: return row == 0u ? 18u : (row == 1u ? 20u : (row == 2u ? 24u : (row == 3u ? 16u : (row == 4u ? 24u : (row == 5u ? 20u : 18u)))));
    case CH_L: return row == 6u ? 31u : 16u;
    case CH_M: return row == 0u ? 17u : (row == 1u ? 27u : (row == 2u ? 21u : 17u));
    case CH_N: return row == 0u ? 17u : (row == 1u ? 25u : (row == 2u ? 21u : (row == 3u ? 19u : 17u)));
    case CH_O: return row == 0u ? 14u : (row == 6u ? 14u : 17u);
    case CH_P: return row == 0u ? 30u : (row == 1u ? 17u : (row == 2u ? 17u : (row == 3u ? 30u : 16u)));
    case CH_Q: return row == 0u ? 14u : (row == 5u ? 18u : (row == 6u ? 13u : 17u));
    case CH_R: return row == 0u ? 30u : (row == 1u ? 17u : (row == 2u ? 17u : (row == 3u ? 30u : (row == 4u ? 20u : (row == 5u ? 18u : 17u)))));
    case CH_S: return row == 0u ? 15u : (row == 1u ? 16u : (row == 2u ? 16u : (row == 3u ? 14u : (row == 4u ? 1u : (row == 5u ? 1u : 30u)))));
    case CH_T: return row == 0u ? 31u : 4u;
    case CH_U: return row == 6u ? 14u : 17u;
    case CH_V: return row < 4u ? 17u : (row == 4u ? 10u : (row == 5u ? 10u : 4u));
    case CH_W: return row < 4u ? 17u : (row == 4u ? 21u : (row == 5u ? 27u : 17u));
    case CH_X: return row == 0u ? 17u : (row == 1u ? 10u : (row == 2u ? 4u : (row == 3u ? 4u : (row == 4u ? 4u : (row == 5u ? 10u : 17u)))));
    case CH_Y: return row < 2u ? 17u : (row == 2u ? 10u : 4u);
    case CH_Z: return row == 0u ? 31u : (row == 1u ? 1u : (row == 2u ? 2u : (row == 3u ? 4u : (row == 4u ? 8u : (row == 5u ? 16u : 31u)))));
    case CH_PCT: return row == 0u ? 25u : (row == 1u ? 26u : (row == 2u ? 2u : (row == 3u ? 4u : (row == 4u ? 8u : (row == 5u ? 11u : 19u)))));
    case CH_SLASH: return row == 0u ? 1u : (row == 1u ? 2u : (row == 2u ? 2u : (row == 3u ? 4u : (row == 4u ? 8u : (row == 5u ? 8u : 16u)))));
    }
    return 0u;
}

float Box(float2 p, float2 mn, float2 mx) {
    float2 a = step(mn, p);
    float2 b = step(p, mx);
    return a.x * a.y * b.x * b.y;
}

float GlyphMask(float2 p, float2 origin, float scale, uint ch) {
    if (ch == 0u) {
        return 0.0;
    }
    float2 local = (p - origin) / scale;
    if (local.x < 0.0 || local.y < 0.0 || local.x >= 5.0 || local.y >= 7.0) {
        return 0.0;
    }
    uint col = (uint)floor(local.x);
    uint row = (uint)floor(local.y);
    uint bits = GlyphRow(ch, row);
    return (((bits >> (4u - col)) & 1u) != 0u) ? 1.0 : 0.0;
}

float WordMask(float2 p, float2 origin, float scale, uint4 word, uint maxChars) {
    float mask = 0.0;
    [unroll]
    for (uint i = 0u; i < 16u; ++i) {
        if (i < maxChars) {
            uint ch = WordChar(word, i);
            mask = max(mask, GlyphMask(p, origin + float2((float)i * scale * 6.0, 0.0), scale, ch));
        }
    }
    return mask;
}

uint Pow10(uint e) {
    return e == 0u ? 1u : (e == 1u ? 10u : 100u);
}

float NumberMask(float2 p, float2 origin, float scale, uint value, uint digits) {
    uint clampedValue = min(value, digits == 2u ? 99u : 999u);
    float mask = 0.0;
    [unroll]
    for (uint i = 0u; i < 3u; ++i) {
        if (i < digits) {
            uint divisor = Pow10(digits - 1u - i);
            uint digit = (clampedValue / divisor) % 10u;
            mask = max(mask, GlyphMask(p, origin + float2((float)i * scale * 6.0, 0.0), scale, CH_0 + digit));
        }
    }
    return mask;
}

void BlendOverlay(inout float3 color, inout float alpha, float3 src, float srcAlpha) {
    float a = saturate(srcAlpha);
    color = lerp(color, src, a);
    alpha = max(alpha, a);
}

void DrawText(inout float3 color, inout float alpha, float2 p, float2 origin, float scale, uint4 word, uint maxChars, float3 textColor) {
    float m = WordMask(p, origin, scale, word, maxChars);
    BlendOverlay(color, alpha, textColor, m);
}

void DrawNumber(inout float3 color, inout float alpha, float2 p, float2 origin, float scale, uint value, uint digits, float3 textColor) {
    float m = NumberMask(p, origin, scale, value, digits);
    BlendOverlay(color, alpha, textColor, m);
}

float LineBox(float2 p, float2 mn, float2 mx, float thickness) {
    float outer = Box(p, mn, mx);
    float inner = Box(p, mn + thickness, mx - thickness);
    return saturate(outer - inner);
}

float VLine(float2 p, float2 origin, float height, float thickness) {
    return Box(p, origin, origin + float2(thickness, height));
}

float HLine(float2 p, float2 origin, float width, float thickness) {
    return Box(p, origin, origin + float2(width, thickness));
}

float Diamond(float2 p, float2 center, float radius) {
    float2 d = abs(p - center);
    return step(d.x + d.y, radius);
}

float3 ElementColor(uint id) {
    switch (id) {
    case 0u:
        return float3(0.26, 0.82, 1.0);
    case 1u:
        return float3(1.0, 0.34, 0.16);
    case 2u:
        return float3(0.68, 0.62, 0.48);
    case 3u:
        return float3(0.55, 0.98, 1.0);
    case 4u:
        return float3(0.64, 0.88, 1.0);
    case 5u:
        return float3(0.74, 1.0, 0.76);
    }
    return float3(0.78, 0.78, 0.84);
}

float StyleWeight(uint packedValue, uint shift) {
    return (float)((packedValue >> shift) & 15u) / 15.0;
}

uint StyleBiome() {
    return gVisualStyleIdentity & 3u;
}

float ShotWeight(uint shift) {
    return (float)((gShotLayoutWeights >> shift) & 15u) / 15.0;
}

float Hash21(float2 p) {
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float CellDot(float2 p, float scale, float radius, out float rnd) {
    float2 cell = floor(p * scale);
    float2 local = frac(p * scale) - 0.5;
    rnd = Hash21(cell + (float)(gVisualStyleVariant & 1023u) * 0.013);
    float2 jitter = float2(Hash21(cell + 19.7), Hash21(cell + 71.3)) - 0.5;
    return 1.0 - smoothstep(radius, radius * 1.85, length(local - jitter * 0.34));
}

float3 StyleHudTint() {
    uint biome = StyleBiome();
    if (biome == 0u) {
        return float3(0.30, 0.96, 0.78);
    }
    if (biome == 1u) {
        return float3(0.24, 0.95, 0.66);
    }
    if (biome == 2u) {
        return float3(0.48, 0.92, 1.00);
    }
    return float3(0.30, 0.82, 1.00);
}

float3 StylePanelBase(float3 elementColor) {
    uint biome = StyleBiome();
    float corruption = StyleWeight(gVisualStyleAtmosphere, 0u);
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float3 base = biome == 3u ? float3(0.014, 0.012, 0.030) :
        (biome == 2u ? float3(0.022, 0.014, 0.030) :
        (biome == 1u ? float3(0.014, 0.022, 0.020) : float3(0.020, 0.016, 0.026)));
    return base * (1.0 - descent * 0.18) + elementColor * (0.016 + glow * 0.014) + float3(0.05, 0.0, 0.02) * corruption * 0.18;
}

float3 StyleGradeTint() {
    uint biome = StyleBiome();
    if (biome == 0u) {
        return float3(0.96, 1.01, 1.04);
    }
    if (biome == 1u) {
        return float3(0.90, 1.06, 0.96);
    }
    if (biome == 2u) {
        return float3(0.94, 0.82, 1.02);
    }
    return float3(0.76, 0.80, 1.04);
}

float3 StyleFogTint() {
    uint biome = StyleBiome();
    if (biome == 0u) {
        return float3(0.060, 0.076, 0.054);
    }
    if (biome == 1u) {
        return float3(0.038, 0.092, 0.060);
    }
    if (biome == 2u) {
        return float3(0.055, 0.030, 0.090);
    }
    return float3(0.018, 0.022, 0.060);
}

float Luminance(float3 color) {
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 FilmicToneMap(float3 color) {
    color = max(color, 0.0);
    color = color * (1.0 + color * 0.24) / (1.0 + color * 1.16);
    return pow(saturate(color), 1.0 / 2.2);
}

float3 BrightSample(float2 uv) {
    float3 c = gDxrOutput.SampleLevel(gLinearClamp, uv, 0.0).rgb;
    float b = smoothstep(0.48, 1.14, max(max(c.r, c.g), c.b));
    return c * b;
}

float3 BloomSample(float2 uv, float2 texel) {
    float3 bloom = BrightSample(uv) * 0.16;
    bloom += BrightSample(uv + texel * float2(1.5, 0.0)) * 0.088;
    bloom += BrightSample(uv + texel * float2(-1.5, 0.0)) * 0.088;
    bloom += BrightSample(uv + texel * float2(0.0, 1.5)) * 0.088;
    bloom += BrightSample(uv + texel * float2(0.0, -1.5)) * 0.088;
    bloom += BrightSample(uv + texel * float2(3.8, 2.4)) * 0.060;
    bloom += BrightSample(uv + texel * float2(-3.8, 2.4)) * 0.060;
    bloom += BrightSample(uv + texel * float2(3.8, -2.4)) * 0.060;
    bloom += BrightSample(uv + texel * float2(-3.8, -2.4)) * 0.060;
    return bloom;
}

float3 SoftSceneSample(float2 uv, float2 texel) {
    float3 c = gDxrOutput.SampleLevel(gLinearClamp, uv, 0.0).rgb * 0.36;
    c += gDxrOutput.SampleLevel(gLinearClamp, uv + texel * float2(2.0, 0.0), 0.0).rgb * 0.11;
    c += gDxrOutput.SampleLevel(gLinearClamp, uv + texel * float2(-2.0, 0.0), 0.0).rgb * 0.11;
    c += gDxrOutput.SampleLevel(gLinearClamp, uv + texel * float2(0.0, 2.0), 0.0).rgb * 0.11;
    c += gDxrOutput.SampleLevel(gLinearClamp, uv + texel * float2(0.0, -2.0), 0.0).rgb * 0.11;
    c += gDxrOutput.SampleLevel(gLinearClamp, uv + texel * float2(4.0, 3.0), 0.0).rgb * 0.05;
    c += gDxrOutput.SampleLevel(gLinearClamp, uv + texel * float2(-4.0, 3.0), 0.0).rgb * 0.05;
    c += gDxrOutput.SampleLevel(gLinearClamp, uv + texel * float2(4.0, -3.0), 0.0).rgb * 0.05;
    c += gDxrOutput.SampleLevel(gLinearClamp, uv + texel * float2(-4.0, -3.0), 0.0).rgb * 0.05;
    return c;
}

float3 GlareSample(float2 uv, float2 texel) {
    float3 glare = float3(0.0, 0.0, 0.0);
    glare += BrightSample(uv + texel * float2(9.0, 0.0)) * 0.032;
    glare += BrightSample(uv + texel * float2(-9.0, 0.0)) * 0.032;
    glare += BrightSample(uv + texel * float2(18.0, 0.0)) * 0.020;
    glare += BrightSample(uv + texel * float2(-18.0, 0.0)) * 0.020;
    glare += BrightSample(uv + texel * float2(12.0, -8.0)) * 0.024;
    glare += BrightSample(uv + texel * float2(-12.0, 8.0)) * 0.024;
    glare += BrightSample(uv + texel * float2(24.0, -16.0)) * 0.014;
    glare += BrightSample(uv + texel * float2(-24.0, 16.0)) * 0.014;
    return glare;
}

float3 EdgeAwareSceneSample(float2 uv, float2 texel, float3 centerColor) {
    float3 left = gDxrOutput.SampleLevel(gLinearClamp, uv - texel * float2(1.0, 0.0), 0.0).rgb;
    float3 right = gDxrOutput.SampleLevel(gLinearClamp, uv + texel * float2(1.0, 0.0), 0.0).rgb;
    float3 up = gDxrOutput.SampleLevel(gLinearClamp, uv - texel * float2(0.0, 1.0), 0.0).rgb;
    float3 down = gDxrOutput.SampleLevel(gLinearClamp, uv + texel * float2(0.0, 1.0), 0.0).rgb;
    float lum = Luminance(centerColor);
    float edge = max(max(abs(lum - Luminance(left)), abs(lum - Luminance(right))), max(abs(lum - Luminance(up)), abs(lum - Luminance(down))));
    float blend = saturate((edge - 0.045) * 5.8);
    float3 crossAverage = centerColor * 0.42 + (left + right + up + down) * 0.145;
    return lerp(centerColor, crossAverage, blend * 0.58);
}

float SunShaftMask(float2 uv, float2 centered) {
    uint biome = StyleBiome();
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float2 source = biome <= 1u ? float2(0.84, 0.08) : (biome == 2u ? float2(0.72, 0.12) : float2(0.28, 0.12));
    float2 d = uv - source;
    float diagonal = abs(d.y + d.x * 0.46);
    float broad = smoothstep(0.22, 0.0, diagonal) * smoothstep(1.00, 0.07, length(d));
    float core = smoothstep(0.074, 0.0, diagonal) * smoothstep(0.56, 0.05, length(d));
    float stripe = 0.78 + 0.22 * sin((uv.x + uv.y) * 26.0 + (float)(gVisualStyleVariant & 63u));
    return (broad * 0.46 + core * 0.40) * stripe * (0.050 + glow * 0.090) * (1.0 - descent * 0.42);
}

float3 ScreenLightPool(float2 uv, float2 centered) {
    uint biome = StyleBiome();
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float fog = StyleWeight(gVisualStyleAtmosphere, 8u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float diagonal = 1.0 - smoothstep(0.035, 0.32, abs((uv.y - 0.18) + (uv.x - 0.78) * 0.54));
    float sourceFalloff = smoothstep(1.10, 0.10, length((uv - float2(0.78, 0.20)) * float2(0.90, 1.35)));
    float centerPool = smoothstep(0.82, 0.10, length((centered - float2(0.08, -0.06)) * float2(0.88, 1.25)));
    float3 warm = float3(1.00, 0.78, 0.48) * (diagonal * sourceFalloff * (0.014 + glow * 0.020) + centerPool * 0.006);
    float darkPulse = smoothstep(0.90, 0.10, length((centered - float2(0.10, -0.02)) * float2(0.92, 1.14)));
    float3 abyss = StyleHudTint() * darkPulse * (0.032 + glow * 0.040 + fog * 0.024);

    if (biome <= 1u) {
        return warm * (1.0 - descent * 0.55);
    }
    return abyss + float3(0.40, 0.02, 0.06) * darkPulse * descent * 0.028;
}

float ReferenceDepthShadow(float2 uv, float2 centered) {
    float fog = StyleWeight(gVisualStyleAtmosphere, 8u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float roomFrame = smoothstep(0.50, 1.16, max(abs(centered.x) * 0.78, abs(centered.y) * 1.12));
    float topMass = smoothstep(0.34, 0.00, uv.y) * smoothstep(0.06, 0.44, uv.x);
    float lowerRail = smoothstep(0.76, 1.02, uv.y) * smoothstep(0.12, 0.92, uv.x) * 0.42;
    float cornerFalloff = smoothstep(0.76, 1.34, length(centered * float2(0.94, 1.06)));
    return saturate(roomFrame * (0.20 + descent * 0.16) + topMass * (0.10 + fog * 0.08) + lowerRail * 0.08 + cornerFalloff * (0.12 + descent * 0.12));
}

float3 ReferenceKeyLight(float2 uv, float2 centered) {
    uint biome = StyleBiome();
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float fog = StyleWeight(gVisualStyleAtmosphere, 8u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float3 hud = StyleHudTint();
    float arena = smoothstep(1.18, 0.20, length(centered * float2(0.80, 1.14)));

    if (biome <= 1u) {
        float2 source = float2(0.84, 0.08);
        float2 d = uv - source;
        float diagonal = abs(d.y + d.x * 0.58);
        float broadBeam = smoothstep(0.24, 0.0, diagonal) * smoothstep(1.00, 0.05, length(d * float2(0.86, 1.10)));
        float hotBeam = smoothstep(0.078, 0.0, diagonal) * smoothstep(0.54, 0.04, length(d * float2(0.94, 1.24)));
        float lattice = 0.70 + 0.30 * smoothstep(0.15, 0.95, sin((uv.x * 20.0 - uv.y * 9.0 + (float)(gVisualStyleVariant & 31u)) * 3.14159265) * 0.5 + 0.5);
        float floorPool = smoothstep(0.80, 0.08, length((centered - float2(0.12, -0.05)) * float2(0.78, 1.26))) * arena;
        float rimLantern = smoothstep(0.24, 0.00, length(uv - float2(0.12, 0.88))) +
            smoothstep(0.20, 0.00, length(uv - float2(0.86, 0.80)));
        float3 warm = float3(1.00, 0.76, 0.44);
        float3 cyan = float3(0.18, 0.96, 1.00);
        return warm * ((broadBeam * 0.016 + hotBeam * 0.024) * lattice + floorPool * 0.006) * (1.0 - descent * 0.58) +
            cyan * rimLantern * (0.030 + glow * 0.028);
    }

    float altar = smoothstep(0.70, 0.08, length((centered - float2(0.04, -0.04)) * float2(0.82, 1.12)));
    float fissureA = 1.0 - smoothstep(0.010, 0.055, abs(frac(uv.x * 2.6 - uv.y * 1.4 + (float)(gVisualStyleVariant & 63u) * 0.017) - 0.50));
    float fissureB = 1.0 - smoothstep(0.012, 0.065, abs(frac(uv.x * -1.8 - uv.y * 2.2 + (float)((gVisualStyleVariant >> 6u) & 63u) * 0.019) - 0.50));
    float fissureMask = arena * smoothstep(0.30, 0.88, length(centered * float2(0.74, 1.08))) * saturate(fissureA * 0.62 + fissureB * 0.46);
    float upperGlow = smoothstep(0.50, 0.02, uv.y) * (0.45 + 0.55 * arena);
    float3 ember = float3(1.0, 0.05, 0.12);
    return hud * altar * (0.075 + glow * 0.070 + fog * 0.028) +
        ember * fissureMask * (0.030 + descent * 0.060) +
        float3(0.56, 0.08, 1.00) * upperGlow * (0.024 + glow * 0.045);
}

float3 ReferenceWetGlints(float2 uv, float2 centered) {
    float wetness = StyleWeight(gVisualStyleSurface, 4u);
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float arena = smoothstep(1.04, 0.22, length(centered * float2(0.84, 1.18)));
    float sweepA = 1.0 - smoothstep(0.004, 0.030, abs(frac(uv.x * 2.2 + uv.y * 0.68 + (float)(gVisualStyleVariant & 127u) * 0.011) - 0.50));
    float sweepB = 1.0 - smoothstep(0.005, 0.040, abs(frac(uv.x * -1.4 + uv.y * 1.9 + (float)((gVisualStyleVariant >> 7u) & 127u) * 0.009) - 0.50));
    float broken = smoothstep(0.42, 0.92, Hash21(floor(uv * float2(46.0, 31.0))));
    float glint = arena * broken * saturate(sweepA * 0.55 + sweepB * 0.38) * (0.20 + wetness * 0.90 + descent * 0.26);
    return lerp(float3(0.88, 0.72, 0.44), StyleHudTint(), 0.48 + descent * 0.26) * glint * (0.010 + glow * 0.018);
}

float3 ReferenceFloorMosaic(float2 uv, float2 centered) {
    uint biome = StyleBiome();
    float wetness = StyleWeight(gVisualStyleSurface, 4u);
    float cracks = StyleWeight(gVisualStyleSurface, 8u);
    float corruption = StyleWeight(gVisualStyleAtmosphere, 0u);
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float arena = smoothstep(1.08, 0.20, length(centered * float2(0.84, 1.16)));
    float2 p = centered * float2(1.24, 0.92) + float2(0.04, -0.05);
    float ring = 1.0 - smoothstep(0.010, 0.045, abs(length(p) - (0.30 + descent * 0.025)));
    float ringOuter = 1.0 - smoothstep(0.010, 0.048, abs(length(p) - (0.54 + cracks * 0.035)));
    float diagA = 1.0 - smoothstep(0.006, 0.034, abs(frac(uv.x * 4.6 + uv.y * 2.7 + (float)(gVisualStyleVariant & 63u) * 0.013) - 0.50));
    float diagB = 1.0 - smoothstep(0.008, 0.042, abs(frac(uv.x * -3.8 + uv.y * 3.2 + (float)((gVisualStyleVariant >> 6u) & 63u) * 0.017) - 0.50));
    float slabX = 1.0 - smoothstep(0.005, 0.030, abs(frac(uv.x * 7.2 + (float)(gVisualStyleVariant & 15u) * 0.031) - 0.50));
    float slabY = 1.0 - smoothstep(0.006, 0.032, abs(frac(uv.y * 5.4 + (float)((gVisualStyleVariant >> 4u) & 15u) * 0.037) - 0.50));
    float carved = arena * saturate(ring * 0.62 + ringOuter * 0.42 + diagA * 0.18 + diagB * 0.14 + (slabX + slabY) * 0.052);
    float broken = smoothstep(0.28, 0.92, Hash21(floor(uv * float2(58.0, 34.0))));
    float3 cold = StyleHudTint() * (0.020 + glow * 0.030 + wetness * 0.018);
    float3 warm = float3(1.00, 0.72, 0.36) * (0.012 + wetness * 0.018) * (1.0 - descent * 0.42);
    float3 infernal = float3(1.00, 0.035, 0.080) * (0.012 + corruption * 0.040 + descent * 0.026);
    return (biome <= 1u ? cold + warm : cold * 0.62 + infernal * 0.82) * carved * (0.55 + broken * 0.75);
}

float3 ReferenceVolumetricVeil(float2 uv, float2 centered) {
    uint biome = StyleBiome();
    float fog = StyleWeight(gVisualStyleAtmosphere, 8u);
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float rnd = 0.0;
    float dust = CellDot(uv + float2(0.041, 0.087) + (float)(gPhase & 31u) * 0.0007, 42.0, 0.038, rnd);
    float dither = Hash21(floor(uv * float2(96.0, 54.0)) + (float)(gVisualStyleVariant & 255u));

    if (biome <= 1u) {
        float2 source = float2(0.86, 0.06);
        float2 d = uv - source;
        float rayA = smoothstep(0.36, 0.0, abs(d.y + d.x * 0.58)) * smoothstep(1.12, 0.08, length(d * float2(0.82, 1.06)));
        float rayB = smoothstep(0.20, 0.0, abs(d.y + d.x * 0.86 - 0.12)) * smoothstep(0.96, 0.08, length(d * float2(1.10, 0.94)));
        float lattice = 0.58 + 0.42 * smoothstep(0.18, 0.94, sin((uv.x * 26.0 - uv.y * 18.0 + (float)(gVisualStyleVariant & 31u)) * 3.14159265) * 0.5 + 0.5);
        float floorHaze = smoothstep(0.90, 0.40, uv.y) * smoothstep(1.22, 0.28, length(centered * float2(0.86, 1.22)));
        float motes = dust * step(0.78 - fog * 0.08, rnd) * (0.45 + dither * 0.55);
        return float3(1.00, 0.84, 0.56) * (rayA * 0.024 + rayB * 0.011) * lattice * (1.0 - descent * 0.54) +
            StyleHudTint() * floorHaze * (0.013 + glow * 0.020) +
            float3(1.00, 0.86, 0.56) * motes * (0.010 + fog * 0.020);
    }

    float arena = smoothstep(1.18, 0.26, length(centered * float2(0.82, 1.14)));
    float smokeA = smoothstep(0.16, 0.62, Hash21(floor(uv * float2(18.0, 11.0)) + (float)(gVisualStyleVariant & 127u) * 0.17));
    float smokeB = smoothstep(0.18, 0.74, sin(uv.x * 18.0 - uv.y * 10.0 + (float)(gPhase & 127u) * 0.025) * 0.5 + 0.5);
    float upperRays = smoothstep(0.64, 0.02, uv.y) * smoothstep(1.12, 0.24, abs(centered.x));
    float fissureGlow = smoothstep(0.92, 0.26, length(centered * float2(0.86, 1.18))) *
        smoothstep(0.68, 1.12, length(centered * float2(0.92, 1.06)));
    float sparks = dust * step(0.84 - glow * 0.10, rnd) * arena;
    return StyleHudTint() * smokeA * smokeB * arena * (0.010 + fog * 0.026 + glow * 0.014) +
        float3(0.72, 0.05, 1.00) * upperRays * (0.014 + glow * 0.034) +
        float3(1.00, 0.045, 0.070) * fissureGlow * (0.012 + descent * 0.035) +
        float3(1.00, 0.10, 0.16) * sparks * (0.018 + descent * 0.026);
}

float3 ApplyReferenceForeground(float3 color, float2 uv, float2 centered) {
    uint biome = StyleBiome();
    float fog = StyleWeight(gVisualStyleAtmosphere, 8u);
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float foregroundBias = ShotWeight(8u);
    float edgeDensity = ShotWeight(4u);
    float topMass = smoothstep(0.36, 0.02, uv.y) * smoothstep(0.03, 0.30, uv.x) * smoothstep(0.99, 0.58, uv.x);
    float lowerRail = smoothstep(0.78, 1.05, uv.y) * smoothstep(0.02, 0.30, uv.x) * smoothstep(0.98, 0.62, uv.x);
    float sideMass = smoothstep(0.82, 1.10, abs(centered.x)) * smoothstep(0.14, 0.88, uv.y);
    float ribCount = biome == 3u ? 8.0 : (biome == 2u ? 7.0 : 5.6);
    float rib = 1.0 - smoothstep(0.010, 0.050, abs(frac(uv.x * ribCount + (float)(gVisualStyleVariant & 31u) * 0.019) - 0.50));
    rib *= smoothstep(0.54, 0.03, uv.y);
    float arch = 1.0 - smoothstep(0.020, 0.11, abs(length((uv - float2(0.50, 0.18)) * float2(1.70, 1.00)) - 0.50));
    arch *= smoothstep(0.40, 0.04, uv.y);

    float vine = 0.0;
    if (biome <= 1u) {
        float strand = 1.0 - smoothstep(0.004, 0.030, abs(frac(uv.x * 18.0 + (float)(gVisualStyleVariant & 63u) * 0.013) - 0.50));
        float hang = smoothstep(0.48, 0.05, uv.y) * smoothstep(0.16, 0.92, uv.x);
        float leafDots = smoothstep(0.78, 0.98, Hash21(floor(uv * float2(72.0, 40.0)) + 11.0));
        vine = saturate(strand * hang * 0.75 + leafDots * hang * 0.22);
    }

    float mask = saturate(topMass * (0.24 + foregroundBias * 0.10) + lowerRail * 0.10 + sideMass * (0.12 + descent * 0.08 + edgeDensity * 0.05) + rib * 0.090 + arch * 0.085 + vine * (0.10 + foregroundBias * 0.06));
    mask *= biome <= 1u ? 0.24 : 0.82;
    float3 shadowTint = biome <= 1u ? float3(0.030, 0.054, 0.038) : float3(0.024, 0.014, 0.048);
    color = lerp(color, color * (0.80 - descent * 0.035) + shadowTint, mask);
    color += (biome <= 1u ? float3(0.030, 0.072, 0.032) : StyleHudTint() * 0.34) * vine * (0.010 + glow * 0.006);
    color += StyleHudTint() * rib * (0.006 + glow * 0.012 + fog * 0.006);
    return color;
}

float3 ApplySunlitShadowGrade(float3 color, float2 uv, float2 centered) {
    uint biome = StyleBiome();
    if (biome > 1u) {
        return color;
    }

    float moss = StyleWeight(gVisualStyleSurface, 0u);
    float fog = StyleWeight(gVisualStyleAtmosphere, 8u);
    float edge = smoothstep(0.44, 1.08, length(centered * float2(0.86, 1.14)));
    float lowerLeft = smoothstep(0.36, 0.96, uv.y) * smoothstep(0.46, 0.00, uv.x);
    float canopy = smoothstep(0.34, 0.02, uv.y) * smoothstep(0.08, 0.86, uv.x);
    float shadow = saturate(edge * 0.30 + lowerLeft * 0.16 + canopy * 0.12);
    float3 coolShadow = color * float3(0.66, 0.72, 0.76) + float3(0.010, 0.022 + moss * 0.004, 0.026);
    color = lerp(color, coolShadow, shadow * (0.46 + fog * 0.10));
    color = lerp(color, color * float3(0.98, 1.00, 1.03), 0.070 + moss * 0.012);
    return color;
}

float ShotRectMask(float2 uv, float2 mn, float2 mx, float edge) {
    float2 low = smoothstep(mn, mn + edge, uv);
    float2 high = 1.0 - smoothstep(mx - edge, mx, uv);
    return saturate(low.x * low.y * high.x * high.y);
}

float ShotStripe(float v, float count, float width) {
    float d = abs(frac(v * count) - 0.5);
    return 1.0 - smoothstep(width, width * 2.8, d);
}

float EllipseGlow(float2 uv, float2 center, float2 aspect, float radius) {
    return smoothstep(radius, 0.0, length((uv - center) * aspect));
}

float3 ReferenceSetDressingLights(float2 uv, float2 centered) {
    uint biome = StyleBiome();
    float wetness = StyleWeight(gVisualStyleSurface, 4u);
    float corruption = StyleWeight(gVisualStyleAtmosphere, 0u);
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float fog = StyleWeight(gVisualStyleAtmosphere, 8u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float3 hud = StyleHudTint();
    float3 lights = float3(0.0, 0.0, 0.0);

    if (biome <= 1u) {
        float3 warm = float3(0.92, 0.80, 0.58);
        float3 cyan = float3(0.20, 0.92, 1.00);
        float windowCore = EllipseGlow(uv, float2(0.865, 0.105), float2(1.0, 1.55), 0.17);
        float windowHalo = EllipseGlow(uv, float2(0.870, 0.130), float2(0.80, 1.20), 0.36);
        float lanternA = EllipseGlow(uv, float2(0.105, 0.855), float2(1.35, 1.0), 0.060);
        float lanternB = EllipseGlow(uv, float2(0.885, 0.780), float2(1.20, 1.0), 0.052);
        float floorSheen = smoothstep(0.78, 0.12, length((centered - float2(0.12, -0.04)) * float2(0.70, 1.18)));
        float dapple = 0.68 + 0.32 * ShotStripe(uv.x * 0.76 - uv.y * 0.44 + (float)(gVisualStyleVariant & 31u) * 0.007, 8.0, 0.060);
        lights += warm * (windowCore * 0.040 + windowHalo * 0.016 + floorSheen * dapple * 0.008) * (1.0 - descent * 0.48);
        lights += cyan * (lanternA + lanternB) * (0.072 + glow * 0.046 + wetness * 0.016);
        return lights;
    }

    float3 ember = float3(1.00, 0.055, 0.095);
    float3 violet = float3(0.72, 0.08, 1.00);
    float altar = EllipseGlow(uv, float2(0.760, 0.185), float2(1.0, 1.42), 0.20);
    float crystalA = EllipseGlow(uv, float2(0.135, 0.175), float2(1.15, 1.0), 0.075);
    float crystalB = EllipseGlow(uv, float2(0.860, 0.160), float2(1.18, 1.0), 0.070);
    float brazierA = EllipseGlow(uv, float2(0.175, 0.785), float2(1.0, 1.10), 0.055);
    float brazierB = EllipseGlow(uv, float2(0.835, 0.735), float2(1.0, 1.05), 0.060);
    float floorPulse = smoothstep(0.74, 0.16, length(centered * float2(0.76, 1.08)));
    float fissure = ShotStripe(uv.x * 0.72 + uv.y * -0.84 + (float)(gVisualStyleVariant & 127u) * 0.004, 5.0, 0.034) * floorPulse;
    float candleRail = smoothstep(0.34, 0.04, uv.y) * smoothstep(0.08, 0.92, uv.x) *
        ShotStripe(uv.x + (float)((gVisualStyleVariant >> 4u) & 31u) * 0.005, 9.0, 0.044);
    lights += hud * (crystalA + crystalB) * (0.052 + glow * 0.052 + fog * 0.014);
    lights += violet * candleRail * (0.010 + glow * 0.026);
    lights += ember * (altar * (0.072 + descent * 0.058) + fissure * (0.012 + corruption * 0.030 + descent * 0.026));
    lights += lerp(hud, ember, 0.44 + corruption * 0.22) * (brazierA + brazierB) * (0.045 + glow * 0.040);
    return lights;
}

float3 ApplyReferenceColorPipeline(float3 color, float2 uv, float2 centered) {
    uint biome = StyleBiome();
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float fog = StyleWeight(gVisualStyleAtmosphere, 8u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float luma = Luminance(color);
    float shadow = 1.0 - smoothstep(0.10, 0.48, luma);
    float highlight = smoothstep(0.48, 1.20, luma);
    float arena = smoothstep(1.16, 0.24, length(centered * float2(0.82, 1.14)));
    float frame = smoothstep(0.52, 1.22, max(abs(centered.x) * 0.86, abs(centered.y) * 1.10));
    float3 gray = float3(luma, luma, luma);
    float3 shadowTint = biome <= 1u ? float3(0.020, 0.040, 0.038) : float3(0.014, 0.006, 0.040);
    float3 highlightTint = biome <= 1u ? float3(0.82, 0.78, 0.62) : lerp(StyleHudTint(), float3(1.0, 0.045, 0.085), 0.38 + descent * 0.22);
    float contrast = 1.06 + glow * 0.052 + descent * 0.074;

    color = lerp(color, color * (biome <= 1u ? float3(0.82, 0.86, 0.86) : float3(0.56, 0.42, 0.68)) + shadowTint, shadow * (0.14 + fog * 0.07 + frame * 0.09));
    color += highlightTint * highlight * (0.018 + glow * 0.024 + arena * 0.014);
    color = lerp(gray, color, 1.20 + glow * 0.12 + (biome >= 2u ? 0.08 : 0.08));
    color = (color - 0.18) * contrast + 0.18;
    color *= 1.0 - frame * (biome <= 1u ? 0.055 : (0.16 + descent * 0.06));
    if (biome >= 2u) {
        color = lerp(color, color * float3(0.86, 0.62, 1.02) + float3(0.030, 0.000, 0.026), descent * 0.16);
    } else {
        color = lerp(color, color * float3(1.00, 1.00, 0.97), highlight * 0.08);
    }
    return max(color, 0.0);
}

float3 ApplyReferenceShotPolish(float3 color, float2 uv, float2 centered) {
    uint biome = StyleBiome();
    float moss = StyleWeight(gVisualStyleSurface, 0u);
    float wetness = StyleWeight(gVisualStyleSurface, 4u);
    float corruption = StyleWeight(gVisualStyleAtmosphere, 0u);
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float fog = StyleWeight(gVisualStyleAtmosphere, 8u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float clearRadius = ShotWeight(0u);
    float edgeDensity = ShotWeight(4u);
    float foliageDensity = ShotWeight(8u);
    float heroBias = ShotWeight(12u);
    float warmCool = ShotWeight(16u);
    float arena = smoothstep(1.18, 0.22, length(centered * float2(0.82, 1.14)));
    float topDepth = (1.0 - smoothstep(0.08, 0.42, uv.y)) * smoothstep(0.02, 0.16, uv.x) * (1.0 - smoothstep(0.96, 1.0, uv.x));
    float rightDepth = smoothstep(0.72, 1.02, uv.x) * smoothstep(0.05, 0.86, uv.y);
    float leftDepth = (1.0 - smoothstep(0.00, 0.20, uv.x)) * smoothstep(0.10, 0.88, uv.y);
    float bottomDepth = smoothstep(0.76, 1.05, uv.y) * smoothstep(0.04, 0.96, uv.x);
    float frameDepth = saturate(topDepth * (0.32 + edgeDensity * 0.12) + rightDepth * 0.24 + leftDepth * 0.12 + bottomDepth * (0.12 + edgeDensity * 0.08));

    if (biome <= 1u) {
        float3 coolShade = color * float3(0.84, 0.86, 0.84) + float3(0.024, 0.048 + moss * 0.010, 0.038);
        color = lerp(color, coolShade, frameDepth * (0.13 + fog * 0.035));

        float window = ShotRectMask(uv, float2(0.762, 0.006), float2(0.998, 0.292), 0.018);
        float paneV = ShotStripe(uv.x - 0.790, 4.4, 0.026);
        float paneH = ShotStripe(uv.y - 0.016, 3.2, 0.034);
        float mullion = window * saturate(paneV + paneH);
        float pane = window * (1.0 - mullion * 0.54);
        float2 winD = (uv - float2(0.886, 0.118)) * float2(0.92, 1.28);
        float windowBloom = window * smoothstep(0.36, 0.02, length(winD));
        float3 warm = float3(0.96, 0.84, 0.62);
        color += warm * (pane * (0.016 + glow * 0.016 + warmCool * 0.008) + windowBloom * (0.030 + fog * 0.012 + warmCool * 0.018)) * (1.0 - descent * 0.42);
        color = lerp(color, color * 0.68 + float3(0.054, 0.044, 0.032), mullion * 0.28);

        float2 source = float2(0.86, 0.06);
        float2 d = uv - source;
        float rayCore = smoothstep(0.12, 0.0, abs(d.y + d.x * 0.62)) * smoothstep(1.02, 0.05, length(d * float2(0.82, 1.08)));
        float rayWide = smoothstep(0.22, 0.0, abs(d.y + d.x * 0.86 - 0.10)) * smoothstep(0.96, 0.08, length(d * float2(1.04, 0.92)));
        float rayHigh = smoothstep(0.18, 0.0, abs(d.y + d.x * 0.44 + 0.040)) * smoothstep(0.92, 0.06, length(d * float2(0.74, 1.22)));
        float rayGate = 0.72 + 0.28 * ShotStripe(uv.x - uv.y * 0.22 + (float)(gVisualStyleVariant & 63u) * 0.003, 11.0, 0.070);
        color += warm * (rayCore * (0.018 + warmCool * 0.009) + rayWide * 0.010 + rayHigh * 0.008) * rayGate * (1.0 - descent * 0.50);

        float floorLattice = arena *
            (ShotStripe(uv.x * 0.90 + uv.y * 0.44 + (float)(gVisualStyleVariant & 31u) * 0.011, 7.0, 0.026) +
             ShotStripe(uv.x * -0.62 + uv.y * 0.76 + (float)((gVisualStyleVariant >> 5u) & 31u) * 0.013, 6.0, 0.024));
        color = lerp(color, color * float3(0.48, 0.62, 0.58), saturate(floorLattice) * 0.080);
        color += float3(0.14, 0.74, 1.00) * arena * wetness * (0.024 + heroBias * 0.014);
        color = lerp(color, color * 1.028, arena * clearRadius * 0.055);
        return color;
    }

    float3 deepShade = color * float3(0.46, 0.34, 0.58) + float3(0.010, 0.006, 0.030);
    color = lerp(color, deepShade, frameDepth * (0.58 + descent * 0.16));
    float ribBand = (1.0 - smoothstep(0.08, 0.48, uv.y)) * smoothstep(0.08, 0.92, uv.x);
    float ribs = ribBand * ShotStripe(uv.x + (float)(gVisualStyleVariant & 63u) * 0.004, biome == 3u ? 10.0 : 8.0, 0.033);
    float altar = smoothstep(0.58, 0.06, length((uv - float2(0.76, 0.20)) * float2(1.18, 1.46)));
    float spine = ShotStripe(uv.x * 0.42 - uv.y * 0.92 + (float)(gVisualStyleVariant & 127u) * 0.006, 5.0, 0.022) *
        smoothstep(0.30, 0.86, length(centered * float2(0.72, 1.08))) * arena;
    float3 arcane = StyleHudTint();
    float3 ember = float3(1.00, 0.050, 0.090);
    color += arcane * ribs * (0.014 + glow * 0.030 + heroBias * 0.012);
    color += lerp(arcane, ember, 0.45 + corruption * 0.28) * altar * (0.030 + glow * 0.050 + descent * 0.026);
    color += ember * spine * (0.010 + corruption * 0.036 + descent * 0.020);
    color = lerp(color, color * float3(1.05, 0.78, 0.98), saturate(ribs + altar) * 0.06);
    return color;
}

float3 ProceduralSceneOverlay(float2 uv, float2 centered) {
    uint biome = StyleBiome();
    float moss = StyleWeight(gVisualStyleSurface, 0u);
    float wetness = StyleWeight(gVisualStyleSurface, 4u);
    float corruption = StyleWeight(gVisualStyleAtmosphere, 0u);
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float fog = StyleWeight(gVisualStyleAtmosphere, 8u);
    float descent = StyleWeight(gVisualStyleAtmosphere, 12u);
    float foliageDensity = ShotWeight(8u);
    float heroBias = ShotWeight(12u);
    float arena = smoothstep(1.18, 0.28, length(centered * float2(0.82, 1.18)));
    float edge = smoothstep(0.28, 1.08, length(centered * float2(0.84, 1.12))) * arena;
    float rndA = 0.0;
    float rndB = 0.0;
    float rndC = 0.0;
    float rndD = 0.0;
    float dotA = CellDot(uv + float2(0.017, 0.031), 64.0, 0.050, rndA);
    float dotB = CellDot(uv + float2(0.113, 0.071), 96.0, 0.042, rndB);
    float wetDot = CellDot(uv + float2(0.271, 0.043), 38.0, 0.070, rndC);
    float sparkDot = CellDot(uv + float2(0.383, 0.117), 128.0, 0.024, rndD);
    float grain = Hash21(uv * float2((float)gOutputWidth, (float)gOutputHeight) + (float)gPhase * 0.031) - 0.5;

    float3 overlay = float3(0.0, 0.0, 0.0);
    if (biome <= 1u) {
        float mossMask = dotA * edge * step(0.72 - moss * 0.20 - foliageDensity * 0.10, rndA);
        float flowerMask = dotB * edge * step(0.955, rndB);
        overlay += float3(0.020, 0.052, 0.024) * mossMask * (0.10 + moss * 0.14);
        overlay += float3(0.90, 0.82, 0.56) * flowerMask * (0.040 + foliageDensity * 0.025);
        overlay += float3(0.92, 0.68, 0.32) * wetDot * arena * wetness * step(0.92, rndC) * 0.024;
    } else {
        float emberMask = sparkDot * arena * step(0.90 - corruption * 0.10, rndD);
        float dustMask = dotA * arena * step(0.92 - fog * 0.08, rndA);
        overlay += StyleHudTint() * dustMask * (0.006 + glow * 0.018 + heroBias * 0.008);
        overlay += float3(1.00, 0.06, 0.12) * emberMask * (0.026 + corruption * 0.050);
    }

    overlay += grain * (0.006 + fog * 0.006 + descent * 0.004);
    return overlay;
}

float3 ApplyScenePost(float3 color, float2 uv, float2 texel) {
    float glow = StyleWeight(gVisualStyleAtmosphere, 4u);
    float fog = StyleWeight(gVisualStyleAtmosphere, 8u);
    float descent = saturate((float)gDescentPercent * 0.01);
    float ptMode = gRenderQuality >= 5u ? 1.0 : 0.0;
    float refMode = ReferenceMode() ? 1.0 : 0.0;
    float bloomScale = lerp(1.0, 0.34, ptMode);
    float screenFxScale = lerp(1.0, 0.12, ptMode);
    float polishScale = lerp(1.0, 0.34, ptMode);
    bloomScale = lerp(bloomScale, 0.74, refMode);
    screenFxScale = lerp(screenFxScale, 0.76, refMode);
    polishScale = lerp(polishScale, 0.68, refMode);
    uint biome = StyleBiome();
    float2 centered = uv * 2.0 - 1.0;
    float vignette = saturate(dot(centered, centered));
    float3 bloom = BloomSample(uv, texel);
    float3 soft = SoftSceneSample(uv, texel);
    float3 glare = GlareSample(uv, texel);
    float3 grade = StyleGradeTint();
    float3 fogTint = StyleFogTint();
    float luma = Luminance(color);
    float focusFalloff = smoothstep(0.40, 1.06, vignette);
    float verticalHaze = smoothstep(0.92, 0.12, uv.y);
    float sceneContrast = lerp(1.22 + glow * 0.070 + descent * 0.070, 1.13 + glow * 0.035 + descent * 0.035, ptMode);
    float3 preSoft = color;

    color = EdgeAwareSceneSample(uv, texel, color);
    color = lerp(color, soft, focusFalloff * (0.15 + fog * 0.06) * lerp(1.0, 0.44, ptMode));
    color += (preSoft - soft) * (0.095 + glow * 0.035) * (1.0 - focusFalloff * 0.42) * lerp(1.0, 0.55, ptMode);
    color += bloom * (0.31 + glow * 0.28) * bloomScale;
    color += glare * (0.25 + glow * 0.20 + descent * 0.085) * bloomScale;
    color *= grade;
    color = (color - 0.5) * sceneContrast + 0.5;
    luma = Luminance(color);
    color = lerp(float3(luma, luma, luma), color, 1.10 + glow * 0.12 + descent * 0.08);
    color *= 1.0 - ReferenceDepthShadow(uv, centered) * (biome <= 1u ? 0.28 : 0.56) * lerp(1.0, 0.52, ptMode);
    float3 shaftColor = biome <= 1u ? float3(0.92, 0.82, 0.62) : StyleHudTint();
    color += shaftColor * SunShaftMask(uv, centered) * (biome <= 1u ? 0.14 : 0.68) * screenFxScale;
    color += ScreenLightPool(uv, centered) * screenFxScale;
    color += ReferenceKeyLight(uv, centered) * screenFxScale;
    color += ReferenceWetGlints(uv, centered) * screenFxScale;
    color += ReferenceFloorMosaic(uv, centered) * screenFxScale;
    color += ReferenceVolumetricVeil(uv, centered) * screenFxScale;
    color += ReferenceSetDressingLights(uv, centered) * screenFxScale;
    color += ProceduralSceneOverlay(uv, centered) * lerp(1.0, 0.30, ptMode);
    color += fogTint * verticalHaze * (0.012 + fog * 0.020 + descent * 0.030);
    float liftedShadow = 1.0 - smoothstep(0.035, 0.22, Luminance(color));
    float edgeLift = smoothstep(0.38, 1.30, length(centered * float2(0.86, 1.10)));
    color += (biome <= 1u ? fogTint * 0.46 + float3(0.008, 0.022, 0.020) : fogTint * 0.48 + StyleHudTint() * 0.040) *
        liftedShadow * (0.026 + fog * 0.070 + edgeLift * (biome <= 1u ? 0.070 : 0.080));
    float blackVoid = 1.0 - smoothstep(0.014, 0.145, Luminance(color));
    float topRightWindow = smoothstep(0.38, 1.0, uv.x) * smoothstep(0.56, 0.04, uv.y);
    color += (fogTint * 0.54 + float3(0.032, 0.036, 0.022)) * blackVoid * refMode * 0.18;
    color += float3(1.0, 0.76, 0.40) * topRightWindow * refMode * 0.060;
    float3 polished = ApplyReferenceForeground(color, uv, centered);
    polished = ApplySunlitShadowGrade(polished, uv, centered);
    polished = ApplyReferenceShotPolish(polished, uv, centered);
    polished = ApplyReferenceColorPipeline(polished, uv, centered);
    color = lerp(color, polished, polishScale);
    color = lerp(color, fogTint, saturate(vignette * (0.016 + fog * 0.038 + descent * 0.044)));
    color *= 1.00 - vignette * (biome <= 1u ? 0.31 : 0.34 + descent * 0.20) * lerp(1.0, 0.58, refMode);
    color = max(color - (biome <= 1u ? 0.006 : 0.011 + descent * 0.005), 0.0) * (1.105 + glow * 0.040);
    float highlightGate = smoothstep(0.58, 1.42, Luminance(color));
    float3 compressedHighlights = (color * 1.10) / (1.0 + color * (0.48 + fog * 0.18));
    color = lerp(color, compressedHighlights, highlightGate * (biome <= 1u ? 0.54 : 0.42));
    return FilmicToneMap(color);
}

float2 RotateSpriteLocal(float2 p, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float SpriteAtlasMask(uint atlasIndex, float2 local) {
    float2 uv01 = local * 0.5 + 0.5;
    if (any(uv01 < 0.0) || any(uv01 > 1.0)) {
        return 0.0;
    }
    uint tile = atlasIndex & 15u;
    float2 tileCoord = float2((float)(tile & 3u), (float)(tile >> 2u));
    float2 uv = (tileCoord + uv01) * 0.25;
    return gVfxAtlas.SampleLevel(gLinearClamp, uv, 0.0).r;
}

void DrawVfxSprites(inout float3 color, inout float alpha, float2 pixel) {
    uint count = min(gSpriteCount, 128u);
    [loop]
    for (uint i = 0u; i < count; ++i) {
        RenderSprite sprite = gSprites[i];
        float radius = max(sprite.positionSize.z, 1.0);
        float2 local = (pixel - sprite.positionSize.xy) / radius;
        local = RotateSpriteLocal(local, -sprite.positionSize.w);

        float boxDistance = max(abs(local.x), abs(local.y));
        if (boxDistance > 1.24) {
            continue;
        }

        float mask = SpriteAtlasMask(sprite.atlasIndex, local);
        float radial = length(local);
        float edgeFade = (1.0 - smoothstep(0.82, 1.18, boxDistance)) * (1.0 - smoothstep(1.00, 1.38, radial));
        float core = saturate(mask * edgeFade);
        float halo = (1.0 - smoothstep(0.10, 1.24, radial)) * sprite.colorAlpha.a * 0.080;
        float srcAlpha = saturate(core * sprite.colorAlpha.a);
        float sparkle = pow(saturate(core), 1.55);
        float3 spriteColor = sprite.colorAlpha.rgb;

        color += spriteColor * (srcAlpha * (0.78 + sparkle * 1.22) + halo);
        alpha = max(alpha, srcAlpha * 0.82);
    }
}

float3 WeaponColor(uint id) {
    switch (id % 5u) {
    case 0u:
        return float3(1.0, 0.58, 0.24);
    case 1u:
        return float3(0.64, 0.92, 0.88);
    case 2u:
        return float3(1.0, 0.38, 0.55);
    case 3u:
        return float3(1.0, 0.74, 0.26);
    }
    return float3(0.72, 0.62, 1.0);
}

uint RewardOptionPacked(uint index) {
    return index == 0u ? gRewardOption0 : (index == 1u ? gRewardOption1 : gRewardOption2);
}

bool RewardOptionActive(uint packed) {
    return (packed & 0x80000000u) != 0u;
}

uint RewardOptionKind(uint packed) {
    return packed & 0x3u;
}

uint RewardOptionSlot(uint packed) {
    return (packed >> 2u) & 0x3u;
}

uint RewardOptionWeapon(uint packed) {
    return (packed >> 4u) & 0xfu;
}

uint RewardOptionElement(uint packed) {
    return (packed >> 8u) & 0x7u;
}

uint RewardOptionUpgrade(uint packed) {
    return (packed >> 11u) & 0x7u;
}

uint RewardOptionSynergyElement(uint packed) {
    return (packed >> 14u) & 0x7u;
}

uint LoadoutSlotPacked(uint index) {
    return index == 0u ? gLoadoutSlot0 : (index == 1u ? gLoadoutSlot1 : gLoadoutSlot2);
}

bool LoadoutSlotActive(uint packed) {
    return (packed & 0x80000000u) != 0u;
}

uint LoadoutSlotWeapon(uint packed) {
    return packed & 0xfu;
}

uint LoadoutSlotElement(uint packed) {
    return (packed >> 4u) & 0x7u;
}

uint LoadoutSlotQReady(uint packed) {
    return (packed >> 7u) & 0x7fu;
}

uint LoadoutSlotEReady(uint packed) {
    return (packed >> 14u) & 0x7fu;
}

void DrawBar(inout float3 color, inout float alpha, float2 p, float2 origin, float width, uint readyPercent, float3 fillColor) {
    float back = Box(p, origin, origin + float2(width, 10.0));
    BlendOverlay(color, alpha, float3(0.08, 0.06, 0.09), back * 0.95);
    float border = LineBox(p, origin, origin + float2(width, 10.0), 1.0);
    BlendOverlay(color, alpha, fillColor * 0.55 + 0.12, border);
    float fillWidth = (width - 4.0) * (float)min(readyPercent, 100u) * 0.01;
    float fill = Box(p, origin + float2(2.0, 2.0), origin + float2(2.0 + fillWidth, 8.0));
    BlendOverlay(color, alpha, fillColor, fill * 0.92);

    [unroll]
    for (uint i = 1u; i < 8u; ++i) {
        float x = origin.x + (width * (float)i / 8.0);
        float cut = VLine(p, float2(x, origin.y + 1.0), 8.0, 1.0);
        BlendOverlay(color, alpha, float3(0.01, 0.01, 0.02), cut * 0.65);
    }
}

void DrawActionShapeBadge(inout float3 color, inout float alpha, float2 p, float2 center, uint shapeId, float3 accent) {
    float2 mn = center - float2(10.0, 8.0);
    float2 mx = center + float2(10.0, 8.0);
    float back = Box(p, mn, mx);
    BlendOverlay(color, alpha, float3(0.035, 0.025, 0.050) + accent * 0.08, back * 0.78);
    BlendOverlay(color, alpha, accent * 0.65 + 0.10, LineBox(p, mn, mx, 1.0) * 0.82);

    float2 d = p - center;
    float icon = 0.0;
    if (shapeId == 0u) {
        icon = saturate(Diamond(p, center, 6.5) - Diamond(p, center, 3.5));
    } else if (shapeId == 1u) {
        icon = step(-7.0, d.x) * step(d.x, 7.0) * step(abs(d.y), (d.x + 7.0) * 0.46 + 0.8);
        icon *= 1.0 - Diamond(p, center - float2(3.0, 0.0), 2.5) * 0.35;
    } else if (shapeId == 2u) {
        icon = HLine(p, center + float2(-7.0, -1.0), 12.0, 2.0) + Diamond(p, center + float2(7.0, 0.0), 3.5);
    } else if (shapeId == 3u) {
        icon = Diamond(p, center, 3.2) +
            Diamond(p, center + float2(-6.0, -4.0), 2.6) +
            Diamond(p, center + float2(6.0, -4.0), 2.6) +
            Diamond(p, center + float2(-5.0, 4.0), 2.3) +
            Diamond(p, center + float2(5.0, 4.0), 2.3);
    } else if (shapeId == 4u) {
        icon = HLine(p, center + float2(-7.0, -1.0), 12.0, 2.0) +
            HLine(p, center + float2(-7.0, 3.0), 8.0, 1.0) +
            Diamond(p, center + float2(7.0, 0.0), 4.0);
    } else if (shapeId == 5u) {
        icon = HLine(p, center + float2(-7.0, -4.0), 8.0, 1.0) +
            HLine(p, center + float2(-4.0, 0.0), 10.0, 1.0) +
            HLine(p, center + float2(-1.0, 4.0), 8.0, 1.0);
    } else if (shapeId == 6u) {
        icon = saturate(Diamond(p, center, 6.8) - Diamond(p, center, 4.8)) +
            Diamond(p, center + float2(6.0, -4.0), 2.5);
    } else {
        icon = LineBox(p, center - float2(6.0, 6.0), center + float2(6.0, 6.0), 1.0) +
            HLine(p, center + float2(-8.0, -0.5), 16.0, 1.0) +
            VLine(p, center + float2(-0.5, -8.0), 16.0, 1.0);
    }

    BlendOverlay(color, alpha, accent * 0.78 + float3(1.0, 1.0, 1.0) * 0.24, saturate(icon) * 0.90);
}

void DrawLoadoutSlotCard(inout float3 color, inout float alpha, float2 p, uint index, float2 origin, uint packed) {
    uint weaponId = LoadoutSlotWeapon(packed);
    uint elementId = LoadoutSlotElement(packed);
    uint qReady = min(LoadoutSlotQReady(packed), 100u);
    uint eReady = min(LoadoutSlotEReady(packed), 100u);
    bool active = LoadoutSlotActive(packed);
    float3 accent = ElementColor(elementId);
    float3 white = float3(0.92, 0.96, 0.92);
    float2 size = float2(36.0, 24.0);
    float2 mx = origin + size;
    float card = Box(p, origin, mx);
    BlendOverlay(color, alpha, float3(0.020, 0.014, 0.030) + accent * (active ? 0.10 : 0.045), card * (active ? 0.92 : 0.68));
    BlendOverlay(color, alpha, active ? accent * 0.75 + white * 0.25 : accent * 0.42, LineBox(p, origin, mx, active ? 2.0 : 1.0) * (active ? 0.95 : 0.48));

    BlendOverlay(color, alpha, accent, Diamond(p, origin + float2(7.0, 7.0), active ? 5.0 : 4.0) * (active ? 0.86 : 0.48));
    DrawNumber(color, alpha, p, origin + float2(3.0, 3.0), 1.1, index + 1u, 1u, active ? float3(0.02, 0.015, 0.025) : white * 0.62);
    DrawText(color, alpha, p, origin + float2(14.0, 3.0), 1.1, WeaponCodeWord(weaponId), 2u, active ? white : white * 0.68);

    float qFill = (size.x - 6.0) * (float)qReady * 0.01;
    float eFill = (size.x - 6.0) * (float)eReady * 0.01;
    BlendOverlay(color, alpha, float3(0.05, 0.035, 0.055), Box(p, origin + float2(3.0, 15.0), origin + float2(size.x - 3.0, 17.0)) * 0.70);
    BlendOverlay(color, alpha, float3(0.05, 0.035, 0.055), Box(p, origin + float2(3.0, 19.0), origin + float2(size.x - 3.0, 21.0)) * 0.70);
    BlendOverlay(color, alpha, float3(1.0, 0.58, 0.22), Box(p, origin + float2(3.0, 15.0), origin + float2(3.0 + qFill, 17.0)) * (active ? 0.88 : 0.52));
    BlendOverlay(color, alpha, float3(0.88, 0.38, 0.92), Box(p, origin + float2(3.0, 19.0), origin + float2(3.0 + eFill, 21.0)) * (active ? 0.88 : 0.52));
}

void DrawLoadoutStrip(inout float3 color, inout float alpha, float2 p, float2 origin) {
    DrawLoadoutSlotCard(color, alpha, p, 0u, origin, LoadoutSlotPacked(0u));
    DrawLoadoutSlotCard(color, alpha, p, 1u, origin + float2(40.0, 0.0), LoadoutSlotPacked(1u));
    DrawLoadoutSlotCard(color, alpha, p, 2u, origin + float2(80.0, 0.0), LoadoutSlotPacked(2u));
}

void DrawStatusBadge(inout float3 color, inout float alpha, float2 p, float2 center, uint bit, uint ch, float3 statusColor) {
    bool active = (gPlayerStatusMask & bit) != 0u;
    float activeAlpha = active ? 0.86 : 0.18;
    float badge = Diamond(p, center, 10.0);
    BlendOverlay(color, alpha, active ? statusColor : float3(0.16, 0.14, 0.18), badge * activeAlpha);
    BlendOverlay(color, alpha, statusColor, LineBox(p, center - float2(9.0, 9.0), center + float2(9.0, 9.0), 1.0) * (active ? 0.50 : 0.22));
    DrawText(color, alpha, p, center - float2(4.0, 5.0), 1.4, Word1(ch), 1u, active ? float3(0.02, 0.016, 0.024) : float3(0.42, 0.42, 0.48));
}

void DrawRewardSynergyHint(inout float3 color, inout float alpha, float2 p, float2 origin, uint elementId, uint synergyElementId) {
    if (synergyElementId >= 6u || elementId >= 6u) {
        return;
    }

    float2 label = origin;
    float2 sourceCenter = origin + float2(25.0, 5.0);
    float2 offeredCenter = origin + float2(42.0, 5.0);
    float3 sourceColor = ElementColor(synergyElementId);
    float3 offeredColor = ElementColor(elementId);

    DrawText(color, alpha, p, label, 1.15, Word2(CH_R, CH_X), 2u, float3(0.58, 0.96, 0.90));
    BlendOverlay(color, alpha, sourceColor * 0.55 + float3(0.12, 0.14, 0.16), Diamond(p, sourceCenter, 6.0) * 0.86);
    BlendOverlay(color, alpha, offeredColor * 0.55 + float3(0.12, 0.14, 0.16), Diamond(p, offeredCenter, 6.0) * 0.86);
    BlendOverlay(color, alpha, offeredColor * 0.45 + sourceColor * 0.35, HLine(p, sourceCenter + float2(5.0, 0.0), 12.0, 1.0) * 0.70);
}

void DrawRewardCard(inout float3 color, inout float alpha, float2 p, uint index, float2 origin, uint packed) {
    float scale = 1.55;
    float2 cardSize = float2(126.0, 96.0);
    float3 white = float3(0.92, 0.96, 0.92);
    float3 muted = float3(0.48, 0.52, 0.58);

    if (!RewardOptionActive(packed)) {
        float empty = Box(p, origin, origin + cardSize);
        BlendOverlay(color, alpha, float3(0.03, 0.025, 0.04), empty * 0.45);
        BlendOverlay(color, alpha, muted, LineBox(p, origin, origin + cardSize, 1.0) * 0.35);
        return;
    }

    uint kind = RewardOptionKind(packed);
    uint slot = RewardOptionSlot(packed);
    uint weaponId = RewardOptionWeapon(packed);
    uint elementId = RewardOptionElement(packed);
    uint upgradeId = RewardOptionUpgrade(packed);
    uint synergyElementId = RewardOptionSynergyElement(packed);

    float3 accent = kind == 0u ? WeaponColor(weaponId) : (kind == 1u ? ElementColor(elementId) : float3(0.62, 0.98, 0.58));
    float card = Box(p, origin, origin + cardSize);
    BlendOverlay(color, alpha, float3(0.018, 0.014, 0.030) + accent * 0.035, card * 0.88);
    BlendOverlay(color, alpha, accent, LineBox(p, origin, origin + cardSize, 1.0));
    BlendOverlay(color, alpha, accent * 0.35, HLine(p, origin + float2(6.0, 8.0), 114.0, 1.0));

    float keyBadge = Diamond(p, origin + float2(16.0, 16.0), 11.0);
    BlendOverlay(color, alpha, accent, keyBadge * 0.88);
    DrawNumber(color, alpha, p, origin + float2(11.0, 9.0), scale, index + 1u, 1u, float3(0.02, 0.015, 0.025));

    uint4 kindWord = kind == 0u ? Word3(CH_W, CH_P, CH_N) : (kind == 1u ? Word3(CH_I, CH_N, CH_F) : Word3(CH_U, CH_P, CH_G));
    DrawText(color, alpha, p, origin + float2(34.0, 9.0), scale, kindWord, 3u, accent * 0.45 + white * 0.7);

    if (kind == 0u) {
        DrawText(color, alpha, p, origin + float2(10.0, 31.0), scale, WeaponWord(weaponId), 10u, white);
        DrawText(color, alpha, p, origin + float2(10.0, 49.0), scale, ElementWord(elementId), 6u, ElementColor(elementId) * 0.55 + white * 0.5);
        DrawRewardSynergyHint(color, alpha, p, origin + float2(74.0, 50.0), elementId, synergyElementId);
        DrawText(color, alpha, p, origin + float2(10.0, 67.0), scale, Word1(CH_Q), 1u, float3(1.0, 0.58, 0.22));
        DrawText(color, alpha, p, origin + float2(28.0, 67.0), scale, ActionWord(weaponId, 0u), 10u, white);
        DrawText(color, alpha, p, origin + float2(10.0, 82.0), scale, Word1(CH_E), 1u, float3(0.88, 0.38, 0.92));
        DrawText(color, alpha, p, origin + float2(28.0, 82.0), scale, ActionWord(weaponId, 1u), 10u, white);
    } else if (kind == 1u) {
        DrawText(color, alpha, p, origin + float2(10.0, 31.0), scale, WeaponWord(weaponId), 10u, white);
        DrawText(color, alpha, p, origin + float2(10.0, 51.0), scale, Word4(CH_E, CH_L, CH_E, CH_M), 4u, muted * 0.4 + white * 0.6);
        DrawText(color, alpha, p, origin + float2(10.0, 71.0), scale, ElementWord(elementId), 6u, ElementColor(elementId) * 0.55 + white * 0.5);
        DrawRewardSynergyHint(color, alpha, p, origin + float2(74.0, 72.0), elementId, synergyElementId);
    } else {
        DrawText(color, alpha, p, origin + float2(10.0, 39.0), scale, Word3(CH_R, CH_U, CH_N), 3u, white);
        DrawText(color, alpha, p, origin + float2(10.0, 62.0), scale, UpgradeWord(upgradeId), 8u, accent * 0.45 + white * 0.7);
    }

    DrawText(color, alpha, p, origin + float2(98.0, 13.0), scale, Word1(CH_S), 1u, float3(0.42, 0.96, 0.90));
    DrawNumber(color, alpha, p, origin + float2(112.0, 13.0), scale, slot + 1u, 1u, white);
}

void DrawRewardAppliedToast(inout float3 color, inout float alpha, float2 p, float3 elementColor) {
    if (gRewardAppliedFlashPercent == 0u || !RewardOptionActive(gRewardAppliedOption)) {
        return;
    }

    uint kind = RewardOptionKind(gRewardAppliedOption);
    uint slot = RewardOptionSlot(gRewardAppliedOption);
    uint weaponId = RewardOptionWeapon(gRewardAppliedOption);
    uint elementId = RewardOptionElement(gRewardAppliedOption);
    uint upgradeId = RewardOptionUpgrade(gRewardAppliedOption);
    float flash = saturate((float)gRewardAppliedFlashPercent * 0.01);
    float3 white = float3(0.92, 0.96, 0.92);
    float3 accent = kind == 0u ? WeaponColor(weaponId) : (kind == 1u ? ElementColor(elementId) : float3(0.62, 0.98, 0.58));
    float2 mn = float2(218.0, 86.0);
    float2 mx = float2(410.0, 110.0);

    float back = Box(p, mn, mx);
    BlendOverlay(color, alpha, float3(0.035, 0.022, 0.050) + accent * 0.12, back * (0.34 + flash * 0.48));
    BlendOverlay(color, alpha, accent * 0.70 + white * 0.24, LineBox(p, mn, mx, 1.0) * (0.38 + flash * 0.52));
    BlendOverlay(color, alpha, elementColor * 0.26, HLine(p, mn + float2(8.0, 4.0), 176.0, 1.0) * flash);

    float scale = 1.45;
    DrawText(color, alpha, p, mn + float2(10.0, 7.0), scale, Word4(CH_G, CH_A, CH_I, CH_N), 4u, accent * 0.42 + white * 0.72);
    uint4 kindWord = kind == 0u ? Word3(CH_W, CH_P, CH_N) : (kind == 1u ? Word3(CH_I, CH_N, CH_F) : Word3(CH_U, CH_P, CH_G));
    DrawText(color, alpha, p, mn + float2(66.0, 7.0), scale, kindWord, 3u, accent * 0.55 + white * 0.56);
    if (kind == 2u) {
        DrawText(color, alpha, p, mn + float2(112.0, 7.0), scale, UpgradeWord(upgradeId), 8u, white);
    } else {
        DrawText(color, alpha, p, mn + float2(112.0, 7.0), scale, Word1(CH_S), 1u, float3(0.42, 0.96, 0.90));
        DrawNumber(color, alpha, p, mn + float2(126.0, 7.0), scale, slot + 1u, 1u, white);
        DrawText(color, alpha, p, mn + float2(148.0, 7.0), scale, WeaponCodeWord(weaponId), 2u, white);
        BlendOverlay(color, alpha, ElementColor(elementId), Diamond(p, mn + float2(181.0, 13.0), 5.0) * 0.86);
    }
}

void DrawRunStatusToast(inout float3 color, inout float alpha, float2 p) {
    if (gRunStatus == 0u) {
        return;
    }

    bool clear = gRunStatus == 2u;
    float3 white = float3(0.92, 0.96, 0.92);
    float3 accent = clear ? float3(0.24, 1.00, 0.72) : float3(1.00, 0.18, 0.20);
    float2 mn = float2(218.0, 86.0);
    float2 mx = float2(398.0, 110.0);
    float back = Box(p, mn, mx);
    BlendOverlay(color, alpha, float3(0.030, 0.018, 0.040) + accent * 0.10, back * 0.88);
    BlendOverlay(color, alpha, accent * 0.76 + white * 0.18, LineBox(p, mn, mx, 1.0) * 0.95);
    BlendOverlay(color, alpha, accent * 0.42, HLine(p, mn + float2(8.0, 4.0), 164.0, 1.0) * 0.82);

    float scale = 1.65;
    if (clear) {
        DrawText(color, alpha, p, mn + float2(12.0, 6.0), scale, uint4(Pack4(CH_C, CH_L, CH_E, CH_A), Pack4(CH_R, 0u, 0u, 0u), 0u, 0u), 5u, accent * 0.35 + white * 0.78);
        DrawText(color, alpha, p, mn + float2(104.0, 6.0), scale, Word4(CH_F, CH_L, CH_R, 0u), 3u, white * 0.82);
    } else {
        DrawText(color, alpha, p, mn + float2(12.0, 6.0), scale, Word4(CH_D, CH_O, CH_W, CH_N), 4u, accent * 0.35 + white * 0.78);
        DrawText(color, alpha, p, mn + float2(94.0, 6.0), scale, Word4(CH_R, CH_U, CH_N, 0u), 3u, white * 0.82);
    }
}

void DrawGlassPanel(inout float3 color, inout float alpha, float2 p, float2 panelMin, float2 panelMax, float3 baseColor, float3 accentColor, float shimmerSeed) {
    float2 size = panelMax - panelMin;
    float panel = Box(p, panelMin, panelMax);
    float localY = saturate((p.y - panelMin.y) / max(size.y, 1.0));
    BlendOverlay(color, alpha, baseColor + accentColor * (0.026 + 0.018 * (1.0 - localY)), panel * 0.86);

    float shimmer = frac((p.x + p.y + shimmerSeed) / 96.0);
    BlendOverlay(color, alpha, accentColor * 0.16, panel * step(0.93, shimmer) * 0.18);

    float railWidth = min(96.0, max(26.0, size.x * 0.32));
    float railHeight = min(32.0, max(18.0, size.y * 0.36));
    float rail = HLine(p, panelMin, railWidth, 2.0) + HLine(p, float2(panelMin.x, panelMax.y - 2.0), railWidth, 2.0);
    rail += VLine(p, panelMin, railHeight, 2.0) + VLine(p, float2(panelMin.x, panelMax.y - railHeight), railHeight, 2.0);
    rail += HLine(p, float2(panelMax.x - railWidth, panelMin.y), railWidth, 2.0) + HLine(p, float2(panelMax.x - railWidth, panelMax.y - 2.0), railWidth, 2.0);
    rail += VLine(p, float2(panelMax.x - 2.0, panelMin.y), railHeight, 2.0) + VLine(p, float2(panelMax.x - 2.0, panelMax.y - railHeight), railHeight, 2.0);
    BlendOverlay(color, alpha, accentColor * 0.68 + float3(0.18, 0.20, 0.22), saturate(rail));

    float topGlow = HLine(p, panelMin + float2(8.0, 7.0), max(0.0, size.x - 16.0), 1.0);
    BlendOverlay(color, alpha, accentColor, topGlow * 0.68);
}

void DrawReactionToastAt(inout float3 color, inout float alpha, float2 p, float2 mn, float3 element, float3 white, float scale) {
    if (gReactionFlashPercent == 0u || gReactionKind == 0u) {
        return;
    }

    float flash = saturate((float)gReactionFlashPercent * 0.01);
    float2 mx = mn + float2(190.0, 28.0);
    float rxBack = Box(p, mn, mx);
    BlendOverlay(color, alpha, float3(0.04, 0.025, 0.08) + element * 0.10, rxBack * (0.28 + flash * 0.52));
    BlendOverlay(color, alpha, element * 0.70 + white * 0.35, LineBox(p, mn, mx, 1.0) * flash);
    DrawText(color, alpha, p, mn + float2(12.0, 8.0), scale, Word2(CH_R, CH_X), 2u, element * 0.35 + white * 0.72);
    DrawText(color, alpha, p, mn + float2(56.0, 8.0), scale, ReactionWord(gReactionKind), 4u, white);
}

void DrawHud(inout float3 color, inout float alpha, float2 p) {
    if (gOverlayEnabled == 0u) {
        return;
    }

    float scale = 2.0;
    float3 element = ElementColor(gElementId);
    float3 panelColor = StylePanelBase(element);
    float3 borderColor = lerp(float3(0.30, 0.92, 0.84), StyleHudTint(), 0.42);
    float3 cyan = float3(0.42, 0.96, 0.90);
    float3 white = float3(0.92, 0.96, 0.92);
    float3 orange = float3(1.0, 0.58, 0.22);
    float3 violet = float3(0.88, 0.38, 0.92);
    float3 weapon = WeaponColor(gWeaponId);
    float2 display = float2(max(gOutputWidth, 1u), max(gOutputHeight, 1u));
    float hudScale = max(0.75, min(display.x / 1280.0, display.y / 720.0));
    float2 logical = display / hudScale;
    float right = logical.x - 18.0;
    float bottom = logical.y - 18.0;
    float shimmerSeed = (float)((gReserved0 ^ gVisualStyleVariant) & 127u) * 2.0;

    if (gPhase == 1u) {
        float2 panelMin = float2(18.0, 18.0);
        float2 panelMax = float2(438.0, 190.0);
        DrawGlassPanel(color, alpha, p, panelMin, panelMax, panelColor, borderColor, shimmerSeed);
        DrawText(color, alpha, p, float2(30.0, 30.0), scale, uint4(Pack4(CH_R, CH_E, CH_W, CH_A), Pack4(CH_R, CH_D, 0u, 0u), 0u, 0u), 6u, cyan);
        DrawText(color, alpha, p, float2(140.0, 30.0), scale, uint4(Pack4(CH_P, CH_I, CH_C, CH_K), 0u, 0u, 0u), 4u, white);
        DrawNumber(color, alpha, p, float2(204.0, 30.0), scale, 1u, 1u, orange);
        DrawNumber(color, alpha, p, float2(230.0, 30.0), scale, 2u, 1u, violet);
        DrawNumber(color, alpha, p, float2(256.0, 30.0), scale, 3u, 1u, cyan);
        DrawRewardCard(color, alpha, p, 0u, float2(28.0, 66.0), RewardOptionPacked(0u));
        DrawRewardCard(color, alpha, p, 1u, float2(156.0, 66.0), RewardOptionPacked(1u));
        DrawRewardCard(color, alpha, p, 2u, float2(284.0, 66.0), RewardOptionPacked(2u));
        return;
    }

    float2 vitalsMin = float2(18.0, 18.0);
    float2 vitalsMax = float2(236.0, 110.0);
    DrawGlassPanel(color, alpha, p, vitalsMin, vitalsMax, panelColor, borderColor, shimmerSeed);

    if (gDamageFlashPercent > 0u) {
        float damageFlash = saturate((float)gDamageFlashPercent * 0.01);
        float3 damageColor = ElementColor(gDamageElementId) * 0.42 + float3(1.0, 0.08, 0.05) * 0.72;
        float vitalsPanel = Box(p, vitalsMin, vitalsMax);
        BlendOverlay(color, alpha, damageColor, vitalsPanel * damageFlash * 0.22);
        BlendOverlay(color, alpha, damageColor + white * 0.20, LineBox(p, vitalsMin, vitalsMax, 2.0) * damageFlash);
    }

    float hpFill = Box(p, float2(30.0, 34.0), float2(30.0 + 92.0 * (float)min(gHp, 100u) * 0.01, 38.0));
    BlendOverlay(color, alpha, float3(0.18, 1.0, 0.74), hpFill * 0.86);
    DrawText(color, alpha, p, float2(30.0, 45.0), scale, Word2(CH_H, CH_P), 2u, cyan);
    DrawNumber(color, alpha, p, float2(64.0, 45.0), scale, gHp, 3u, white);
    DrawText(color, alpha, p, float2(30.0, 74.0), scale, Word3(CH_S, CH_T, CH_S), 3u, cyan);
    DrawStatusBadge(color, alpha, p, float2(82.0, 81.0), 1u, CH_W, float3(0.24, 0.84, 1.00));
    DrawStatusBadge(color, alpha, p, float2(108.0, 81.0), 2u, CH_B, float3(1.00, 0.36, 0.15));
    DrawStatusBadge(color, alpha, p, float2(134.0, 81.0), 4u, CH_C, float3(0.62, 0.98, 1.00));
    DrawStatusBadge(color, alpha, p, float2(160.0, 81.0), 8u, CH_I, float3(0.70, 0.90, 1.00));

    float2 objMin = float2(logical.x * 0.5 - 178.0, 18.0);
    float2 objMax = objMin + float2(356.0, 72.0);
    DrawGlassPanel(color, alpha, p, objMin, objMax, panelColor, borderColor, shimmerSeed + 21.0);
    DrawText(color, alpha, p, objMin + float2(14.0, 22.0), scale, uint4(Pack4(CH_O, CH_B, CH_J, 0u), 0u, 0u, 0u), 3u, cyan);
    DrawText(color, alpha, p, objMin + float2(60.0, 22.0), scale, ObjectiveWord(gObjectiveKind), 5u, white);
    DrawNumber(color, alpha, p, objMin + float2(150.0, 22.0), scale, gObjectiveProgressPercent, 3u, cyan);
    DrawText(color, alpha, p, objMin + float2(186.0, 22.0), scale, Word1(CH_PCT), 1u, cyan);
    DrawText(color, alpha, p, objMin + float2(230.0, 22.0), scale, Word2(CH_E, CH_N), 2u, cyan);
    DrawNumber(color, alpha, p, objMin + float2(264.0, 22.0), scale, gActiveEnemies, 2u, white);
    DrawText(color, alpha, p, objMin + float2(14.0, 47.0), 1.55, Word2(CH_R, CH_M), 2u, cyan);
    DrawNumber(color, alpha, p, objMin + float2(46.0, 47.0), 1.55, gCurrentRoom, 2u, white);
    DrawText(color, alpha, p, objMin + float2(74.0, 47.0), 1.55, Word1(CH_SLASH), 1u, white);
    DrawNumber(color, alpha, p, objMin + float2(86.0, 47.0), 1.55, gRoomCount, 2u, white);
    DrawText(color, alpha, p, objMin + float2(140.0, 47.0), 1.55, Word2(CH_F, CH_L), 2u, cyan);
    DrawNumber(color, alpha, p, objMin + float2(172.0, 47.0), 1.55, gFloorIndex + 1u, 2u, white);
    DrawText(color, alpha, p, objMin + float2(226.0, 47.0), 1.55, Word3(CH_D, CH_P, CH_T), 3u, cyan);
    DrawNumber(color, alpha, p, objMin + float2(270.0, 47.0), 1.55, gDescentPercent, 3u, white);
    DrawText(color, alpha, p, objMin + float2(306.0, 47.0), 1.55, Word1(CH_PCT), 1u, white);

    if (gBossPhase > 0u) {
        float3 bossColor = float3(1.00, 0.24, 0.36);
        float2 bossMin = float2(right - 300.0, 18.0);
        float2 bossMax = float2(right, 88.0);
        DrawGlassPanel(color, alpha, p, bossMin, bossMax, panelColor + bossColor * 0.03, bossColor, shimmerSeed + 39.0);
        DrawText(color, alpha, p, bossMin + float2(14.0, 22.0), scale, Word3(CH_B, CH_O, CH_S), 3u, bossColor * 0.35 + white * 0.72);
        DrawText(color, alpha, p, bossMin + float2(72.0, 22.0), scale, Word1(CH_P), 1u, bossColor);
        DrawNumber(color, alpha, p, bossMin + float2(88.0, 22.0), scale, gBossPhase, 1u, white);
        DrawNumber(color, alpha, p, bossMin + float2(126.0, 22.0), scale, gBossHpPercent, 3u, bossColor * 0.30 + white * 0.78);
        DrawText(color, alpha, p, bossMin + float2(162.0, 22.0), scale, Word1(CH_PCT), 1u, bossColor * 0.55 + white * 0.45);
        DrawBar(color, alpha, p, bossMin + float2(202.0, 25.0), 76.0, gBossHpPercent, bossColor);
    }

    if (gRunStatus > 0u) {
        DrawRunStatusToast(color, alpha, p);
    } else if (gRewardAppliedFlashPercent > 0u && RewardOptionActive(gRewardAppliedOption)) {
        DrawRewardAppliedToast(color, alpha, p, element);
    } else {
        DrawReactionToastAt(color, alpha, p, float2(logical.x * 0.5 - 95.0, 98.0), element, white, scale);
    }

    float2 loadoutMin = float2(18.0, bottom - 94.0);
    float2 loadoutMax = float2(294.0, bottom);
    DrawGlassPanel(color, alpha, p, loadoutMin, loadoutMax, panelColor, borderColor, shimmerSeed + 57.0);

    float badge = Diamond(p, loadoutMin + float2(18.0, 26.0), 12.0);
    BlendOverlay(color, alpha, element, badge * 0.85);
    BlendOverlay(color, alpha, float3(1.0, 1.0, 1.0), (Diamond(p, loadoutMin + float2(18.0, 26.0), 12.0) - Diamond(p, loadoutMin + float2(18.0, 26.0), 9.0)) * 0.5);
    DrawText(color, alpha, p, loadoutMin + float2(42.0, 17.0), scale, WeaponWord(gWeaponId), 12u, weapon * 0.35 + white * 0.8);
    DrawText(color, alpha, p, loadoutMin + float2(42.0, 42.0), 1.55, ElementWord(gElementId), 6u, element * 0.55 + white * 0.5);
    DrawLoadoutStrip(color, alpha, p, loadoutMin + float2(148.0, 39.0));

    uint qShape = min(gQShapeId, 7u);
    uint eShape = min(gEShapeId, 7u);
    float shapeScale = 1.25;
    float percentScale = 1.6;

    float2 actionMin = float2(right - 370.0, bottom - 132.0);
    float2 actionMax = float2(right, bottom);
    DrawGlassPanel(color, alpha, p, actionMin, actionMax, panelColor, borderColor, shimmerSeed + 75.0);

    DrawText(color, alpha, p, actionMin + float2(16.0, 28.0), scale, Word1(CH_Q), 1u, orange);
    DrawText(color, alpha, p, actionMin + float2(42.0, 28.0), scale, ActionWord(gWeaponId, 0u), 10u, white);
    DrawActionShapeBadge(color, alpha, p, actionMin + float2(178.0, 35.0), qShape, orange);
    DrawText(color, alpha, p, actionMin + float2(194.0, 29.0), shapeScale, ShapeWord(qShape), ShapeWordChars(qShape), orange * 0.35 + white * 0.68);
    DrawNumber(color, alpha, p, actionMin + float2(248.0, 29.0), percentScale, gQReadyPercent, 3u, orange);
    DrawText(color, alpha, p, actionMin + float2(278.0, 29.0), percentScale, Word1(CH_PCT), 1u, orange);
    DrawBar(color, alpha, p, actionMin + float2(16.0, 54.0), 338.0, gQReadyPercent, orange);

    DrawText(color, alpha, p, actionMin + float2(16.0, 86.0), scale, Word1(CH_E), 1u, violet);
    DrawText(color, alpha, p, actionMin + float2(42.0, 86.0), scale, ActionWord(gWeaponId, 1u), 10u, white);
    DrawActionShapeBadge(color, alpha, p, actionMin + float2(178.0, 93.0), eShape, violet);
    DrawText(color, alpha, p, actionMin + float2(194.0, 87.0), shapeScale, ShapeWord(eShape), ShapeWordChars(eShape), violet * 0.35 + white * 0.68);
    DrawNumber(color, alpha, p, actionMin + float2(248.0, 87.0), percentScale, gEReadyPercent, 3u, violet);
    DrawText(color, alpha, p, actionMin + float2(278.0, 87.0), percentScale, Word1(CH_PCT), 1u, violet);
    DrawBar(color, alpha, p, actionMin + float2(16.0, 112.0), 338.0, gEReadyPercent, violet);
}

float4 MainPS(PSIn input) : SV_Target0 {
    float4 dxrSample = gDxrOutput.SampleLevel(gLinearClamp, input.uv, 0.0);
    float3 color = dxrSample.rgb;
    float intensity = max(max(color.r, color.g), color.b);
    float alpha = saturate(max(dxrSample.a, intensity * 1.15));
    alpha = saturate(max(alpha, step(0.01, dxrSample.a) * 0.985));
    float2 centered = input.uv * 2.0 - 1.0;
    float descent = saturate((float)gDescentPercent * 0.01);
    float vignette = saturate(dot(centered, centered) * (0.28 + descent * 0.42));
    float2 renderDimensions = float2(max(gRenderWidth, 1u), max(gRenderHeight, 1u));
    float2 texel = 1.0 / renderDimensions;
    float2 dimensions = float2(max(gOutputWidth, 1u), max(gOutputHeight, 1u));
    color = lerp(color, color * float3(0.52, 0.42, 0.58), vignette * descent * 0.18);
    color = ApplyScenePost(color, input.uv, texel);
    DrawVfxSprites(color, alpha, input.uv * dimensions);

    float uiScale = max(0.75, min(dimensions.x / 1280.0, dimensions.y / 720.0));
    if (!ReferenceMode()) {
        DrawHud(color, alpha, input.uv * dimensions / uiScale);
    }
    return float4(color, 1.0);
}
