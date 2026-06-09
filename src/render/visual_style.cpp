#include "render/visual_style.h"

#include <algorithm>
#include <cmath>

namespace rogue {

namespace {

uint32_t HashMix(uint32_t h, uint32_t v) {
    h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}

uint32_t QuantizeWeight(float value) {
    return static_cast<uint32_t>(std::lround(Clamp(value, 0.0f, 1.0f) * 15.0f)) & 0xfu;
}

float HashUnit(uint32_t hash) {
    hash ^= hash >> 16u;
    hash *= 0x7feb352du;
    hash ^= hash >> 15u;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16u;
    return static_cast<float>(hash & 0xffffu) / 65535.0f;
}

Vec3 Mix(Vec3 a, Vec3 b, float t) {
    const float u = Clamp(t, 0.0f, 1.0f);
    return Vec3{
        a.x + (b.x - a.x) * u,
        a.y + (b.y - a.y) * u,
        a.z + (b.z - a.z) * u
    };
}

VisualBiome ChooseBiome(const RoomGraph& world, int roomIndex, uint32_t hash) {
    const float descent = Clamp(world.descent, 0.0f, 1.0f);
    if (roomIndex <= 0) {
        if (descent > 0.82f) {
            return VisualBiome::AbyssCrypt;
        }
        if (descent > 0.48f) {
            return VisualBiome::ArcaneLibrary;
        }
        if (descent > 0.18f) {
            return VisualBiome::OvergrownSanctuary;
        }
        return VisualBiome::SunlitRuins;
    }
    if (roomIndex == world.roomCount - 1) {
        return VisualBiome::AbyssCrypt;
    }

    const Room& room = world.rooms[roomIndex];
    if (room.objective.kind == RoomObjectiveKind::ControlPoint) {
        return VisualBiome::ArcaneLibrary;
    }
    if (room.objective.kind == RoomObjectiveKind::SurviveTimer) {
        return VisualBiome::OvergrownSanctuary;
    }

    const uint32_t roll = hash % 4u;
    if (descent < 0.18f) {
        if (roomIndex <= 1) {
            return VisualBiome::SunlitRuins;
        }
        return (roll & 1u) == 0u ? VisualBiome::SunlitRuins : VisualBiome::OvergrownSanctuary;
    }
    if (descent > 0.70f && roll != 0u) {
        return VisualBiome::AbyssCrypt;
    }
    if (descent > 0.42f && roll >= 1u) {
        return roll == 1u ? VisualBiome::ArcaneLibrary : VisualBiome::AbyssCrypt;
    }
    return roll == 0u ? VisualBiome::SunlitRuins :
        (roll == 1u ? VisualBiome::OvergrownSanctuary :
        (roll == 2u ? VisualBiome::ArcaneLibrary : VisualBiome::AbyssCrypt));
}

float RoomMaterialSeed(const RoomGraph& world, int roomIndex) {
    if (roomIndex < 0 || roomIndex >= world.roomCount) {
        return 0.0f;
    }
    return world.sdfRooms[roomIndex].materialSeed;
}

uint32_t FloatToken(float value) {
    return static_cast<uint32_t>(std::lround(Clamp(value, 0.0f, 1.0f) * 4095.0f));
}

}

RoomVisualStyle BuildVisualStyle(const RoomGraph& world, int activeRoom) {
    const int roomIndex = activeRoom >= 0 && activeRoom < world.roomCount ? activeRoom : 0;
    uint32_t hash = HashMix(0x96f00du, world.seed);
    hash = HashMix(hash, static_cast<uint32_t>(roomIndex));
    hash = HashMix(hash, FloatToken(RoomMaterialSeed(world, roomIndex)));
    if (roomIndex >= 0 && roomIndex < world.roomCount) {
        hash = HashMix(hash, static_cast<uint32_t>(world.rooms[roomIndex].objective.kind));
        hash = HashMix(hash, static_cast<uint32_t>(world.rooms[roomIndex].lifecycle));
    }

    RoomVisualStyle style{};
    style.biome = ChooseBiome(world, roomIndex, hash);
    style.floorIndex = static_cast<uint32_t>(std::max(0, world.floorIndex));
    style.paletteId = (hash >> 4u) & 0x3u;
    style.floorPatternId = (hash >> 8u) & 0x3u;
    style.propGrammarId = static_cast<uint32_t>(style.biome);
    style.lightRigId = static_cast<uint32_t>(style.biome);
    style.descent = Clamp(world.descent, 0.0f, 1.0f);

    const float jitterA = HashUnit(HashMix(hash, 0xabc123u));
    const float jitterB = HashUnit(HashMix(hash, 0xdef456u));

    switch (style.biome) {
    case VisualBiome::SunlitRuins:
        style.moss = 0.22f + jitterA * 0.16f;
        style.wetness = 0.14f + jitterB * 0.08f;
        style.cracks = 0.42f + jitterA * 0.28f;
        style.decay = 0.30f + jitterB * 0.22f;
        style.corruption = 0.04f + jitterB * 0.08f;
        style.glow = 0.30f + jitterA * 0.16f;
        style.fog = 0.12f + jitterB * 0.10f;
        break;
    case VisualBiome::OvergrownSanctuary:
        style.moss = 0.68f + jitterA * 0.22f;
        style.wetness = 0.36f + jitterB * 0.20f;
        style.cracks = 0.38f + jitterB * 0.20f;
        style.decay = 0.45f + jitterA * 0.24f;
        style.corruption = 0.08f + jitterA * 0.10f;
        style.glow = 0.36f + jitterB * 0.18f;
        style.fog = 0.20f + jitterA * 0.16f;
        break;
    case VisualBiome::ArcaneLibrary:
        style.moss = 0.14f + jitterB * 0.12f;
        style.wetness = 0.24f + jitterA * 0.16f;
        style.cracks = 0.54f + jitterB * 0.24f;
        style.decay = 0.58f + jitterA * 0.24f;
        style.corruption = 0.28f + jitterB * 0.18f;
        style.glow = 0.62f + jitterA * 0.18f;
        style.fog = 0.34f + jitterB * 0.20f;
        break;
    case VisualBiome::AbyssCrypt:
        style.moss = 0.04f + jitterA * 0.08f;
        style.wetness = 0.58f + jitterB * 0.26f;
        style.cracks = 0.70f + jitterA * 0.24f;
        style.decay = 0.74f + jitterB * 0.18f;
        style.corruption = 0.72f + jitterA * 0.24f;
        style.glow = 0.76f + jitterB * 0.20f;
        style.fog = 0.56f + jitterA * 0.26f;
        break;
    }

    style.moss = Clamp(style.moss * (1.0f - style.descent * 0.48f), 0.0f, 1.0f);
    style.wetness = Clamp(style.wetness + style.descent * 0.16f, 0.0f, 1.0f);
    style.cracks = Clamp(style.cracks + style.descent * 0.18f, 0.0f, 1.0f);
    style.decay = Clamp(style.decay + style.descent * 0.22f, 0.0f, 1.0f);
    style.corruption = Clamp(style.corruption + style.descent * 0.36f, 0.0f, 1.0f);
    style.glow = Clamp(style.glow + style.descent * 0.14f, 0.0f, 1.0f);
    style.fog = Clamp(style.fog + style.descent * 0.26f, 0.0f, 1.0f);

    return style;
}

VisualStylePacked PackVisualStyle(const RoomVisualStyle& style) {
    VisualStylePacked packed{};
    packed.identity =
        ((static_cast<uint32_t>(style.biome) & 0xfu) << kVisualStyleBiomeShift) |
        ((style.paletteId & 0xfu) << kVisualStylePaletteShift) |
        ((style.floorPatternId & 0xfu) << kVisualStyleFloorPatternShift) |
        ((style.propGrammarId & 0xfu) << kVisualStylePropGrammarShift) |
        ((style.lightRigId & 0xfu) << kVisualStyleLightRigShift);
    packed.surface =
        (QuantizeWeight(style.moss) << kVisualStyleMossShift) |
        (QuantizeWeight(style.wetness) << kVisualStyleWetnessShift) |
        (QuantizeWeight(style.cracks) << kVisualStyleCracksShift) |
        (QuantizeWeight(style.decay) << kVisualStyleDecayShift);
    packed.atmosphere =
        (QuantizeWeight(style.corruption) << kVisualStyleCorruptionShift) |
        (QuantizeWeight(style.glow) << kVisualStyleGlowShift) |
        (QuantizeWeight(style.fog) << kVisualStyleFogShift) |
        (QuantizeWeight(style.descent) << kVisualStyleDescentShift);
    packed.variant = VisualStyleHash(style);
    return packed;
}

ShotLayout BuildShotLayout(const RoomGraph& world, int activeRoom, const RoomVisualStyle& style) {
    const int roomIndex = activeRoom >= 0 && activeRoom < world.roomCount ? activeRoom : 0;
    uint32_t hash = HashMix(0x5107f00du, world.seed);
    hash = HashMix(hash, static_cast<uint32_t>(roomIndex));
    hash = HashMix(hash, static_cast<uint32_t>(style.biome));
    hash = HashMix(hash, style.floorIndex);
    hash = HashMix(hash, style.paletteId);

    ShotLayout layout{};
    layout.keyLightSide = style.biome <= VisualBiome::OvergrownSanctuary ? 0u : 2u;
    layout.windowFrame = style.biome <= VisualBiome::OvergrownSanctuary ? 1u : 2u;
    layout.foregroundMask = style.descent > 0.55f ? 2u : 1u;

    const float jitterA = HashUnit(HashMix(hash, 0x81u)) - 0.5f;
    const float jitterB = HashUnit(HashMix(hash, 0x97u)) - 0.5f;
    const float warmBiome = style.biome <= VisualBiome::OvergrownSanctuary ? 1.0f : 0.0f;
    const float lush = style.biome == VisualBiome::OvergrownSanctuary ? 1.0f : 0.0f;
    const float deep = style.biome == VisualBiome::AbyssCrypt ? 1.0f : 0.0f;

    layout.combatClearRadius = Clamp(0.62f + style.descent * 0.08f - style.fog * 0.04f, 0.48f, 0.82f);
    layout.edgeDensity = Clamp(0.68f + style.decay * 0.16f + style.fog * 0.10f + jitterA * 0.05f, 0.42f, 1.0f);
    layout.foliageDensity = Clamp(0.22f + style.moss * 0.72f + lush * 0.14f - style.descent * 0.20f + jitterB * 0.04f, 0.05f, 1.0f);
    layout.heroVfxBias = Clamp(0.70f + style.glow * 0.18f + deep * 0.04f, 0.54f, 1.0f);
    layout.warmCoolContrast = Clamp(0.62f + warmBiome * 0.20f + style.wetness * 0.10f + style.corruption * 0.08f, 0.48f, 1.0f);
    return layout;
}

ShotLayoutPacked PackShotLayout(const ShotLayout& layout) {
    ShotLayoutPacked packed{};
    packed.identity =
        ((layout.keyLightSide & 0xfu) << kShotLayoutKeyLightSideShift) |
        ((layout.windowFrame & 0xfu) << kShotLayoutWindowFrameShift) |
        ((layout.foregroundMask & 0xfu) << kShotLayoutForegroundMaskShift);
    packed.weights =
        (QuantizeWeight(layout.combatClearRadius) << kShotLayoutCombatClearShift) |
        (QuantizeWeight(layout.edgeDensity) << kShotLayoutEdgeDensityShift) |
        (QuantizeWeight(layout.foliageDensity) << kShotLayoutFoliageDensityShift) |
        (QuantizeWeight(layout.heroVfxBias) << kShotLayoutHeroVfxBiasShift) |
        (QuantizeWeight(layout.warmCoolContrast) << kShotLayoutWarmCoolContrastShift);
    return packed;
}

uint32_t VisualStyleHash(const RoomVisualStyle& style) {
    uint32_t hash = HashMix(0x7651f00du, static_cast<uint32_t>(style.biome));
    hash = HashMix(hash, style.paletteId);
    hash = HashMix(hash, style.floorIndex);
    hash = HashMix(hash, style.floorPatternId);
    hash = HashMix(hash, style.propGrammarId);
    hash = HashMix(hash, style.lightRigId);
    hash = HashMix(hash, QuantizeWeight(style.moss));
    hash = HashMix(hash, QuantizeWeight(style.wetness));
    hash = HashMix(hash, QuantizeWeight(style.cracks));
    hash = HashMix(hash, QuantizeWeight(style.decay));
    hash = HashMix(hash, QuantizeWeight(style.corruption));
    hash = HashMix(hash, QuantizeWeight(style.glow));
    hash = HashMix(hash, QuantizeWeight(style.fog));
    hash = HashMix(hash, QuantizeWeight(style.descent));
    return hash;
}

Vec3 VisualStyleColor(const RoomVisualStyle& style, VisualStyleColorRole role) {
    const float variant = static_cast<float>(style.paletteId & 0x3u) * 0.035f;
    switch (style.biome) {
    case VisualBiome::SunlitRuins:
        switch (role) {
        case VisualStyleColorRole::Floor: return Mix(Vec3{0.148f, 0.146f, 0.128f}, Vec3{0.060f, 0.094f, 0.055f}, style.moss * 0.14f + variant * 0.10f);
        case VisualStyleColorRole::Wall: return Vec3{0.118f + variant * 0.035f, 0.104f, 0.080f};
        case VisualStyleColorRole::Corridor: return Vec3{0.074f, 0.082f + variant * 0.020f, 0.076f};
        case VisualStyleColorRole::Fog: return Vec3{0.105f, 0.088f, 0.052f};
        case VisualStyleColorRole::Hud: return Vec3{0.24f, 0.96f, 0.84f};
        case VisualStyleColorRole::Light: return Vec3{1.00f, 0.72f, 0.34f};
        case VisualStyleColorRole::Danger: return Vec3{0.88f, 0.18f, 0.08f};
        case VisualStyleColorRole::Control: return Vec3{0.24f, 0.80f, 0.62f};
        }
        break;
    case VisualBiome::OvergrownSanctuary:
        switch (role) {
        case VisualStyleColorRole::Floor: return Mix(Vec3{0.092f, 0.094f, 0.080f}, Vec3{0.030f, 0.088f, 0.040f}, style.moss * 0.24f + variant * 0.16f);
        case VisualStyleColorRole::Wall: return Vec3{0.076f, 0.070f + variant * 0.065f, 0.052f};
        case VisualStyleColorRole::Corridor: return Vec3{0.036f, 0.060f, 0.052f + variant * 0.060f};
        case VisualStyleColorRole::Fog: return Vec3{0.030f, 0.066f, 0.052f};
        case VisualStyleColorRole::Hud: return Vec3{0.22f, 0.95f, 0.76f};
        case VisualStyleColorRole::Light: return Vec3{0.95f, 0.70f, 0.36f};
        case VisualStyleColorRole::Danger: return Vec3{0.78f, 0.22f, 0.12f};
        case VisualStyleColorRole::Control: return Vec3{0.14f, 0.74f, 0.50f};
        }
        break;
    case VisualBiome::ArcaneLibrary:
        switch (role) {
        case VisualStyleColorRole::Floor: return Vec3{0.135f + variant * 0.45f, 0.112f, 0.105f};
        case VisualStyleColorRole::Wall: return Vec3{0.155f, 0.090f, 0.080f + variant * 0.45f};
        case VisualStyleColorRole::Corridor: return Vec3{0.110f, 0.082f, 0.102f + variant * 0.42f};
        case VisualStyleColorRole::Fog: return Vec3{0.068f, 0.042f, 0.082f};
        case VisualStyleColorRole::Hud: return Vec3{0.42f, 0.92f, 0.94f};
        case VisualStyleColorRole::Light: return Vec3{1.00f, 0.48f, 0.18f};
        case VisualStyleColorRole::Danger: return Vec3{0.94f, 0.20f, 0.62f};
        case VisualStyleColorRole::Control: return Vec3{0.74f, 0.48f, 1.00f};
        }
        break;
    case VisualBiome::AbyssCrypt:
        switch (role) {
        case VisualStyleColorRole::Floor: return Vec3{0.145f, 0.130f + variant * 0.18f, 0.165f + variant * 0.30f};
        case VisualStyleColorRole::Wall: return Vec3{0.145f + variant * 0.28f, 0.060f, 0.075f};
        case VisualStyleColorRole::Corridor: return Vec3{0.085f, 0.092f, 0.128f + variant * 0.28f};
        case VisualStyleColorRole::Fog: return Vec3{0.030f, 0.035f, 0.060f};
        case VisualStyleColorRole::Hud: return Vec3{0.26f, 0.88f, 1.00f};
        case VisualStyleColorRole::Light: return Vec3{0.50f, 0.18f, 1.00f};
        case VisualStyleColorRole::Danger: return Vec3{1.00f, 0.10f, 0.18f};
        case VisualStyleColorRole::Control: return Vec3{0.20f, 0.80f, 1.00f};
        }
        break;
    }
    return Vec3{0.2f, 0.2f, 0.2f};
}

uint32_t VisualStylePackedNibble(uint32_t packed, uint32_t shift) {
    return (packed >> shift) & 0xfu;
}

float VisualStylePackedWeight(uint32_t packed, uint32_t shift) {
    return static_cast<float>(VisualStylePackedNibble(packed, shift)) / 15.0f;
}

VisualBiome PackedVisualStyleBiome(const VisualStylePacked& packed) {
    return static_cast<VisualBiome>(VisualStylePackedNibble(packed.identity, kVisualStyleBiomeShift));
}

uint32_t PackedVisualStylePalette(const VisualStylePacked& packed) {
    return VisualStylePackedNibble(packed.identity, kVisualStylePaletteShift);
}

uint32_t PackedVisualStyleFloorPattern(const VisualStylePacked& packed) {
    return VisualStylePackedNibble(packed.identity, kVisualStyleFloorPatternShift);
}

uint32_t PackedVisualStylePropGrammar(const VisualStylePacked& packed) {
    return VisualStylePackedNibble(packed.identity, kVisualStylePropGrammarShift);
}

uint32_t PackedVisualStyleLightRig(const VisualStylePacked& packed) {
    return VisualStylePackedNibble(packed.identity, kVisualStyleLightRigShift);
}

float PackedVisualStyleDescent(const VisualStylePacked& packed) {
    return VisualStylePackedWeight(packed.atmosphere, kVisualStyleDescentShift);
}

uint32_t ShotLayoutPackedNibble(uint32_t packed, uint32_t shift) {
    return (packed >> shift) & 0xfu;
}

float ShotLayoutPackedWeight(uint32_t packed, uint32_t shift) {
    return static_cast<float>(ShotLayoutPackedNibble(packed, shift)) / 15.0f;
}

}
