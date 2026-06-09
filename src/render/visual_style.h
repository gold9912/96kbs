#pragma once

#include "game/math.h"
#include "game/world_gen.h"

#include <cstdint>

namespace rogue {

enum class VisualBiome : uint8_t {
    SunlitRuins,
    OvergrownSanctuary,
    ArcaneLibrary,
    AbyssCrypt
};

enum class VisualStyleColorRole : uint8_t {
    Floor,
    Wall,
    Corridor,
    Fog,
    Hud,
    Light,
    Danger,
    Control
};

struct RoomVisualStyle {
    VisualBiome biome = VisualBiome::SunlitRuins;
    uint32_t floorIndex = 0;
    uint32_t paletteId = 0;
    uint32_t floorPatternId = 0;
    uint32_t propGrammarId = 0;
    uint32_t lightRigId = 0;
    float descent = 0.0f;
    float moss = 0.0f;
    float wetness = 0.0f;
    float cracks = 0.0f;
    float decay = 0.0f;
    float corruption = 0.0f;
    float glow = 0.0f;
    float fog = 0.0f;
};

struct VisualStylePacked {
    uint32_t identity = 0;
    uint32_t surface = 0;
    uint32_t atmosphere = 0;
    uint32_t variant = 0;
};

struct ShotLayout {
    uint32_t keyLightSide = 0;
    uint32_t windowFrame = 1;
    uint32_t foregroundMask = 1;
    float combatClearRadius = 0.64f;
    float edgeDensity = 0.76f;
    float foliageDensity = 0.44f;
    float heroVfxBias = 0.72f;
    float warmCoolContrast = 0.78f;
};

struct ShotLayoutPacked {
    uint32_t identity = 0;
    uint32_t weights = 0;
};

constexpr uint32_t kVisualStyleBiomeShift = 0u;
constexpr uint32_t kVisualStylePaletteShift = 4u;
constexpr uint32_t kVisualStyleFloorPatternShift = 8u;
constexpr uint32_t kVisualStylePropGrammarShift = 12u;
constexpr uint32_t kVisualStyleLightRigShift = 16u;

constexpr uint32_t kVisualStyleMossShift = 0u;
constexpr uint32_t kVisualStyleWetnessShift = 4u;
constexpr uint32_t kVisualStyleCracksShift = 8u;
constexpr uint32_t kVisualStyleDecayShift = 12u;

constexpr uint32_t kVisualStyleCorruptionShift = 0u;
constexpr uint32_t kVisualStyleGlowShift = 4u;
constexpr uint32_t kVisualStyleFogShift = 8u;
constexpr uint32_t kVisualStyleDescentShift = 12u;

constexpr uint32_t kShotLayoutKeyLightSideShift = 0u;
constexpr uint32_t kShotLayoutWindowFrameShift = 4u;
constexpr uint32_t kShotLayoutForegroundMaskShift = 8u;

constexpr uint32_t kShotLayoutCombatClearShift = 0u;
constexpr uint32_t kShotLayoutEdgeDensityShift = 4u;
constexpr uint32_t kShotLayoutFoliageDensityShift = 8u;
constexpr uint32_t kShotLayoutHeroVfxBiasShift = 12u;
constexpr uint32_t kShotLayoutWarmCoolContrastShift = 16u;

RoomVisualStyle BuildVisualStyle(const RoomGraph& world, int activeRoom);
VisualStylePacked PackVisualStyle(const RoomVisualStyle& style);
uint32_t VisualStyleHash(const RoomVisualStyle& style);
Vec3 VisualStyleColor(const RoomVisualStyle& style, VisualStyleColorRole role);
ShotLayout BuildShotLayout(const RoomGraph& world, int activeRoom, const RoomVisualStyle& style);
ShotLayoutPacked PackShotLayout(const ShotLayout& layout);

uint32_t VisualStylePackedNibble(uint32_t packed, uint32_t shift);
float VisualStylePackedWeight(uint32_t packed, uint32_t shift);
VisualBiome PackedVisualStyleBiome(const VisualStylePacked& packed);
uint32_t PackedVisualStylePalette(const VisualStylePacked& packed);
uint32_t PackedVisualStyleFloorPattern(const VisualStylePacked& packed);
uint32_t PackedVisualStylePropGrammar(const VisualStylePacked& packed);
uint32_t PackedVisualStyleLightRig(const VisualStylePacked& packed);
float PackedVisualStyleDescent(const VisualStylePacked& packed);
uint32_t ShotLayoutPackedNibble(uint32_t packed, uint32_t shift);
float ShotLayoutPackedWeight(uint32_t packed, uint32_t shift);

}
