#include "render/entity_rt_proxy.h"

#include <cmath>
#include <cstring>

namespace rogue {

namespace {

constexpr uint32_t kMaterialFloor = 0;
constexpr uint32_t kMaterialWall = 1;
constexpr uint32_t kMaterialPlayerCore = 2;
constexpr uint32_t kMaterialPlayerBlade = 3;
constexpr uint32_t kMaterialEnemyBrute = 4;
constexpr uint32_t kMaterialEnemyCaster = 5;
constexpr uint32_t kMaterialProjectile = 6;
constexpr uint32_t kMaterialPortal = 7;
constexpr uint32_t kMaterialControl = 8;
constexpr uint32_t kMaterialCorridor = 9;

Vec3 MakeVec3(Vec2 p, float z) {
    return Vec3{p.x, z, p.y};
}

Vec3 Sub(Vec3 a, Vec3 b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Cross(Vec3 a, Vec3 b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vec3 Normalize(Vec3 v) {
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lenSq <= 0.000001f) {
        return Vec3{0.0f, 1.0f, 0.0f};
    }
    const float invLen = 1.0f / std::sqrt(lenSq);
    return Vec3{v.x * invLen, v.y * invLen, v.z * invLen};
}

Vec2 Normalize2(Vec2 v, Vec2 fallback) {
    const float lenSq = v.x * v.x + v.y * v.y;
    if (lenSq <= 0.000001f) {
        return fallback;
    }
    const float invLen = 1.0f / std::sqrt(lenSq);
    return Vec2{v.x * invLen, v.y * invLen};
}

Vec3 Scale(Vec3 v, float s) {
    return Vec3{v.x * s, v.y * s, v.z * s};
}

Vec3 Add(Vec3 a, Vec3 b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

uint32_t HashMix(uint32_t h, uint32_t v) {
    h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}

uint32_t HashFloat(float v) {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(v));
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

void AddTri(GeneratedRTGeometry& geo, Vec3 a, Vec3 b, Vec3 c, uint32_t materialId) {
    const Vec3 n = Normalize(Cross(Sub(b, a), Sub(c, a)));
    geo.triangles.push_back(RtTriangle{
        RtVertex{a, n},
        RtVertex{b, n},
        RtVertex{c, n},
        materialId
    });
}

void AddQuad(GeneratedRTGeometry& geo, Vec3 a, Vec3 b, Vec3 c, Vec3 d, uint32_t materialId) {
    AddTri(geo, a, b, c, materialId);
    AddTri(geo, a, c, d, materialId);
}

void AddBox(GeneratedRTGeometry& geo, Vec3 minP, Vec3 maxP, uint32_t materialId) {
    const Vec3 p000{minP.x, minP.y, minP.z};
    const Vec3 p001{minP.x, minP.y, maxP.z};
    const Vec3 p010{minP.x, maxP.y, minP.z};
    const Vec3 p011{minP.x, maxP.y, maxP.z};
    const Vec3 p100{maxP.x, minP.y, minP.z};
    const Vec3 p101{maxP.x, minP.y, maxP.z};
    const Vec3 p110{maxP.x, maxP.y, minP.z};
    const Vec3 p111{maxP.x, maxP.y, maxP.z};

    AddQuad(geo, p000, p100, p110, p010, materialId);
    AddQuad(geo, p101, p001, p011, p111, materialId);
    AddQuad(geo, p001, p000, p010, p011, materialId);
    AddQuad(geo, p100, p101, p111, p110, materialId);
    AddQuad(geo, p010, p110, p111, p011, materialId);
    AddQuad(geo, p001, p101, p100, p000, materialId);
}

void AddFloor(GeneratedRTGeometry& geo, Vec2 center, Vec2 halfSize, uint32_t materialId) {
    const float y = -0.03f;
    const Vec3 a{center.x - halfSize.x, y, center.y - halfSize.y};
    const Vec3 b{center.x + halfSize.x, y, center.y - halfSize.y};
    const Vec3 c{center.x + halfSize.x, y, center.y + halfSize.y};
    const Vec3 d{center.x - halfSize.x, y, center.y + halfSize.y};
    AddQuad(geo, a, b, c, d, materialId);
}

void AddRoomBorders(GeneratedRTGeometry& geo, const Room& room) {
    constexpr float kThickness = 0.34f;
    constexpr float kHeight = 0.52f;
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;
    AddBox(geo, Vec3{x0 - kThickness, 0.0f, z0 - kThickness}, Vec3{x1 + kThickness, kHeight, z0}, kMaterialWall);
    AddBox(geo, Vec3{x0 - kThickness, 0.0f, z1}, Vec3{x1 + kThickness, kHeight, z1 + kThickness}, kMaterialWall);
    AddBox(geo, Vec3{x0 - kThickness, 0.0f, z0}, Vec3{x0, kHeight, z1}, kMaterialWall);
    AddBox(geo, Vec3{x1, 0.0f, z0}, Vec3{x1 + kThickness, kHeight, z1}, kMaterialWall);
}

void AddCorridor(GeneratedRTGeometry& geo, Vec2 a, Vec2 b, float halfWidth) {
    const Vec2 dir = Normalize2(b - a, Vec2{1.0f, 0.0f});
    const Vec2 right{-dir.y, dir.x};
    const Vec2 p0 = a + right * halfWidth;
    const Vec2 p1 = b + right * halfWidth;
    const Vec2 p2 = b - right * halfWidth;
    const Vec2 p3 = a - right * halfWidth;
    const float y = -0.02f;
    AddQuad(
        geo,
        Vec3{p0.x, y, p0.y},
        Vec3{p1.x, y, p1.y},
        Vec3{p2.x, y, p2.y},
        Vec3{p3.x, y, p3.y},
        kMaterialCorridor);
}

void AddPyramid(GeneratedRTGeometry& geo, Vec3 center, float radius, float height, uint32_t materialId) {
    const Vec3 top{center.x, center.y + height, center.z};
    const Vec3 p0{center.x - radius, center.y, center.z - radius};
    const Vec3 p1{center.x + radius, center.y, center.z - radius};
    const Vec3 p2{center.x + radius, center.y, center.z + radius};
    const Vec3 p3{center.x - radius, center.y, center.z + radius};
    AddTri(geo, p0, p1, top, materialId);
    AddTri(geo, p1, p2, top, materialId);
    AddTri(geo, p2, p3, top, materialId);
    AddTri(geo, p3, p0, top, materialId);
    AddTri(geo, p0, p2, p1, materialId);
    AddTri(geo, p0, p3, p2, materialId);
}

void AddOctahedron(GeneratedRTGeometry& geo, Vec3 center, float radius, uint32_t materialId) {
    const Vec3 top{center.x, center.y + radius * 1.35f, center.z};
    const Vec3 bottom{center.x, center.y - radius * 0.25f, center.z};
    const Vec3 p0{center.x - radius, center.y + radius * 0.35f, center.z};
    const Vec3 p1{center.x, center.y + radius * 0.35f, center.z - radius};
    const Vec3 p2{center.x + radius, center.y + radius * 0.35f, center.z};
    const Vec3 p3{center.x, center.y + radius * 0.35f, center.z + radius};
    AddTri(geo, p0, p1, top, materialId);
    AddTri(geo, p1, p2, top, materialId);
    AddTri(geo, p2, p3, top, materialId);
    AddTri(geo, p3, p0, top, materialId);
    AddTri(geo, p1, p0, bottom, materialId);
    AddTri(geo, p2, p1, bottom, materialId);
    AddTri(geo, p3, p2, bottom, materialId);
    AddTri(geo, p0, p3, bottom, materialId);
}

void AddBlade(GeneratedRTGeometry& geo, Vec3 center, Vec3 direction, float radius, uint32_t materialId) {
    Vec3 forward = Normalize(Vec3{direction.x, 0.0f, direction.z});
    if (forward.x == 0.0f && forward.z == 0.0f) {
        forward = Vec3{1.0f, 0.0f, 0.0f};
    }
    const Vec3 right = Normalize(Vec3{-forward.z, 0.0f, forward.x});
    const Vec3 root = Add(center, Scale(forward, -radius * 0.45f));
    const Vec3 tip = Add(center, Scale(forward, radius * 1.75f));
    const Vec3 left = Add(center, Scale(right, -radius * 0.42f));
    const Vec3 rightP = Add(center, Scale(right, radius * 0.42f));
    const Vec3 rootRaised{root.x, root.y + 0.18f, root.z};
    const Vec3 tipRaised{tip.x, tip.y + 0.34f, tip.z};
    const Vec3 leftRaised{left.x, left.y + 0.10f, left.z};
    const Vec3 rightRaised{rightP.x, rightP.y + 0.10f, rightP.z};
    AddTri(geo, rootRaised, leftRaised, tipRaised, materialId);
    AddTri(geo, rootRaised, tipRaised, rightRaised, materialId);
}

void AddFlatRing(GeneratedRTGeometry& geo, Vec3 center, float radius, float thickness, uint32_t materialId) {
    constexpr int kSegments = 24;
    for (int i = 0; i < kSegments; ++i) {
        const float a0 = static_cast<float>(i) * 6.28318530718f / static_cast<float>(kSegments);
        const float a1 = static_cast<float>(i + 1) * 6.28318530718f / static_cast<float>(kSegments);
        const float inner = radius - thickness;
        const float outer = radius + thickness;
        const Vec3 p0{center.x + std::cos(a0) * inner, center.y, center.z + std::sin(a0) * inner};
        const Vec3 p1{center.x + std::cos(a1) * inner, center.y, center.z + std::sin(a1) * inner};
        const Vec3 p2{center.x + std::cos(a1) * outer, center.y, center.z + std::sin(a1) * outer};
        const Vec3 p3{center.x + std::cos(a0) * outer, center.y, center.z + std::sin(a0) * outer};
        AddQuad(geo, p0, p1, p2, p3, materialId);
    }
}

void AddPortalMarker(GeneratedRTGeometry& geo, const Portal& portal) {
    const Vec3 center = MakeVec3(portal.position, 0.08f);
    const float r = portal.radius * 0.82f;
    const uint32_t materialId = portal.open ? kMaterialPortal : kMaterialWall;
    AddQuad(
        geo,
        Vec3{center.x, center.y, center.z - r},
        Vec3{center.x + r, center.y, center.z},
        Vec3{center.x, center.y, center.z + r},
        Vec3{center.x - r, center.y, center.z},
        materialId);
    if (portal.open) {
        AddPyramid(geo, Vec3{center.x, 0.05f, center.z}, r * 0.32f, 1.55f, kMaterialPortal);
    }
}

}

std::vector<EntityRTProxy> BuildEntityProxies(const CombatSim& sim) {
    std::vector<EntityRTProxy> proxies;
    proxies.reserve(32);

    const PlayerState& player = sim.Player();
    const Vec3 facing{player.facing.x, 0.0f, player.facing.y};
    proxies.push_back(EntityRTProxy{
        EntityProxyKind::PlayerCore,
        MakeVec3(player.position, 0.05f),
        facing,
        0.54f,
        kMaterialPlayerCore
    });
    proxies.push_back(EntityRTProxy{
        EntityProxyKind::PlayerBlade,
        MakeVec3(player.position + player.facing * 0.9f, 0.35f),
        facing,
        0.85f,
        kMaterialPlayerBlade
    });

    if (player.controlCooldown > 2.55f) {
        proxies.push_back(EntityRTProxy{
            EntityProxyKind::ControlSpell,
            MakeVec3(player.position, 0.06f),
            facing,
            4.2f,
            kMaterialControl
        });
    }

    for (const EnemyState& enemy : sim.Enemies()) {
        if (!enemy.active || enemy.hp <= 0.0f) {
            continue;
        }
        proxies.push_back(EntityRTProxy{
            enemy.kind == EnemyKind::Brute ? EntityProxyKind::EnemyBrute : EntityProxyKind::EnemyCaster,
            MakeVec3(enemy.position, 0.0f),
            Vec3{1.0f, 0.0f, 0.0f},
            enemy.kind == EnemyKind::Brute ? 0.75f : 0.55f,
            enemy.kind == EnemyKind::Brute ? kMaterialEnemyBrute : kMaterialEnemyCaster
        });
    }

    for (const ProjectileState& projectile : sim.Projectiles()) {
        if (!projectile.active) {
            continue;
        }
        const Vec2 trailDir = Normalize2(Vec2{projectile.velocity.x, projectile.velocity.y}, Vec2{1.0f, 0.0f});
        const Vec3 dir{trailDir.x, 0.0f, trailDir.y};
        proxies.push_back(EntityRTProxy{
            EntityProxyKind::Projectile,
            MakeVec3(projectile.position, 0.25f),
            dir,
            projectile.radius,
            kMaterialProjectile
        });
        proxies.push_back(EntityRTProxy{
            EntityProxyKind::ProjectileTrail,
            MakeVec3(projectile.position - trailDir * 0.55f, 0.15f),
            dir,
            projectile.radius * 1.35f,
            kMaterialProjectile
        });
    }

    return proxies;
}

GeneratedRTGeometry GenerateWorldGeometry(const RoomGraph& world) {
    GeneratedRTGeometry geo{};
    geo.triangles.reserve(static_cast<std::size_t>(world.roomCount) * 28u + static_cast<std::size_t>(world.portalCount) * 10u);

    for (int i = 0; i < world.roomCount; ++i) {
        const Room& room = world.rooms[i];
        AddFloor(geo, room.center, room.halfSize, kMaterialFloor);
        AddRoomBorders(geo, room);
    }

    for (int i = 0; i < world.portalCount; ++i) {
        const Portal& portal = world.portals[i];
        if (portal.a >= 0 && portal.a < world.roomCount && portal.b >= 0 && portal.b < world.roomCount) {
            AddCorridor(geo, world.rooms[portal.a].center, world.rooms[portal.b].center, portal.radius * 0.55f);
        }
        AddPortalMarker(geo, portal);
    }

    return geo;
}

GeneratedRTGeometry GenerateRTGeometry(std::span<const EntityRTProxy> proxies) {
    GeneratedRTGeometry geo{};
    geo.triangles.reserve(proxies.size() * 8);

    for (const EntityRTProxy& proxy : proxies) {
        switch (proxy.kind) {
        case EntityProxyKind::PlayerCore:
            AddOctahedron(geo, proxy.position, proxy.radius, proxy.materialId);
            break;
        case EntityProxyKind::PlayerBlade:
            AddBlade(geo, proxy.position, proxy.direction, proxy.radius, proxy.materialId);
            break;
        case EntityProxyKind::EnemyBrute:
            AddPyramid(geo, proxy.position, proxy.radius, proxy.radius * 1.8f, proxy.materialId);
            break;
        case EntityProxyKind::EnemyCaster:
            AddPyramid(geo, proxy.position, proxy.radius, proxy.radius * 2.4f, proxy.materialId);
            break;
        case EntityProxyKind::Projectile:
            AddPyramid(geo, proxy.position, proxy.radius, proxy.radius, proxy.materialId);
            break;
        case EntityProxyKind::ProjectileTrail:
            AddBlade(geo, proxy.position, proxy.direction, proxy.radius, proxy.materialId);
            break;
        case EntityProxyKind::ControlSpell:
            AddFlatRing(geo, proxy.position, proxy.radius, 0.08f, proxy.materialId);
            break;
        }
    }

    return geo;
}

PackedRTGeometry PackRTGeometry(const GeneratedRTGeometry& geometry) {
    PackedRTGeometry packed{};
    packed.vertices.reserve(geometry.triangles.size() * 3);
    packed.indices.reserve(geometry.triangles.size() * 3);

    for (const RtTriangle& tri : geometry.triangles) {
        const uint32_t base = static_cast<uint32_t>(packed.vertices.size());
        packed.vertices.push_back(PackedRtVertex{tri.a.position, tri.a.normal, tri.materialId});
        packed.vertices.push_back(PackedRtVertex{tri.b.position, tri.b.normal, tri.materialId});
        packed.vertices.push_back(PackedRtVertex{tri.c.position, tri.c.normal, tri.materialId});
        packed.indices.push_back(base + 0u);
        packed.indices.push_back(base + 1u);
        packed.indices.push_back(base + 2u);
        packed.triangleMetadata.push_back(RtTriangleMetadata{tri.a.normal, tri.materialId});
    }

    return packed;
}

uint32_t HashPackedRTGeometry(const PackedRTGeometry& geometry) {
    uint32_t h = 0x7296d123u;
    h = HashMix(h, static_cast<uint32_t>(geometry.vertices.size()));
    h = HashMix(h, static_cast<uint32_t>(geometry.indices.size()));
    h = HashMix(h, static_cast<uint32_t>(geometry.triangleMetadata.size()));

    for (const PackedRtVertex& vertex : geometry.vertices) {
        h = HashMix(h, HashFloat(vertex.position.x));
        h = HashMix(h, HashFloat(vertex.position.y));
        h = HashMix(h, HashFloat(vertex.position.z));
        h = HashMix(h, HashFloat(vertex.normal.x));
        h = HashMix(h, HashFloat(vertex.normal.y));
        h = HashMix(h, HashFloat(vertex.normal.z));
        h = HashMix(h, vertex.materialId);
    }
    for (uint32_t index : geometry.indices) {
        h = HashMix(h, index);
    }
    for (const RtTriangleMetadata& triangle : geometry.triangleMetadata) {
        h = HashMix(h, HashFloat(triangle.normal.x));
        h = HashMix(h, HashFloat(triangle.normal.y));
        h = HashMix(h, HashFloat(triangle.normal.z));
        h = HashMix(h, triangle.materialId);
    }
    return h;
}

}
