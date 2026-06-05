#include "render/entity_rt_proxy.h"

#include <cmath>
#include <cstring>

namespace rogue {

namespace {

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

void AddBlade(GeneratedRTGeometry& geo, Vec3 center, float radius, uint32_t materialId) {
    const Vec3 root{center.x - radius * 0.4f, center.y + 0.2f, center.z};
    const Vec3 tip{center.x + radius * 1.6f, center.y + 0.35f, center.z};
    const Vec3 left{center.x, center.y + 0.1f, center.z - radius * 0.35f};
    const Vec3 right{center.x, center.y + 0.1f, center.z + radius * 0.35f};
    AddTri(geo, root, left, tip, materialId);
    AddTri(geo, root, tip, right, materialId);
}

}

std::vector<EntityRTProxy> BuildEntityProxies(const CombatSim& sim) {
    std::vector<EntityRTProxy> proxies;
    proxies.reserve(32);

    const PlayerState& player = sim.Player();
    proxies.push_back(EntityRTProxy{
        EntityProxyKind::PlayerBlade,
        MakeVec3(player.position + player.facing * 0.9f, 0.35f),
        0.85f,
        1u
    });

    for (const EnemyState& enemy : sim.Enemies()) {
        if (!enemy.active || enemy.hp <= 0.0f) {
            continue;
        }
        proxies.push_back(EntityRTProxy{
            enemy.kind == EnemyKind::Brute ? EntityProxyKind::EnemyBrute : EntityProxyKind::EnemyCaster,
            MakeVec3(enemy.position, 0.0f),
            enemy.kind == EnemyKind::Brute ? 0.75f : 0.55f,
            enemy.kind == EnemyKind::Brute ? 2u : 3u
        });
    }

    for (const ProjectileState& projectile : sim.Projectiles()) {
        if (!projectile.active) {
            continue;
        }
        proxies.push_back(EntityRTProxy{
            EntityProxyKind::Projectile,
            MakeVec3(projectile.position, 0.25f),
            projectile.radius,
            4u
        });
    }

    return proxies;
}

GeneratedRTGeometry GenerateRTGeometry(std::span<const EntityRTProxy> proxies) {
    GeneratedRTGeometry geo{};
    geo.triangles.reserve(proxies.size() * 8);

    for (const EntityRTProxy& proxy : proxies) {
        switch (proxy.kind) {
        case EntityProxyKind::PlayerBlade:
            AddBlade(geo, proxy.position, proxy.radius, proxy.materialId);
            break;
        case EntityProxyKind::EnemyBrute:
            AddPyramid(geo, proxy.position, proxy.radius, proxy.radius * 1.8f, proxy.materialId);
            break;
        case EntityProxyKind::EnemyCaster:
            AddPyramid(geo, proxy.position, proxy.radius, proxy.radius * 2.4f, proxy.materialId);
            break;
        case EntityProxyKind::Projectile:
        case EntityProxyKind::ControlSpell:
            AddPyramid(geo, proxy.position, proxy.radius, proxy.radius, proxy.materialId);
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
    }

    return packed;
}

uint32_t HashPackedRTGeometry(const PackedRTGeometry& geometry) {
    uint32_t h = 0x7296d123u;
    h = HashMix(h, static_cast<uint32_t>(geometry.vertices.size()));
    h = HashMix(h, static_cast<uint32_t>(geometry.indices.size()));

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
    return h;
}

}
