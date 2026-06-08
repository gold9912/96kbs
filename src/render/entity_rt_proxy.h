#pragma once

#include "game/combat_sim.h"
#include "game/math.h"
#include "render/visual_style.h"

#include <cstdint>
#include <span>
#include <vector>

namespace rogue {

enum class EntityProxyKind : uint8_t {
    PlayerCore,
    PlayerBlade,
    PlayerPrimaryGuide,
    PlayerAbilityLine,
    PlayerAbilityRing,
    PlayerAbilityCooldown,
    PlayerActionCone,
    PlayerActionLine,
    PlayerActionRing,
    PlayerActionBurst,
    EnemyBrute,
    EnemyCaster,
    EnemySkirmisher,
    EnemyBulwark,
    EnemyBoss,
    EnemyHealthBack,
    EnemyHealthFill,
    EnemyShieldFill,
    EnemyStatusPip,
    EnemyTellCone,
    EnemyTellLine,
    EnemyTellRing,
    Projectile,
    ProjectileTrail,
    ControlSpell,
    HitSpark,
    RoomClearPulse,
    PortalPulse
};

enum class RenderVfxKind : uint8_t {
    HitSpark,
    RoomClearPulse,
    PortalPulse,
    WeaponCone,
    WeaponLine,
    WeaponRing,
    WeaponBurst
};

constexpr int kMaxRenderVfxPulses = 32;

struct RenderVfxPulse {
    RenderVfxKind kind = RenderVfxKind::HitSpark;
    Vec3 position{};
    float radius = 0.0f;
    float ttl = 0.0f;
    float duration = 0.0f;
    float intensity = 1.0f;
    Vec3 direction{1.0f, 0.0f, 0.0f};
};

struct EntityRTProxy {
    EntityProxyKind kind = EntityProxyKind::EnemyBrute;
    Vec3 position{};
    Vec3 direction{1.0f, 0.0f, 0.0f};
    float radius = 1.0f;
    uint32_t materialId = 0;
    float phase = 0.0f;
    uint32_t visualTag = 0;
};

struct RtVertex {
    Vec3 position{};
    Vec3 normal{};
};

struct RtTriangle {
    RtVertex a{};
    RtVertex b{};
    RtVertex c{};
    uint32_t materialId = 0;
};

struct GeneratedRTGeometry {
    std::vector<RtTriangle> triangles;
};

struct PackedRtVertex {
    Vec3 position{};
    Vec3 normal{};
    uint32_t materialId = 0;
};

struct RtTriangleMetadata {
    Vec3 normal{};
    uint32_t materialId = 0;
};

struct PackedRTGeometry {
    std::vector<PackedRtVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<RtTriangleMetadata> triangleMetadata;
};

std::vector<EntityRTProxy> BuildEntityProxies(const CombatSim& sim);
std::vector<EntityRTProxy> BuildVfxProxies(std::span<const RenderVfxPulse> pulses);
GeneratedRTGeometry GenerateWorldGeometry(const RoomGraph& world, const RoomVisualStyle* style = nullptr, int focusRoomIndex = -1);
GeneratedRTGeometry GenerateRTGeometry(std::span<const EntityRTProxy> proxies);
PackedRTGeometry PackRTGeometry(const GeneratedRTGeometry& geometry);
uint32_t HashPackedRTGeometry(const PackedRTGeometry& geometry);

}
