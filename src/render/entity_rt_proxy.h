#pragma once

#include "game/combat_sim.h"
#include "game/math.h"

#include <cstdint>
#include <span>
#include <vector>

namespace rogue {

enum class EntityProxyKind : uint8_t {
    PlayerCore,
    PlayerBlade,
    EnemyBrute,
    EnemyCaster,
    Projectile,
    ProjectileTrail,
    ControlSpell
};

struct EntityRTProxy {
    EntityProxyKind kind = EntityProxyKind::EnemyBrute;
    Vec3 position{};
    Vec3 direction{1.0f, 0.0f, 0.0f};
    float radius = 1.0f;
    uint32_t materialId = 0;
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
GeneratedRTGeometry GenerateWorldGeometry(const RoomGraph& world);
GeneratedRTGeometry GenerateRTGeometry(std::span<const EntityRTProxy> proxies);
PackedRTGeometry PackRTGeometry(const GeneratedRTGeometry& geometry);
uint32_t HashPackedRTGeometry(const PackedRTGeometry& geometry);

}
