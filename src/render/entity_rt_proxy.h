#pragma once

#include "game/combat_sim.h"
#include "game/math.h"

#include <cstdint>
#include <span>
#include <vector>

namespace rogue {

enum class EntityProxyKind : uint8_t {
    PlayerBlade,
    EnemyBrute,
    EnemyCaster,
    Projectile,
    ControlSpell
};

struct EntityRTProxy {
    EntityProxyKind kind = EntityProxyKind::EnemyBrute;
    Vec3 position{};
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

struct PackedRTGeometry {
    std::vector<PackedRtVertex> vertices;
    std::vector<uint32_t> indices;
};

std::vector<EntityRTProxy> BuildEntityProxies(const CombatSim& sim);
GeneratedRTGeometry GenerateRTGeometry(std::span<const EntityRTProxy> proxies);
PackedRTGeometry PackRTGeometry(const GeneratedRTGeometry& geometry);
uint32_t HashPackedRTGeometry(const PackedRTGeometry& geometry);

}
