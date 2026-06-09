#pragma once

#include "game/combat_sim.h"
#include "game/math.h"
#include "game/world_gen.h"
#include "render/entity_rt_proxy.h"
#include "render/visual_style.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace rogue {

constexpr uint32_t kMaxDxrMaterials = 15;
constexpr uint32_t kMaxRenderSprites = 128;

struct CameraParams {
    Vec3 position{0.0f, 18.0f, -14.0f};
    Vec3 target{0.0f, 0.0f, 0.0f};
    float tiltRadians = 0.92f;
};

struct DxrFrameConstants {
    float invViewProj[16]{};
    float cameraPosition[3]{};
    float timeSeconds = 0.0f;
    uint32_t frameIndex = 0;
    uint32_t materialCount = 0;
    uint32_t outputWidth = 0;
    uint32_t outputHeight = 0;
    uint32_t displayWidth = 0;
    uint32_t displayHeight = 0;
    uint32_t visualStyleIdentity = 0;
    uint32_t visualStyleSurface = 0;
    uint32_t visualStyleAtmosphere = 0;
    uint32_t visualStyleVariant = 0;
    uint32_t shotLayoutIdentity = 0;
    uint32_t shotLayoutWeights = 0;
    uint32_t renderQuality = 5;
    uint32_t reserved0 = 0;
};

struct RenderOverlayData {
    uint32_t overlayWeaponId = 0;
    uint32_t overlayElementId = 0;
    uint32_t overlayActiveSlot = 1;
    uint32_t overlayQReadyPercent = 100;
    uint32_t overlayEReadyPercent = 100;
    uint32_t overlayQActionShape = static_cast<uint32_t>(AttackShape::Cone);
    uint32_t overlayEActionShape = static_cast<uint32_t>(AttackShape::Cone);
    uint32_t overlayLoadoutSlot0 = 0;
    uint32_t overlayLoadoutSlot1 = 0;
    uint32_t overlayLoadoutSlot2 = 0;
    uint32_t overlayHp = 100;
    uint32_t overlayCurrentRoom = 1;
    uint32_t overlayRoomCount = 0;
    uint32_t overlayActiveEnemies = 0;
    uint32_t overlayBossHpPercent = 0;
    uint32_t overlayBossPhase = 0;
    uint32_t overlayObjectiveKind = 0;
    uint32_t overlayObjectiveProgressPercent = 0;
    uint32_t overlayRunStatus = 0;
    uint32_t overlayPlayerStatusMask = 0;
    uint32_t overlayFloorIndex = 0;
    uint32_t overlayDescentPercent = 0;
};

constexpr uint32_t kLoadoutOverlayActiveBit = 0x80000000u;
constexpr uint32_t kLoadoutOverlayWeaponShift = 0u;
constexpr uint32_t kLoadoutOverlayElementShift = 4u;
constexpr uint32_t kLoadoutOverlayQReadyShift = 7u;
constexpr uint32_t kLoadoutOverlayEReadyShift = 14u;

inline uint32_t PackLoadoutOverlaySlot(
    WeaponId weapon,
    Element element,
    uint32_t qReadyPercent,
    uint32_t eReadyPercent,
    bool active) {
    const uint32_t qReady = qReadyPercent > 100u ? 100u : qReadyPercent;
    const uint32_t eReady = eReadyPercent > 100u ? 100u : eReadyPercent;
    return (active ? kLoadoutOverlayActiveBit : 0u) |
        ((static_cast<uint32_t>(weapon) & 0xfu) << kLoadoutOverlayWeaponShift) |
        ((static_cast<uint32_t>(element) & 0x7u) << kLoadoutOverlayElementShift) |
        ((qReady & 0x7fu) << kLoadoutOverlayQReadyShift) |
        ((eReady & 0x7fu) << kLoadoutOverlayEReadyShift);
}

inline bool LoadoutOverlaySlotActive(uint32_t packed) {
    return (packed & kLoadoutOverlayActiveBit) != 0u;
}

inline WeaponId LoadoutOverlaySlotWeapon(uint32_t packed) {
    return static_cast<WeaponId>((packed >> kLoadoutOverlayWeaponShift) & 0xfu);
}

inline Element LoadoutOverlaySlotElement(uint32_t packed) {
    return static_cast<Element>((packed >> kLoadoutOverlayElementShift) & 0x7u);
}

inline uint32_t LoadoutOverlaySlotQReady(uint32_t packed) {
    return (packed >> kLoadoutOverlayQReadyShift) & 0x7fu;
}

inline uint32_t LoadoutOverlaySlotEReady(uint32_t packed) {
    return (packed >> kLoadoutOverlayEReadyShift) & 0x7fu;
}

constexpr uint32_t kOverlayStatusWetBit = 1u << 0u;
constexpr uint32_t kOverlayStatusBurningBit = 1u << 1u;
constexpr uint32_t kOverlayStatusChargedBit = 1u << 2u;
constexpr uint32_t kOverlayStatusChilledBit = 1u << 3u;

constexpr uint32_t OverlayStatusBit(StatusKind kind) {
    switch (kind) {
    case StatusKind::Wet:
        return kOverlayStatusWetBit;
    case StatusKind::Burning:
        return kOverlayStatusBurningBit;
    case StatusKind::Charged:
        return kOverlayStatusChargedBit;
    case StatusKind::Chilled:
        return kOverlayStatusChilledBit;
    case StatusKind::None:
        return 0u;
    }
    return 0u;
}

struct EntityMaterial {
    Vec3 baseColor{};
    float emission = 0.0f;
};

struct RenderSprite {
    float positionSize[4]{}; // screen x, screen y, radius in pixels, rotation radians
    float colorAlpha[4]{};   // linear rgb + alpha
    uint32_t atlasIndex = 0;
    uint32_t flags = 0;
    uint32_t seed = 0;
    uint32_t reserved = 0;
};

struct RenderScene {
    CameraParams camera{};
    DxrFrameConstants frame{};
    RenderOverlayData overlay{};
    RoomVisualStyle visualStyle{};
    VisualStylePacked visualStylePacked{};
    ShotLayout shotLayout{};
    ShotLayoutPacked shotLayoutPacked{};
    RoomGraph world{};
    std::vector<EntityRTProxy> proxies;
    GeneratedRTGeometry generatedGeometry;
    PackedRTGeometry packedGeometry;
    std::array<RenderSprite, kMaxRenderSprites> sprites{};
    std::array<EntityMaterial, kMaxDxrMaterials> materials{};
    uint32_t materialCount = 0;
    uint32_t spriteCount = 0;
    uint32_t geometryHash = 0;
};

RenderScene BuildRenderScene(
    const RoomGraph& world,
    const CombatSim& combat,
    uint32_t outputWidth,
    uint32_t outputHeight,
    float timeSeconds,
    uint32_t frameIndex,
    std::span<const RenderVfxPulse> transientVfx = {},
    uint32_t runStatus = 0,
    uint32_t displayWidth = 0,
    uint32_t displayHeight = 0,
    uint32_t renderQuality = 5,
    bool referenceTarget = false);

}

