#include "render/entity_rt_proxy.h"

#include <algorithm>
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
constexpr uint32_t kMaterialHitSpark = 10;
constexpr uint32_t kMaterialRoomPulse = 11;
constexpr uint32_t kMaterialEnemySkirmisher = 12;
constexpr uint32_t kMaterialEnemyBulwark = 13;
constexpr uint32_t kMaterialEnemyBoss = 14;

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

float LengthSq(Vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

Vec3 Normalize(Vec3 v) {
    const float lenSq = LengthSq(v);
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

Vec2 Rotate2(Vec2 v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return Vec2{v.x * c - v.y * s, v.x * s + v.y * c};
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

float HashUnit(uint32_t hash) {
    hash ^= hash >> 16u;
    hash *= 0x7feb352du;
    hash ^= hash >> 15u;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16u;
    return static_cast<float>(hash & 0xffffu) / 65535.0f;
}

float Smooth01(float value) {
    const float t = Saturate(value);
    return t * t * (3.0f - 2.0f * t);
}

uint32_t StyleRoleForMaterial(uint32_t materialId) {
    switch (materialId) {
    case kMaterialFloor:
    case kMaterialCorridor:
        return 1u;
    case kMaterialWall:
        return 2u;
    case kMaterialPlayerCore:
    case kMaterialPlayerBlade:
        return 3u;
    case kMaterialEnemyBrute:
    case kMaterialEnemyCaster:
    case kMaterialEnemySkirmisher:
    case kMaterialEnemyBulwark:
    case kMaterialEnemyBoss:
        return 4u;
    case kMaterialHitSpark:
    case kMaterialRoomPulse:
    case kMaterialPortal:
    case kMaterialProjectile:
    case kMaterialControl:
        return 5u;
    default:
        return 0u;
    }
}

uint32_t ElementTokenForMaterial(uint32_t materialId) {
    switch (materialId) {
    case kMaterialHitSpark:
    case kMaterialRoomPulse:
        return 1u;
    case kMaterialEnemyBulwark:
        return 2u;
    case kMaterialProjectile:
    case kMaterialPlayerBlade:
    case kMaterialControl:
        return 3u;
    case kMaterialEnemyCaster:
    case kMaterialEnemyBoss:
        return 4u;
    case kMaterialPortal:
        return 5u;
    default:
        return 6u;
    }
}

uint32_t SurfaceTokenForNormal(Vec3 normal) {
    if (normal.y > 0.72f) {
        return 1u;
    }
    if (normal.y < -0.35f) {
        return 3u;
    }
    return 2u;
}

uint32_t DefaultStyleTag(uint32_t materialId, Vec3 normal) {
    const uint32_t role = StyleRoleForMaterial(materialId);
    const uint32_t element = ElementTokenForMaterial(materialId);
    const uint32_t surface = SurfaceTokenForNormal(normal);
    const uint32_t emissive = (materialId == kMaterialHitSpark ||
        materialId == kMaterialRoomPulse ||
        materialId == kMaterialPortal ||
        materialId == kMaterialProjectile ||
        materialId == kMaterialControl) ? 1u : 0u;
    return (role & 0xfu) |
        ((element & 0xfu) << 4u) |
        ((surface & 0xfu) << 8u) |
        ((emissive & 0xfu) << 12u);
}

void AddTri(GeneratedRTGeometry& geo, Vec3 a, Vec3 b, Vec3 c, uint32_t materialId) {
    const Vec3 n = Normalize(Cross(Sub(b, a), Sub(c, a)));
    geo.triangles.push_back(RtTriangle{
        RtVertex{a, n},
        RtVertex{b, n},
        RtVertex{c, n},
        materialId,
        DefaultStyleTag(materialId, n)
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

void AddCenteredBox(GeneratedRTGeometry& geo, Vec3 center, Vec3 halfExtent, uint32_t materialId) {
    const Vec3 half{
        std::max(0.004f, halfExtent.x),
        std::max(0.004f, halfExtent.y),
        std::max(0.004f, halfExtent.z)
    };
    AddBox(
        geo,
        Vec3{center.x - half.x, center.y - half.y, center.z - half.z},
        Vec3{center.x + half.x, center.y + half.y, center.z + half.z},
        materialId);
}

void AddOrientedPrism(GeneratedRTGeometry& geo, Vec3 center, Vec3 direction, float length, float halfWidth, float halfHeight, uint32_t materialId) {
    Vec3 forward = Normalize(Vec3{direction.x, 0.0f, direction.z});
    if (forward.x == 0.0f && forward.z == 0.0f) {
        forward = Vec3{1.0f, 0.0f, 0.0f};
    }
    const Vec3 right = Normalize(Vec3{-forward.z, 0.0f, forward.x});
    const Vec3 up{0.0f, 1.0f, 0.0f};
    const float l = std::max(0.010f, length * 0.50f);
    const float w = std::max(0.004f, halfWidth);
    const float h = std::max(0.004f, halfHeight);

    const Vec3 f = Scale(forward, l);
    const Vec3 r = Scale(right, w);
    const Vec3 u = Scale(up, h);
    const Vec3 p000 = Add(center, Add(Scale(f, -1.0f), Add(Scale(r, -1.0f), Scale(u, -1.0f))));
    const Vec3 p001 = Add(center, Add(Scale(f, -1.0f), Add(Scale(r, -1.0f), u)));
    const Vec3 p010 = Add(center, Add(Scale(f, -1.0f), Add(r, Scale(u, -1.0f))));
    const Vec3 p011 = Add(center, Add(Scale(f, -1.0f), Add(r, u)));
    const Vec3 p100 = Add(center, Add(f, Add(Scale(r, -1.0f), Scale(u, -1.0f))));
    const Vec3 p101 = Add(center, Add(f, Add(Scale(r, -1.0f), u)));
    const Vec3 p110 = Add(center, Add(f, Add(r, Scale(u, -1.0f))));
    const Vec3 p111 = Add(center, Add(f, Add(r, u)));

    AddQuad(geo, p000, p100, p110, p010, materialId);
    AddQuad(geo, p001, p011, p111, p101, materialId);
    AddQuad(geo, p000, p001, p101, p100, materialId);
    AddQuad(geo, p010, p110, p111, p011, materialId);
    AddQuad(geo, p000, p010, p011, p001, materialId);
    AddQuad(geo, p100, p101, p111, p110, materialId);
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
    constexpr float kThickness = 0.46f;
    constexpr float kHeight = 3.32f;
    constexpr float kNearHeight = 0.24f;
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;
    AddBox(geo, Vec3{x0 - kThickness, 0.0f, z0 - kThickness}, Vec3{x1 + kThickness, kNearHeight, z0}, kMaterialWall);
    AddBox(geo, Vec3{x0 - kThickness, 0.0f, z1}, Vec3{x1 + kThickness, kHeight, z1 + kThickness}, kMaterialWall);
    AddBox(geo, Vec3{x0 - kThickness, 0.0f, z0}, Vec3{x0, kNearHeight, z1}, kMaterialWall);
    AddBox(geo, Vec3{x1, 0.0f, z0}, Vec3{x1 + kThickness, kHeight, z1}, kMaterialWall);
    AddBox(geo, Vec3{x0 - kThickness * 1.18f, kHeight - 0.10f, z1 - 0.08f}, Vec3{x1 + kThickness * 1.18f, kHeight + 0.10f, z1 + kThickness * 1.12f}, kMaterialWall);
    AddBox(geo, Vec3{x1 - 0.08f, kHeight - 0.10f, z0 - kThickness * 1.18f}, Vec3{x1 + kThickness * 1.12f, kHeight + 0.10f, z1 + kThickness * 1.18f}, kMaterialWall);
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
    constexpr int kSegments = 8;
    const float r = std::max(0.003f, radius);
    const float h = std::max(0.004f, height);
    const float shoulderY = center.y + h * 0.42f;
    const Vec3 top{center.x, center.y + h, center.z};
    std::array<Vec3, kSegments> base{};
    std::array<Vec3, kSegments> shoulder{};

    for (int i = 0; i < kSegments; ++i) {
        const float a = (static_cast<float>(i) + 0.5f) * 6.28318530718f / static_cast<float>(kSegments);
        const float squash = (i & 1) ? 0.86f : 1.0f;
        base[i] = Vec3{center.x + std::cos(a) * r * squash, center.y, center.z + std::sin(a) * r * 0.84f};
        shoulder[i] = Vec3{
            center.x + std::cos(a) * r * 0.58f * squash,
            shoulderY,
            center.z + std::sin(a) * r * 0.48f
        };
    }

    for (int i = 0; i < kSegments; ++i) {
        const int j = (i + 1) % kSegments;
        AddQuad(geo, base[i], base[j], shoulder[j], shoulder[i], materialId);
        AddTri(geo, shoulder[i], shoulder[j], top, materialId);
        AddTri(geo, center, base[i], base[j], materialId);
    }
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

void AddCylinder(GeneratedRTGeometry& geo, Vec3 center, float radius, float height, uint32_t materialId, int segments = 10) {
    const int segCount = std::max(6, segments);
    const float r = std::max(0.01f, radius);
    const float y0 = center.y;
    const float y1 = center.y + std::max(0.01f, height);
    const Vec3 bottomCenter{center.x, y0, center.z};
    const Vec3 topCenter{center.x, y1, center.z};

    for (int i = 0; i < segCount; ++i) {
        const float a0 = static_cast<float>(i) * 6.28318530718f / static_cast<float>(segCount);
        const float a1 = static_cast<float>(i + 1) * 6.28318530718f / static_cast<float>(segCount);
        const Vec3 b0{center.x + std::cos(a0) * r, y0, center.z + std::sin(a0) * r};
        const Vec3 b1{center.x + std::cos(a1) * r, y0, center.z + std::sin(a1) * r};
        const Vec3 t0{b0.x, y1, b0.z};
        const Vec3 t1{b1.x, y1, b1.z};
        AddQuad(geo, b0, b1, t1, t0, materialId);
        AddTri(geo, bottomCenter, b1, b0, materialId);
        AddTri(geo, topCenter, t0, t1, materialId);
    }
}

void AddEllipsoid(GeneratedRTGeometry& geo, Vec3 center, Vec3 radii, uint32_t materialId, int segments = 10, int stacks = 5) {
    const int segCount = std::max(6, segments);
    const int stackCount = std::max(3, stacks);
    const Vec3 r{
        std::max(0.01f, radii.x),
        std::max(0.01f, radii.y),
        std::max(0.01f, radii.z)
    };

    auto point = [&](float yaw, float pitch) {
        const float cp = std::cos(pitch);
        return Vec3{
            center.x + std::cos(yaw) * cp * r.x,
            center.y + std::sin(pitch) * r.y,
            center.z + std::sin(yaw) * cp * r.z
        };
    };

    for (int stack = 0; stack < stackCount; ++stack) {
        const float pitch0 = -1.57079632679f + static_cast<float>(stack) * 3.14159265359f / static_cast<float>(stackCount);
        const float pitch1 = -1.57079632679f + static_cast<float>(stack + 1) * 3.14159265359f / static_cast<float>(stackCount);
        for (int i = 0; i < segCount; ++i) {
            const float yaw0 = static_cast<float>(i) * 6.28318530718f / static_cast<float>(segCount);
            const float yaw1 = static_cast<float>(i + 1) * 6.28318530718f / static_cast<float>(segCount);
            const Vec3 p00 = point(yaw0, pitch0);
            const Vec3 p10 = point(yaw1, pitch0);
            const Vec3 p01 = point(yaw0, pitch1);
            const Vec3 p11 = point(yaw1, pitch1);
            if (stack == 0) {
                AddTri(geo, p00, p11, p01, materialId);
            } else if (stack == stackCount - 1) {
                AddTri(geo, p00, p10, p11, materialId);
            } else {
                AddQuad(geo, p00, p10, p11, p01, materialId);
            }
        }
    }
}

void AddBlade(GeneratedRTGeometry& geo, Vec3 center, Vec3 direction, float radius, uint32_t materialId) {
    Vec3 forward = Normalize(Vec3{direction.x, 0.0f, direction.z});
    if (forward.x == 0.0f && forward.z == 0.0f) {
        forward = Vec3{1.0f, 0.0f, 0.0f};
    }
    const Vec3 right = Normalize(Vec3{-forward.z, 0.0f, forward.x});
    const float r = std::max(0.004f, radius);
    const float len = r * 1.72f;
    const Vec3 spine = Add(center, Scale(forward, r * 0.45f));
    AddOrientedPrism(geo, spine, forward, len, r * 0.055f, r * 0.020f, materialId);
    AddOrientedPrism(
        geo,
        Add(spine, Scale(right, r * 0.055f)),
        Normalize(Add(forward, Scale(right, 0.18f))),
        len * 0.58f,
        r * 0.022f,
        r * 0.014f,
        materialId);
    AddOctahedron(geo, Add(center, Scale(forward, r * 1.35f)), r * 0.060f, materialId);
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

void AddGuideStrip(GeneratedRTGeometry& geo, Vec3 center, Vec3 direction, float length, float width, uint32_t materialId) {
    Vec3 forward = Normalize(Vec3{direction.x, 0.0f, direction.z});
    if (forward.x == 0.0f && forward.z == 0.0f) {
        forward = Vec3{1.0f, 0.0f, 0.0f};
    }
    const Vec3 right = Normalize(Vec3{-forward.z, 0.0f, forward.x});
    const Vec3 start = Add(center, Scale(forward, -length * 0.50f));
    const Vec3 end = Add(center, Scale(forward, length * 0.50f));
    const Vec3 a = Add(start, Scale(right, -width));
    const Vec3 b = Add(start, Scale(right, width));
    const Vec3 c = Add(end, Scale(right, width));
    const Vec3 d = Add(end, Scale(right, -width));
    AddQuad(geo, a, b, c, d, materialId);
}

void AddEnemyReadabilityStrip(GeneratedRTGeometry& geo, Vec3 center, Vec3 direction, float length, uint32_t materialId) {
    AddGuideStrip(geo, center, direction, std::max(length, 0.04f), 0.045f, materialId);
}

void AddConeWedge(GeneratedRTGeometry& geo, Vec3 origin, Vec3 direction, float range, float halfAngle, uint32_t materialId) {
    constexpr int kSegments = 18;
    const Vec2 forward = Normalize2(Vec2{direction.x, direction.z}, Vec2{1.0f, 0.0f});
    for (int i = 0; i < kSegments; ++i) {
        const uint32_t h = HashMix(HashFloat(range), static_cast<uint32_t>(0x1510u + i * 73u));
        const float lane = (static_cast<float>(i) + HashUnit(HashMix(h, 0x17u)) * 0.72f) / static_cast<float>(kSegments);
        const float angle = -halfAngle + lane * halfAngle * 2.0f;
        const float distance = range * (0.18f + HashUnit(HashMix(h, 0x29u)) * 0.70f);
        const Vec2 d = Rotate2(forward, angle);
        const Vec3 dir{d.x, 0.0f, d.y};
        const Vec3 p{
            origin.x + d.x * distance,
            origin.y + 0.020f + HashUnit(HashMix(h, 0x43u)) * range * 0.045f,
            origin.z + d.y * distance
        };
        if ((i % 3) == 0) {
            AddBlade(geo, p, dir, range * (0.022f + HashUnit(HashMix(h, 0x5bu)) * 0.026f), materialId);
        } else {
            AddOctahedron(geo, p, range * (0.010f + HashUnit(HashMix(h, 0x6du)) * 0.015f), materialId);
        }
    }
}

void AddArcBand(GeneratedRTGeometry& geo, Vec3 origin, Vec3 direction, float radius, float thickness, float halfAngle, uint32_t materialId) {
    constexpr int kSegments = 16;
    const Vec2 forward = Normalize2(Vec2{direction.x, direction.z}, Vec2{1.0f, 0.0f});
    const float inner = std::max(0.04f, radius - thickness);
    const float outer = radius + thickness;
    for (int i = 0; i < kSegments; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(kSegments);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(kSegments);
        const Vec2 d0 = Rotate2(forward, -halfAngle + t0 * halfAngle * 2.0f);
        const Vec2 d1 = Rotate2(forward, -halfAngle + t1 * halfAngle * 2.0f);
        AddQuad(
            geo,
            Vec3{origin.x + d0.x * inner, origin.y, origin.z + d0.y * inner},
            Vec3{origin.x + d1.x * inner, origin.y, origin.z + d1.y * inner},
            Vec3{origin.x + d1.x * outer, origin.y, origin.z + d1.y * outer},
            Vec3{origin.x + d0.x * outer, origin.y, origin.z + d0.y * outer},
            materialId);
    }
}

void AddSlashCrescent(GeneratedRTGeometry& geo, Vec3 origin, Vec3 direction, float range, float phase, uint32_t materialId) {
    Vec3 dir = Normalize(Vec3{direction.x, 0.0f, direction.z});
    if (dir.x == 0.0f && dir.z == 0.0f) {
        dir = Vec3{1.0f, 0.0f, 0.0f};
    }
    const Vec3 center = Add(origin, Scale(dir, range * (0.34f + phase * 0.10f)));
    const Vec3 lifted{center.x, origin.y + 0.030f + phase * 0.020f, center.z};
    const Vec3 right = Normalize(Vec3{-dir.z, 0.0f, dir.x});
    AddBlade(geo, lifted, Normalize(Add(dir, Scale(right, 0.28f + phase * 0.10f))), range * (0.052f + phase * 0.020f), materialId);
    AddBlade(geo, Add(lifted, Scale(right, -range * 0.050f)), Normalize(Add(dir, Scale(right, -0.22f - phase * 0.06f))), range * (0.040f + phase * 0.016f), materialId);
    for (int i = 0; i < 11; ++i) {
        const uint32_t h = HashMix(HashFloat(range), static_cast<uint32_t>(0x2410u + i * 19u));
        const float side = (HashUnit(HashMix(h, 0x11u)) - 0.5f) * 0.46f;
        const Vec3 mote = Add(lifted, Add(Scale(dir, range * (0.05f + 0.025f * static_cast<float>(i))), Scale(right, range * side)));
        AddOctahedron(geo, mote, range * (0.0045f + phase * 0.0030f), materialId);
    }
}

void AddLayeredActionCrescent(GeneratedRTGeometry& geo, Vec3 origin, Vec3 direction, float range, float phase, uint32_t materialId) {
    Vec3 dir = Normalize(Vec3{direction.x, 0.0f, direction.z});
    if (dir.x == 0.0f && dir.z == 0.0f) {
        dir = Vec3{1.0f, 0.0f, 0.0f};
    }
    const Vec3 right = Normalize(Vec3{-dir.z, 0.0f, dir.x});
    const float impact = Smooth01(phase);
    const Vec3 center = Add(origin, Scale(dir, range * (0.34f + impact * 0.12f)));
    const Vec3 lifted{center.x, origin.y + 0.040f + impact * 0.030f, center.z};

    AddSlashCrescent(geo, origin, dir, range, impact, materialId);
    AddBlade(
        geo,
        Add(lifted, Scale(right, range * 0.11f)),
        Normalize(Add(dir, Scale(right, 0.48f))),
        range * (0.048f + impact * 0.018f),
        materialId);
    AddBlade(
        geo,
        Add(lifted, Scale(right, -range * 0.12f)),
        Normalize(Add(dir, Scale(right, -0.42f))),
        range * (0.038f + impact * 0.015f),
        materialId);
    for (int i = 0; i < 15; ++i) {
        const uint32_t h = HashMix(HashFloat(range), static_cast<uint32_t>(0x3810u + i * 31u));
        const float t = static_cast<float>(i) / 14.0f;
        const float side = (HashUnit(HashMix(h, 0x17u)) - 0.5f) * 0.54f;
        const Vec3 p = Add(origin, Add(Scale(dir, range * (0.18f + t * 0.32f)), Scale(right, range * side)));
        if ((i % 4) == 0) {
            AddBlade(
                geo,
                Vec3{p.x, origin.y + 0.036f + impact * 0.018f, p.z},
                Normalize(Add(dir, Scale(right, side * 0.55f))),
                range * (0.012f + impact * 0.010f),
                materialId);
        } else {
            AddOctahedron(geo, Vec3{p.x, origin.y + 0.070f + impact * 0.020f, p.z}, range * (0.0045f + impact * 0.0030f), materialId);
        }
    }
}

void AddLayeredLineVfx(GeneratedRTGeometry& geo, Vec3 origin, Vec3 direction, float range, float phase, uint32_t materialId) {
    Vec3 dir = Normalize(Vec3{direction.x, 0.0f, direction.z});
    if (dir.x == 0.0f && dir.z == 0.0f) {
        dir = Vec3{1.0f, 0.0f, 0.0f};
    }
    const Vec3 right = Normalize(Vec3{-dir.z, 0.0f, dir.x});
    const float impact = Smooth01(phase);

    for (int i = 0; i < 14; ++i) {
        const float t = static_cast<float>(i) / 13.0f;
        const float side = (HashUnit(HashMix(HashFloat(range), static_cast<uint32_t>(0x610u + i * 37u))) - 0.5f) * range * 0.14f;
        const Vec3 p = Add(origin, Add(Scale(dir, range * (0.15f + t * 0.48f)), Scale(right, side)));
        AddOctahedron(geo, Vec3{p.x, origin.y + 0.050f + impact * 0.030f, p.z}, range * (0.0045f + impact * 0.003f), materialId);
    }
    AddBlade(geo, Add(origin, Scale(dir, range * (0.40f + impact * 0.18f))), dir, range * (0.035f + impact * 0.018f), materialId);
}

void AddLayeredRingVfx(GeneratedRTGeometry& geo, Vec3 origin, Vec3 direction, float radius, float phase, uint32_t materialId) {
    Vec3 dir = Normalize(Vec3{direction.x, 0.0f, direction.z});
    if (dir.x == 0.0f && dir.z == 0.0f) {
        dir = Vec3{1.0f, 0.0f, 0.0f};
    }
    const float impact = Smooth01(phase);
    constexpr int kBursts = 18;
    for (int i = 0; i < kBursts; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kBursts);
        const float angle = t * 6.28318530718f + impact * 0.72f;
        const Vec3 radial{std::cos(angle), 0.0f, std::sin(angle)};
        const Vec3 tangent{-radial.z, 0.0f, radial.x};
        const float lane = radius * (0.34f + 0.44f * HashUnit(HashMix(static_cast<uint32_t>(i * 97u + 0x733u), HashFloat(radius))));
        const Vec3 p = Add(origin, Add(Scale(radial, lane), Vec3{0.0f, 0.022f + impact * 0.018f, 0.0f}));
        AddOctahedron(geo, p, radius * (0.006f + impact * 0.004f), materialId);
        if ((i % 3) == 0) {
            AddOrientedPrism(
                geo,
                Add(p, Scale(tangent, radius * 0.012f)),
                Normalize(Add(tangent, Scale(radial, 0.22f + impact * 0.18f))),
                radius * (0.055f + impact * 0.040f),
                radius * 0.0028f,
                radius * 0.0022f,
                materialId);
        }
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

void AddControlObjectiveMarker(GeneratedRTGeometry& geo, const Room& room) {
    if (room.objective.kind != RoomObjectiveKind::ControlPoint ||
        room.objective.controlRadius <= 0.0f ||
        room.lifecycle == RoomLifecycle::Completed) {
        return;
    }

    const float progress = room.objective.targetSeconds > 0.0001f
        ? Saturate(room.objective.elapsedSeconds / room.objective.targetSeconds)
        : 1.0f;
    const Vec3 center = MakeVec3(room.objective.controlPoint, 0.015f);
    const float radius = room.objective.controlRadius;
    AddFlatRing(geo, center, radius, 0.075f + progress * 0.045f, kMaterialControl);
    AddFlatRing(geo, Vec3{center.x, center.y + 0.012f, center.z}, radius * (0.38f + progress * 0.42f), 0.045f, kMaterialControl);

    const float diamond = radius * 0.26f;
    AddQuad(
        geo,
        Vec3{center.x, center.y + 0.018f, center.z - diamond},
        Vec3{center.x + diamond, center.y + 0.018f, center.z},
        Vec3{center.x, center.y + 0.018f, center.z + diamond},
        Vec3{center.x - diamond, center.y + 0.018f, center.z},
        kMaterialControl);
}

void AddCornerPatch(GeneratedRTGeometry& geo, Vec2 center, Vec2 halfSize, float sx, float sy, float size, uint32_t materialId) {
    const Vec3 patchCenter{center.x + halfSize.x * sx, 0.006f, center.y + halfSize.y * sy};
    const uint32_t seed = HashMix(HashFloat(patchCenter.x + size), HashFloat(patchCenter.z - size));
    const int blades = 9;
    for (int i = 0; i < blades; ++i) {
        const uint32_t h = HashMix(seed, static_cast<uint32_t>(0x331u + i * 47u));
        const float angle = HashUnit(HashMix(h, 0x11u)) * 6.2831853f;
        const Vec2 dir{std::cos(angle), std::sin(angle)};
        const Vec2 right{-dir.y, dir.x};
        const float spread = size * (0.08f + HashUnit(HashMix(h, 0x21u)) * 0.62f);
        const float len = size * (0.12f + HashUnit(HashMix(h, 0x31u)) * 0.28f);
        const float width = size * (0.018f + HashUnit(HashMix(h, 0x41u)) * 0.020f);
        const Vec3 root{
            patchCenter.x + dir.x * spread * 0.32f,
            patchCenter.y + static_cast<float>(i & 1) * 0.002f,
            patchCenter.z + dir.y * spread * 0.32f
        };
        const Vec3 tip{
            root.x + dir.x * len,
            root.y + size * (0.030f + HashUnit(HashMix(h, 0x51u)) * 0.060f),
            root.z + dir.y * len
        };
        AddQuad(
            geo,
            Vec3{root.x - right.x * width, root.y, root.z - right.y * width},
            Vec3{root.x + right.x * width, root.y, root.z + right.y * width},
            Vec3{tip.x + right.x * width * 0.25f, tip.y, tip.z + right.y * width * 0.25f},
            Vec3{tip.x - right.x * width * 0.25f, tip.y, tip.z - right.y * width * 0.25f},
            materialId);
        if ((i % 4) == 0) {
            AddOctahedron(geo, tip, size * 0.018f, materialId);
        }
    }
}

Vec2 RoomDecorPoint(const Room& room, float sx, float sy, float inset);
void AddSquareColumn(GeneratedRTGeometry& geo, Vec2 position, float radius, float height, uint32_t materialId, uint32_t accentMaterial);
void AddCandleCluster(GeneratedRTGeometry& geo, Vec2 position, float scale, uint32_t flameMaterial);
void AddCrystalCluster(GeneratedRTGeometry& geo, Vec2 position, float scale, uint32_t materialId);
void AddVineCascade(GeneratedRTGeometry& geo, Vec2 anchor, float length, float phase, uint32_t materialId);

uint32_t EnvReliefAccent(const RoomVisualStyle& style) {
    if (style.descent > 0.48f) {
        return kMaterialWall;
    }
    if (style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary) {
        return kMaterialWall;
    }
    return style.biome == VisualBiome::ArcaneLibrary ? kMaterialRoomPulse : kMaterialHitSpark;
}

void AddRectFrame(GeneratedRTGeometry& geo, Vec2 center, Vec2 halfSize, float inset, float y, float width, uint32_t materialId) {
    const float hx = std::max(0.20f, halfSize.x - inset);
    const float hy = std::max(0.20f, halfSize.y - inset);
    AddGuideStrip(geo, Vec3{center.x, y, center.y - hy}, Vec3{1.0f, 0.0f, 0.0f}, hx * 2.0f, width, materialId);
    AddGuideStrip(geo, Vec3{center.x, y, center.y + hy}, Vec3{1.0f, 0.0f, 0.0f}, hx * 2.0f, width, materialId);
    AddGuideStrip(geo, Vec3{center.x - hx, y, center.y}, Vec3{0.0f, 0.0f, 1.0f}, hy * 2.0f, width, materialId);
    AddGuideStrip(geo, Vec3{center.x + hx, y, center.y}, Vec3{0.0f, 0.0f, 1.0f}, hy * 2.0f, width, materialId);
}

void AddSlabGrid(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, int roomIndex) {
    const uint32_t grooveMaterial = kMaterialFloor;
    const uint32_t accentMaterial = kMaterialFloor;
    const int verticalLines = 1 + static_cast<int>((style.floorPatternId + static_cast<uint32_t>(roomIndex)) & 1u);
    const int horizontalLines = 1 + static_cast<int>(((style.floorPatternId >> 1u) + static_cast<uint32_t>(roomIndex)) & 1u);
    const float xSpan = room.halfSize.x * 1.68f;
    const float zSpan = room.halfSize.y * 1.68f;
    uint32_t seed = HashMix(VisualStyleHash(style), static_cast<uint32_t>(roomIndex * 193u + 0x771u));

    for (int i = 1; i <= verticalLines; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(verticalLines + 1);
        const float x = room.center.x - room.halfSize.x * 0.78f + room.halfSize.x * 1.56f * t;
        seed = HashMix(seed, static_cast<uint32_t>(i * 29u));
        const float zOffset = (HashUnit(seed) - 0.5f) * room.halfSize.y * 0.46f;
        const float segment = zSpan * (0.38f + HashUnit(HashMix(seed, 0x8bu)) * 0.24f);
        AddGuideStrip(geo, Vec3{x, 0.001f, room.center.y + zOffset}, Vec3{0.0f, 0.0f, 1.0f}, segment * 0.72f, 0.0030f, grooveMaterial);
    }
    for (int i = 1; i <= horizontalLines; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(horizontalLines + 1);
        const float z = room.center.y - room.halfSize.y * 0.78f + room.halfSize.y * 1.56f * t;
        seed = HashMix(seed, static_cast<uint32_t>(i * 43u + 7u));
        const float xOffset = (HashUnit(seed) - 0.5f) * room.halfSize.x * 0.42f;
        const float segment = xSpan * (0.40f + HashUnit(HashMix(seed, 0x31u)) * 0.22f);
        AddGuideStrip(geo, Vec3{room.center.x + xOffset, 0.001f, z}, Vec3{1.0f, 0.0f, 0.0f}, segment * 0.72f, 0.0030f, grooveMaterial);
    }

    AddRectFrame(geo, room.center, room.halfSize, 0.54f, 0.002f, 0.0035f, grooveMaterial);
    AddRectFrame(geo, room.center, room.halfSize, 1.14f, 0.003f, 0.0025f, style.descent > 0.70f ? accentMaterial : grooveMaterial);
}

void AddRaisedStoneMosaic(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    const int columns = 7;
    const int rows = 5;
    const float spanX = room.halfSize.x * 1.52f;
    const float spanZ = room.halfSize.y * 1.44f;
    const float cellX = spanX / static_cast<float>(columns);
    const float cellZ = spanZ / static_cast<float>(rows);
    const float topY = -0.018f + style.wetness * 0.004f;
    const float bottomY = -0.034f;

    for (int z = 0; z < rows; ++z) {
        for (int x = 0; x < columns; ++x) {
            const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x5a0u + z * 37u + x * 53u));
            const float insetX = cellX * (0.075f + HashUnit(HashMix(h, 0x11u)) * 0.030f);
            const float insetZ = cellZ * (0.080f + HashUnit(HashMix(h, 0x29u)) * 0.030f);
            const float jitterX = (HashUnit(HashMix(h, 0x47u)) - 0.5f) * cellX * 0.10f;
            const float jitterZ = (HashUnit(HashMix(h, 0x61u)) - 0.5f) * cellZ * 0.10f;
            const float cx = room.center.x - spanX * 0.5f + cellX * (static_cast<float>(x) + 0.5f) + jitterX;
            const float cz = room.center.y - spanZ * 0.5f + cellZ * (static_cast<float>(z) + 0.5f) + jitterZ;
            AddBox(
                geo,
                Vec3{cx - cellX * 0.50f + insetX, bottomY, cz - cellZ * 0.50f + insetZ},
                Vec3{cx + cellX * 0.50f - insetX, topY + static_cast<float>((x + z) & 1) * 0.003f, cz + cellZ * 0.50f - insetZ},
                kMaterialFloor);
        }
    }
}

void AddBrokenStoneSeams(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    const int seamCount = 4 + static_cast<int>(style.cracks * 4.0f) + (style.biome == VisualBiome::AbyssCrypt ? 1 : 0);
    for (int i = 0; i < seamCount; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x200u + i * 59u));
        const float sx = HashUnit(h) * 1.50f - 0.75f;
        const float sy = HashUnit(HashMix(h, 0x33u)) * 1.44f - 0.72f;
        const float angle = (HashUnit(HashMix(h, 0x91u)) - 0.5f) * 2.50f;
        const Vec2 p = RoomDecorPoint(room, sx, sy, 0.76f);
        const float len = std::min(room.halfSize.x, room.halfSize.y) * (0.26f + HashUnit(HashMix(h, 0xddu)) * 0.28f);
        const float width = 0.005f + style.cracks * 0.006f;
        const uint32_t mat = kMaterialFloor;
        AddGuideStrip(
            geo,
            Vec3{p.x, 0.008f + static_cast<float>(i & 1) * 0.001f, p.y},
            Vec3{std::cos(angle), 0.0f, std::sin(angle)},
            len,
            width,
            mat);
    }
}

void AddCentralFloorOrnament(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    const Vec3 center = MakeVec3(room.center, 0.024f);
    const float roomRadius = std::min(room.halfSize.x, room.halfSize.y);
    const uint32_t darkLine = kMaterialFloor;
    const uint32_t magicLine = EnvReliefAccent(style);

    AddFlatRing(geo, center, roomRadius * 0.42f, 0.014f, darkLine);
    AddFlatRing(geo, Vec3{center.x, center.y + 0.004f, center.z}, roomRadius * 0.24f, 0.008f, darkLine);

    const int spokeCount = style.descent > 0.40f ? 6 : 4;
    for (int i = 0; i < spokeCount; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x340u + i * 73u));
        const float a = (static_cast<float>(i) / static_cast<float>(spokeCount)) * 6.2831853f +
            (HashUnit(h) - 0.5f) * 0.22f;
        const float len = roomRadius * (0.42f + HashUnit(HashMix(h, 0x55u)) * 0.16f);
        AddGuideStrip(
            geo,
            center,
            Vec3{std::cos(a), 0.0f, std::sin(a)},
            len,
            style.descent > 0.40f ? 0.006f : 0.005f,
            (style.descent > 0.62f && (i % 3) == 0) ? magicLine : darkLine);
    }

    if (style.biome == VisualBiome::ArcaneLibrary || style.biome == VisualBiome::AbyssCrypt) {
        AddFlatRing(geo, Vec3{center.x, center.y + 0.007f, center.z}, roomRadius * 0.56f, 0.006f, style.descent > 0.58f ? magicLine : darkLine);
    }
}

void AddStoneChipScatter(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    const int count = 28 + static_cast<int>(style.decay * 18.0f) + static_cast<int>(style.cracks * 10.0f);
    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x4f00u + i * 109u));
        const float rx = HashUnit(HashMix(h, 0x13u)) * 1.64f - 0.82f;
        const float rz = HashUnit(HashMix(h, 0x29u)) * 1.54f - 0.77f;
        const float edgeBias = HashUnit(HashMix(h, 0x3bu));
        const float sx = edgeBias < 0.58f ? (rx < 0.0f ? rx - 0.08f : rx + 0.08f) : rx * 0.72f;
        const float sy = edgeBias < 0.58f ? (rz < 0.0f ? rz - 0.05f : rz + 0.05f) : rz * 0.70f;
        const Vec2 p = RoomDecorPoint(room, sx, sy, 0.56f);
        const float s = 0.035f + HashUnit(HashMix(h, 0x61u)) * (0.070f + style.decay * 0.035f);
        const float aspect = 0.46f + HashUnit(HashMix(h, 0x77u)) * 0.70f;
        const float y = -0.014f + HashUnit(HashMix(h, 0x91u)) * 0.010f;
        const uint32_t mat = kMaterialFloor;
        AddBox(
            geo,
            Vec3{p.x - s, y, p.y - s * aspect},
            Vec3{p.x + s, y + 0.010f + style.decay * 0.004f, p.y + s * aspect},
            mat);
    }
}

void AddFloorMicroInlays(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    const int count = 34 + static_cast<int>(style.cracks * 20.0f) + static_cast<int>(style.decay * 16.0f);
    const float radius = std::min(room.halfSize.x, room.halfSize.y);
    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x5700u + i * 131u));
        const float rx = HashUnit(HashMix(h, 0x17u)) * 1.42f - 0.71f;
        const float rz = HashUnit(HashMix(h, 0x2du)) * 1.34f - 0.67f;
        const Vec2 p = RoomDecorPoint(room, rx, rz, 0.72f);
        const float angle = HashUnit(HashMix(h, 0x41u)) * 6.2831853f;
        const float len = radius * (0.030f + HashUnit(HashMix(h, 0x5fu)) * (0.045f + style.decay * 0.016f));
        const float width = 0.0035f + HashUnit(HashMix(h, 0x73u)) * 0.0045f;
        const uint32_t mat = kMaterialFloor;
        AddGuideStrip(
            geo,
            Vec3{p.x, 0.009f + static_cast<float>(i & 1) * 0.001f, p.y},
            Vec3{std::cos(angle), 0.0f, std::sin(angle)},
            len,
            width,
            mat);
    }
}

void AddRaisedWallScaffold(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style) {
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;
    const bool dramatic = style.biome == VisualBiome::ArcaneLibrary || style.biome == VisualBiome::AbyssCrypt;
    const float wallHeight = 2.82f + style.decay * 0.72f + style.corruption * 0.62f +
        (dramatic ? 0.78f : 0.42f);
    const float ledge = 0.38f;
    AddBox(geo, Vec3{x0 + 0.55f, 0.00f, z0 + 0.10f}, Vec3{x1 - 0.55f, 0.26f, z0 + ledge}, kMaterialWall);
    AddBox(geo, Vec3{x0 + 0.55f, 0.00f, z1 - ledge}, Vec3{x1 - 0.55f, wallHeight, z1 - 0.10f}, kMaterialWall);
    AddBox(geo, Vec3{x0 + 0.10f, 0.00f, z0 + 0.55f}, Vec3{x0 + ledge, 0.24f, z1 - 0.55f}, kMaterialWall);
    AddBox(geo, Vec3{x1 - ledge, 0.00f, z0 + 0.55f}, Vec3{x1 - 0.10f, wallHeight * 0.96f, z1 - 0.55f}, kMaterialWall);

    const float crownY = wallHeight + 0.045f + style.glow * 0.025f;
    AddBox(geo, Vec3{x0 + 0.42f, wallHeight - 0.070f, z1 - ledge - 0.070f}, Vec3{x1 - 0.42f, crownY, z1 - 0.06f}, kMaterialWall);
    AddBox(geo, Vec3{x1 - ledge - 0.070f, wallHeight * 0.90f - 0.070f, z0 + 0.42f}, Vec3{x1 - 0.06f, wallHeight * 0.96f + 0.08f, z1 - 0.42f}, kMaterialWall);

    const uint32_t glowMaterial = style.descent > 0.72f ? kMaterialHitSpark : kMaterialWall;
    AddGuideStrip(geo, Vec3{room.center.x, wallHeight + 0.018f, z1 - ledge - 0.015f}, Vec3{1.0f, 0.0f, 0.0f}, room.halfSize.x * 1.58f, 0.016f, glowMaterial);
    AddGuideStrip(geo, Vec3{x1 - ledge - 0.015f, wallHeight * 0.96f + 0.018f, room.center.y}, Vec3{0.0f, 0.0f, 1.0f}, room.halfSize.y * 1.52f, 0.014f, glowMaterial);
}

void AddWindowLightBeams(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, float phase) {
    if (style.biome != VisualBiome::SunlitRuins && style.biome != VisualBiome::OvergrownSanctuary) {
        return;
    }

    const float span = std::min(room.halfSize.x, room.halfSize.y);
    const Vec2 window = RoomDecorPoint(room, 0.58f + phase * 0.06f, 0.78f, 0.54f);
    const Vec3 dir{-0.82f, 0.0f, -0.48f};
    for (int i = 0; i < 12; ++i) {
        const float offset = (static_cast<float>(i) - 5.5f) * span * 0.095f;
        const float length = span * (0.44f + style.glow * 0.16f - static_cast<float>(i & 1) * 0.030f);
        const Vec3 center{
            window.x + offset * 0.42f - length * 0.18f,
            0.045f + static_cast<float>(i & 3) * 0.006f,
            window.y - offset * 0.32f - length * 0.18f
        };
        AddOctahedron(geo, Add(center, Scale(dir, length * 0.18f)), 0.020f + style.glow * 0.014f, kMaterialHitSpark);
        AddOctahedron(geo, Add(center, Scale(dir, length * 0.03f)), 0.011f + style.glow * 0.008f, kMaterialRoomPulse);
    }

    const Vec3 moteCenter = MakeVec3(RoomDecorPoint(room, 0.12f + phase * 0.08f, 0.05f, 0.50f), 0.038f);
    for (int i = 0; i < 6; ++i) {
        const float a = static_cast<float>(i) * 1.04719755f + phase * 0.30f;
        const Vec3 radial{std::cos(a), 0.0f, std::sin(a)};
        const Vec3 p = Add(moteCenter, Add(Scale(radial, span * (0.04f + static_cast<float>(i & 1) * 0.018f)), Vec3{0.0f, static_cast<float>(i & 2) * 0.006f, 0.0f}));
        AddOctahedron(geo, p, 0.014f + style.glow * 0.006f, kMaterialHitSpark);
    }
}

void AddWindowMullionWall(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t accentMaterial) {
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;
    const float farZ = z1 - 0.22f;
    const float sideX = x1 - 0.26f;
    const float wallTop = 4.02f + style.glow * 0.46f + style.decay * 0.28f;
    const float span = (x1 - x0) * 0.50f;
    const float centerX = room.center.x + room.halfSize.x * 0.22f;

    AddBox(geo, Vec3{centerX - span * 0.52f, 0.18f, farZ - 0.045f}, Vec3{centerX + span * 0.52f, wallTop, farZ + 0.045f}, kMaterialWall);
    AddBox(geo, Vec3{sideX - 0.045f, 0.18f, z0 + room.halfSize.y * 0.10f}, Vec3{sideX + 0.045f, wallTop * 0.92f, z1 - room.halfSize.y * 0.18f}, kMaterialWall);

    for (int i = 0; i < 4; ++i) {
        const float t = (static_cast<float>(i) - 1.5f) / 3.0f;
        const float x = centerX + t * span * 0.78f;
        AddBox(geo, Vec3{x - 0.020f, 0.22f, farZ - 0.070f}, Vec3{x + 0.020f, wallTop + 0.10f, farZ + 0.070f}, accentMaterial);
    }

    for (int i = 0; i < 4; ++i) {
        const float y = 0.62f + static_cast<float>(i) * 0.58f;
        AddBox(geo, Vec3{centerX - span * 0.50f, y, farZ - 0.072f}, Vec3{centerX + span * 0.50f, y + 0.025f, farZ + 0.072f}, accentMaterial);
    }
}

void AddWindowGlowPanes(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t paneMaterial) {
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z1 = room.center.y + room.halfSize.y;
    const float farZ = z1 - 0.285f;
    const float span = (x1 - x0) * 0.34f;
    const float centerX = room.center.x + room.halfSize.x * 0.22f;
    const float y0 = 0.78f;
    const float y1 = 3.88f + style.glow * 0.40f;

    if (style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary) {
        for (int col = 0; col < 3; ++col) {
            const float xT = (static_cast<float>(col) - 1.0f) * 0.30f;
            const float paneX = centerX + xT * span;
            const float paneW = span * 0.072f;
            AddBox(
                geo,
                Vec3{paneX - paneW, y0 + 0.10f, farZ - 0.090f},
                Vec3{paneX + paneW, y1 - 0.18f, farZ - 0.084f},
                paneMaterial);
        }
        AddBox(
            geo,
            Vec3{centerX - span * 0.46f, y1 - 0.36f, farZ - 0.091f},
            Vec3{centerX + span * 0.46f, y1 - 0.16f, farZ - 0.084f},
            paneMaterial);
        for (int i = 0; i < 5; ++i) {
            const float t = (static_cast<float>(i) - 2.0f) * 0.23f;
            const float cx = centerX + t * span;
            const float w = span * 0.022f;
            AddBox(
                geo,
                Vec3{cx - w, y0, farZ - 0.083f},
                Vec3{cx + w, y1, farZ - 0.078f},
                paneMaterial);
        }
        for (int i = 0; i < 4; ++i) {
            const float y = y0 + (y1 - y0) * (0.18f + static_cast<float>(i) * 0.20f);
            AddBox(geo, Vec3{centerX - span * 0.54f, y, farZ - 0.084f}, Vec3{centerX + span * 0.54f, y + 0.018f, farZ - 0.078f}, paneMaterial);
        }
        AddOctahedron(geo, Vec3{centerX - span * 0.58f, y0 + 0.10f, farZ - 0.082f}, span * 0.032f, paneMaterial);
        AddOctahedron(geo, Vec3{centerX + span * 0.58f, y0 + 0.20f, farZ - 0.082f}, span * 0.026f, paneMaterial);
    } else {
        AddOctahedron(geo, Vec3{centerX - span * 0.12f, 0.92f, farZ - 0.080f}, span * 0.075f, paneMaterial);
        AddOctahedron(geo, Vec3{centerX + span * 0.22f, 1.16f, farZ - 0.080f}, span * 0.060f, paneMaterial);
    }
}

void AddSegmentedBackArch(
    GeneratedRTGeometry& geo,
    Vec3 center,
    float halfSpan,
    float baseY,
    float archHeight,
    float depth,
    uint32_t stoneMaterial,
    uint32_t accentMaterial,
    uint32_t seed);

void AddReferenceWindowBays(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t paneMaterial) {
    if (style.biome != VisualBiome::SunlitRuins && style.biome != VisualBiome::OvergrownSanctuary) {
        return;
    }

    const float x1 = room.center.x + room.halfSize.x;
    const float z1 = room.center.y + room.halfSize.y;
    const float y0 = 0.92f;
    const float y1 = 3.52f + style.glow * 0.46f;
    const float backZ = z1 - 0.026f;
    const float bayHalfW = room.halfSize.x * 0.145f;
    const float rib = 0.030f;
    const float depth = 0.036f;

    for (int bay = 0; bay < 2; ++bay) {
        const float lane = bay == 0 ? -0.18f : 0.30f;
        const float cx = room.center.x + room.halfSize.x * lane;
        AddBox(geo, Vec3{cx - bayHalfW - rib, y0 - rib, backZ - depth}, Vec3{cx + bayHalfW + rib, y0 + rib, backZ + depth}, kMaterialWall);
        AddBox(geo, Vec3{cx - bayHalfW - rib, y1 - rib, backZ - depth}, Vec3{cx + bayHalfW + rib, y1 + rib, backZ + depth}, kMaterialWall);
        AddBox(geo, Vec3{cx - bayHalfW - rib, y0, backZ - depth}, Vec3{cx - bayHalfW + rib, y1, backZ + depth}, kMaterialWall);
        AddBox(geo, Vec3{cx + bayHalfW - rib, y0, backZ - depth}, Vec3{cx + bayHalfW + rib, y1, backZ + depth}, kMaterialWall);
        AddBox(
            geo,
            Vec3{cx - bayHalfW * 0.86f, y0 + 0.08f, backZ - depth * 1.34f},
            Vec3{cx + bayHalfW * 0.86f, y1 - 0.14f, backZ - depth * 1.18f},
            paneMaterial);

        for (int i = 0; i < 3; ++i) {
            const float t = (static_cast<float>(i) + 1.0f) / 4.0f;
            const float x = cx - bayHalfW * 0.82f + bayHalfW * 1.64f * t;
            AddBox(geo, Vec3{x - rib * 0.34f, y0 + 0.06f, backZ - depth * 1.42f}, Vec3{x + rib * 0.34f, y1 - 0.12f, backZ - depth * 0.96f}, kMaterialWall);
        }
        for (int i = 0; i < 4; ++i) {
            const float t = (static_cast<float>(i) + 1.0f) / 5.0f;
            const float y = y0 + (y1 - y0) * t;
            AddBox(geo, Vec3{cx - bayHalfW * 0.78f, y - rib * 0.26f, backZ - depth * 1.44f}, Vec3{cx + bayHalfW * 0.78f, y + rib * 0.26f, backZ - depth * 0.96f}, kMaterialWall);
        }
        AddSegmentedBackArch(
            geo,
            Vec3{cx, 0.0f, backZ - depth * 0.52f},
            bayHalfW + rib * 0.40f,
            y1 - 0.18f,
            0.36f + style.glow * 0.08f,
            depth * 0.82f,
            kMaterialWall,
            paneMaterial,
            HashMix(VisualStyleHash(style), static_cast<uint32_t>(0x8260u + bay * 97u)));
    }

    const float sideX = x1 - 0.030f;
    const float sideCenterZ = room.center.y + room.halfSize.y * 0.28f;
    const float sideHalfZ = room.halfSize.y * 0.18f;
    AddBox(geo, Vec3{sideX - depth, y0 - rib, sideCenterZ - sideHalfZ - rib}, Vec3{sideX + depth, y0 + rib, sideCenterZ + sideHalfZ + rib}, kMaterialWall);
    AddBox(geo, Vec3{sideX - depth, y1 - rib, sideCenterZ - sideHalfZ - rib}, Vec3{sideX + depth, y1 + rib, sideCenterZ + sideHalfZ + rib}, kMaterialWall);
    AddBox(geo, Vec3{sideX - depth, y0, sideCenterZ - sideHalfZ - rib}, Vec3{sideX + depth, y1, sideCenterZ - sideHalfZ + rib}, kMaterialWall);
    AddBox(geo, Vec3{sideX - depth, y0, sideCenterZ + sideHalfZ - rib}, Vec3{sideX + depth, y1, sideCenterZ + sideHalfZ + rib}, kMaterialWall);
    AddBox(
        geo,
        Vec3{sideX - depth * 1.42f, y0 + 0.08f, sideCenterZ - sideHalfZ * 0.78f},
        Vec3{sideX - depth * 1.18f, y1 - 0.16f, sideCenterZ + sideHalfZ * 0.78f},
        paneMaterial);
    for (int i = 0; i < 3; ++i) {
        const float z = sideCenterZ - sideHalfZ * 0.66f + sideHalfZ * 1.32f * (static_cast<float>(i) + 1.0f) / 4.0f;
        AddBox(geo, Vec3{sideX - depth * 1.48f, y0 + 0.06f, z - rib * 0.30f}, Vec3{sideX - depth * 0.96f, y1 - 0.14f, z + rib * 0.30f}, kMaterialWall);
    }

    const Vec3 beamDir{-0.78f, 0.0f, -0.46f};
    for (int i = 0; i < 32; ++i) {
        const uint32_t h = HashMix(VisualStyleHash(style), static_cast<uint32_t>(0x8b40u + i * 37u));
        const float lane = (HashUnit(HashMix(h, 0x11u)) - 0.5f) * room.halfSize.x * 0.62f;
        const float along = HashUnit(HashMix(h, 0x23u)) * room.halfSize.x * 1.04f;
        const Vec3 p{
            room.center.x + room.halfSize.x * 0.18f + lane * 0.24f + beamDir.x * along * 0.54f,
            0.038f + HashUnit(HashMix(h, 0x37u)) * (0.18f + style.glow * 0.12f),
            room.center.y + room.halfSize.y * 0.38f + beamDir.z * along * 0.50f
        };
        AddOctahedron(geo, p, 0.010f + HashUnit(HashMix(h, 0x51u)) * 0.014f, paneMaterial);
    }
}

void AddLowBenchRun(GeneratedRTGeometry& geo, const Room& room, float sx, float sy, bool horizontal, uint32_t materialId) {
    const Vec2 p = RoomDecorPoint(room, sx, sy, 0.54f);
    const float length = horizontal ? room.halfSize.x * 0.78f : room.halfSize.y * 0.78f;
    const float halfLength = std::max(0.36f, length * 0.50f);
    const float halfDepth = 0.075f;
    if (horizontal) {
        AddBox(geo, Vec3{p.x - halfLength, 0.04f, p.y - halfDepth}, Vec3{p.x + halfLength, 0.15f, p.y + halfDepth}, materialId);
    } else {
        AddBox(geo, Vec3{p.x - halfDepth, 0.04f, p.y - halfLength}, Vec3{p.x + halfDepth, 0.15f, p.y + halfLength}, materialId);
    }
}

void AddLeafBlade(GeneratedRTGeometry& geo, Vec2 position, Vec2 direction, float length, float width, float height, uint32_t materialId) {
    const Vec2 dir = Normalize2(direction, Vec2{1.0f, 0.0f});
    const Vec2 right{-dir.y, dir.x};
    const float len = std::max(0.025f, length);
    const float w = std::max(0.004f, width);
    const float y0 = 0.018f;
    const float y1 = std::max(y0 + 0.010f, height);
    const Vec3 root{position.x, y0, position.y};
    const Vec3 tip{position.x + dir.x * len, y1, position.y + dir.y * len};
    AddQuad(
        geo,
        Vec3{root.x - right.x * w, root.y, root.z - right.y * w},
        Vec3{root.x + right.x * w, root.y, root.z + right.y * w},
        Vec3{tip.x + right.x * w * 0.28f, tip.y, tip.z + right.y * w * 0.28f},
        Vec3{tip.x - right.x * w * 0.28f, tip.y, tip.z - right.y * w * 0.28f},
        materialId);
}

void AddLeafClump(
    GeneratedRTGeometry& geo,
    Vec2 position,
    float scale,
    uint32_t seed,
    uint32_t leafMaterial,
    uint32_t flowerMaterial) {
    const float s = std::max(0.035f, scale);
    const int blades = 5 + static_cast<int>(HashMix(seed, 0x3a1u) & 5u);
    for (int i = 0; i < blades; ++i) {
        const uint32_t h = HashMix(seed, static_cast<uint32_t>(0x510u + i * 53u));
        const float a = HashUnit(HashMix(h, 0x11u)) * 6.2831853f;
        const float spread = HashUnit(HashMix(h, 0x29u)) * s * 0.34f;
        const Vec2 dir{std::cos(a), std::sin(a)};
        const Vec2 p{position.x + dir.x * spread * 0.28f, position.y + dir.y * spread * 0.28f};
        const float len = s * (0.46f + HashUnit(HashMix(h, 0x43u)) * 0.82f);
        const float width = s * (0.035f + HashUnit(HashMix(h, 0x5fu)) * 0.026f);
        const float height = 0.040f + s * (0.24f + HashUnit(HashMix(h, 0x71u)) * 0.48f);
        AddLeafBlade(geo, p, dir, len, width, height, leafMaterial);
    }
    if (flowerMaterial != leafMaterial && (seed & 3u) == 0u) {
        const float d = s * (0.30f + HashUnit(HashMix(seed, 0x91u)) * 0.36f);
        const float a = HashUnit(HashMix(seed, 0xa7u)) * 6.2831853f;
        AddEllipsoid(
            geo,
            Vec3{position.x + std::cos(a) * d, 0.052f + s * 0.12f, position.y + std::sin(a) * d},
            Vec3{s * 0.090f, s * 0.060f, s * 0.080f},
            flowerMaterial,
            7,
            3);
    }
}

void AddPetalScatter(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    if (style.moss < 0.25f) {
        return;
    }
    const float edgeInset = 0.58f;
    const int count = style.biome == VisualBiome::OvergrownSanctuary ? 34 : 22;
    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(i * 41 + 0x1907u));
        const float rx = HashUnit(h) * 2.0f - 1.0f;
        const float rz = HashUnit(HashMix(h, 0x5eu)) * 2.0f - 1.0f;
        const float side = HashUnit(HashMix(h, 0x91u)) > 0.5f ? 1.0f : -1.0f;
        const float sx = side * (0.62f + std::abs(rx) * 0.28f);
        const float sy = rz * 0.82f;
        const Vec2 p = RoomDecorPoint(room, sx, sy, edgeInset);
        const float s = 0.034f + HashUnit(HashMix(h, 0xd3u)) * 0.030f;
        const uint32_t mat = kMaterialEnemySkirmisher;
        AddLeafBlade(geo, p, Rotate2(Vec2{1.0f, 0.0f}, HashUnit(HashMix(h, 0xe5u)) * 6.2831853f), s * 1.45f, s * 0.26f, 0.035f + s * 0.26f, mat);
    }
}

void AddRubbleScatter(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, float stagePad) {
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;
    const int count = 22 + static_cast<int>(style.decay * 18.0f) + static_cast<int>(style.descent * 14.0f);

    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x3910u + i * 71u));
        const int side = static_cast<int>(HashMix(h, 0x13u) & 3u);
        const float t = HashUnit(HashMix(h, 0x2fu)) * 2.0f - 1.0f;
        const float d = 0.28f + HashUnit(HashMix(h, 0x47u)) * stagePad * 0.72f;
        float x = room.center.x;
        float z = room.center.y;
        if (side == 0) {
            x = room.center.x + t * (room.halfSize.x + stagePad * 0.48f);
            z = z0 - d;
        } else if (side == 1) {
            x = x1 + d;
            z = room.center.y + t * (room.halfSize.y + stagePad * 0.48f);
        } else if (side == 2) {
            x = room.center.x + t * (room.halfSize.x + stagePad * 0.48f);
            z = z1 + d;
        } else {
            x = x0 - d;
            z = room.center.y + t * (room.halfSize.y + stagePad * 0.48f);
        }

        const float sx = 0.07f + HashUnit(HashMix(h, 0x63u)) * (0.18f + style.decay * 0.08f);
        const float sz = 0.05f + HashUnit(HashMix(h, 0x89u)) * (0.16f + style.decay * 0.06f);
        const float height = 0.025f + HashUnit(HashMix(h, 0xa7u)) * (0.12f + style.decay * 0.08f);
        const uint32_t mat = (style.corruption > 0.58f && (i % 7) == 0) ? kMaterialHitSpark : kMaterialWall;
        AddBox(geo, Vec3{x - sx, -0.035f, z - sz}, Vec3{x + sx, -0.020f + height, z + sz}, mat);
    }
}

void AddEdgeFoliageBeds(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, float stagePad) {
    if (style.moss < 0.18f) {
        return;
    }

    const int count = style.biome == VisualBiome::OvergrownSanctuary ? 112 : 76;
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;

    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x4c20u + i * 97u));
        const int side = static_cast<int>(HashMix(h, 0x21u) & 3u);
        const float t = HashUnit(HashMix(h, 0x43u)) * 2.0f - 1.0f;
        const float d = 0.14f + HashUnit(HashMix(h, 0x59u)) * (0.72f + stagePad * 0.18f);
        float x = room.center.x;
        float z = room.center.y;
        if (side == 0) {
            x = room.center.x + t * (room.halfSize.x + 0.56f);
            z = z0 - d;
        } else if (side == 1) {
            x = x1 + d;
            z = room.center.y + t * (room.halfSize.y + 0.56f);
        } else if (side == 2) {
            x = room.center.x + t * (room.halfSize.x + 0.56f);
            z = z1 + d;
        } else {
            x = x0 - d;
            z = room.center.y + t * (room.halfSize.y + 0.56f);
        }

        const float sx = 0.040f + HashUnit(HashMix(h, 0x71u)) * 0.120f;
        const float sz = 0.030f + HashUnit(HashMix(h, 0x8fu)) * 0.100f;
        const float y = 0.006f + static_cast<float>(i & 3) * 0.0015f;
        const uint32_t mat = kMaterialEnemySkirmisher;
        AddQuad(
            geo,
            Vec3{x - sx, y, z - sz * 0.35f},
            Vec3{x - sx * 0.20f, y, z - sz},
            Vec3{x + sx, y, z + sz * 0.35f},
            Vec3{x + sx * 0.20f, y, z + sz},
            mat);
        if ((i & 1) == 0) {
            AddLeafClump(geo, Vec2{x, z}, std::max(sx, sz) * (0.64f + style.moss * 0.32f), h, mat, kMaterialWall);
        }
    }
}

void AddMossCarpetRuns(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    if (style.biome != VisualBiome::SunlitRuins && style.biome != VisualBiome::OvergrownSanctuary) {
        return;
    }

    const int count = style.biome == VisualBiome::OvergrownSanctuary ? 52 : 36;
    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x6e00u + i * 137u));
        const int side = static_cast<int>(h & 3u);
        const float along = HashUnit(HashMix(h, 0x21u)) * 1.78f - 0.89f;
        const float inward = HashUnit(HashMix(h, 0x43u)) * 0.18f;
        float sx = along;
        float sy = -0.82f + inward;
        if (side == 1) {
            sy = 0.82f - inward;
        } else if (side == 2) {
            sx = -0.82f + inward;
            sy = along;
        } else if (side == 3) {
            sx = 0.82f - inward;
            sy = along;
        }

        const Vec2 p = RoomDecorPoint(room, sx, sy, 0.56f);
        const float sxSize = 0.056f + HashUnit(HashMix(h, 0x65u)) * (0.110f + style.moss * 0.050f);
        const float sySize = 0.038f + HashUnit(HashMix(h, 0x77u)) * (0.090f + style.moss * 0.040f);
        const float skew = (HashUnit(HashMix(h, 0x91u)) - 0.5f) * 0.32f;
        const float y = 0.019f + static_cast<float>(i & 3) * 0.001f;
        AddQuad(
            geo,
            Vec3{p.x - sxSize, y, p.y - sySize * (0.52f + skew)},
            Vec3{p.x - sxSize * 0.10f, y, p.y - sySize},
            Vec3{p.x + sxSize, y, p.y + sySize * (0.48f - skew)},
            Vec3{p.x + sxSize * 0.12f, y, p.y + sySize},
            kMaterialEnemySkirmisher);

        if ((i % 8) == 0) {
            AddQuad(
                geo,
                Vec3{p.x - sxSize * 0.30f, y + 0.002f, p.y - sySize * 0.25f},
                Vec3{p.x, y + 0.002f, p.y - sySize * 0.42f},
                Vec3{p.x + sxSize * 0.34f, y + 0.002f, p.y + sySize * 0.18f},
            Vec3{p.x - sxSize * 0.04f, y + 0.002f, p.y + sySize * 0.34f},
                kMaterialWall);
        }
        if ((i & 1) == 0) {
            AddLeafClump(geo, p, std::max(sxSize, sySize) * 0.66f, h, kMaterialEnemySkirmisher, kMaterialWall);
        }
    }
}

void AddWindowShadowLattice(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    (void)geo;
    (void)room;
    (void)style;
    (void)seedBase;
}

void AddBackdropRelief(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, float stagePad) {
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;
    const float backWallZ = z1 + stagePad * 0.72f - 0.34f;
    const float sideWallX = x1 + stagePad * 0.74f - 0.33f;
    const uint32_t relief = EnvReliefAccent(style);
    const bool dark = style.biome == VisualBiome::ArcaneLibrary || style.biome == VisualBiome::AbyssCrypt;
    const int panels = dark ? 5 : 4;
    const float panelTop = 1.72f + style.decay * 0.44f + style.descent * 0.56f;

    for (int i = 0; i < panels; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xe120u + i * 61u));
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(panels);
        const float cx = x0 - stagePad * 0.46f + (x1 - x0 + stagePad * 0.92f) * t;
        const float halfW = (x1 - x0) * (dark ? 0.055f : 0.070f) * (0.84f + HashUnit(HashMix(h, 0x11u)) * 0.36f);
        const float bottom = 0.28f + HashUnit(HashMix(h, 0x23u)) * 0.08f;
        AddBox(geo, Vec3{cx - halfW, bottom, backWallZ - 0.060f}, Vec3{cx + halfW, panelTop, backWallZ + 0.020f}, kMaterialWall);
        AddBox(geo, Vec3{cx - halfW * 0.78f, bottom + 0.12f, backWallZ - 0.074f}, Vec3{cx + halfW * 0.78f, bottom + 0.16f, backWallZ + 0.026f}, relief);
        if (dark && (i & 1) == 0) {
            AddCandleCluster(geo, Vec2{cx + (HashUnit(h) - 0.5f) * halfW, backWallZ - 0.22f}, 0.16f + style.glow * 0.05f, style.biome == VisualBiome::AbyssCrypt ? kMaterialHitSpark : kMaterialRoomPulse);
        }
    }

    const int sidePanels = dark ? 4 : 3;
    for (int i = 0; i < sidePanels; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(sidePanels);
        const float cz = z0 - stagePad * 0.18f + (z1 - z0 + stagePad * 0.66f) * t;
        const float h = 1.12f + style.decay * 0.30f + static_cast<float>(i & 1) * 0.22f;
        AddBox(geo, Vec3{sideWallX - 0.045f, 0.24f, cz - 0.22f}, Vec3{sideWallX + 0.050f, 0.24f + h, cz + 0.22f}, kMaterialWall);
        AddBox(geo, Vec3{sideWallX - 0.060f, 0.24f + h * 0.46f, cz - 0.17f}, Vec3{sideWallX + 0.055f, 0.24f + h * 0.52f, cz + 0.17f}, relief);
    }

    if (dark) {
        const float daisX = x1 - stagePad * 0.05f;
        const float daisZ = z1 + stagePad * 0.34f;
        AddBox(geo, Vec3{daisX - 0.62f, 0.00f, daisZ - 0.34f}, Vec3{daisX + 0.52f, 0.16f, daisZ + 0.38f}, kMaterialWall);
        AddBox(geo, Vec3{daisX - 0.44f, 0.16f, daisZ - 0.24f}, Vec3{daisX + 0.42f, 0.30f, daisZ + 0.28f}, kMaterialWall);
        AddBox(geo, Vec3{daisX - 0.32f, 0.30f, daisZ + 0.14f}, Vec3{daisX + 0.32f, 1.32f + style.descent * 0.24f, daisZ + 0.24f}, kMaterialWall);
        AddBox(geo, Vec3{daisX - 0.48f, 0.38f, daisZ + 0.10f}, Vec3{daisX - 0.38f, 1.12f + style.decay * 0.22f, daisZ + 0.28f}, kMaterialWall);
        AddBox(geo, Vec3{daisX + 0.36f, 0.38f, daisZ + 0.10f}, Vec3{daisX + 0.46f, 1.18f + style.decay * 0.24f, daisZ + 0.28f}, kMaterialWall);
        AddGuideStrip(geo, Vec3{daisX, 0.78f, daisZ + 0.075f}, Vec3{1.0f, 0.0f, 0.0f}, 0.74f, 0.018f, relief);
        AddGuideStrip(geo, Vec3{daisX, 1.08f, daisZ + 0.075f}, Vec3{1.0f, 0.0f, 0.0f}, 0.54f, 0.014f, relief);
        AddPyramid(geo, Vec3{daisX + 0.10f, 0.30f, daisZ + 0.02f}, 0.24f + style.corruption * 0.06f, 0.54f + style.descent * 0.20f, relief);
        AddPyramid(geo, Vec3{daisX, 1.35f + style.descent * 0.24f, daisZ + 0.18f}, 0.18f + style.corruption * 0.05f, 0.44f + style.descent * 0.18f, kMaterialWall);
    } else {
        const float sillZ = z1 + stagePad * 0.40f;
        AddBox(geo, Vec3{x0 + 0.40f, 0.18f, sillZ - 0.050f}, Vec3{x1 - 0.20f, 0.32f, sillZ + 0.040f}, kMaterialWall);
        AddVineCascade(geo, Vec2{x1 - 0.42f, sillZ - 0.08f}, room.halfSize.x * 0.58f, 0.10f, kMaterialEnemySkirmisher);
    }
}

void AddForegroundDepth(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, float stagePad) {
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float frontZ = z0 - stagePad * 0.76f;
    const float sideSpan = (x1 - x0) * 0.24f;
    const float h = 0.38f + style.decay * 0.16f + style.descent * 0.22f;
    const bool lush = style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary;
    if (lush) {
        const int chunks = style.biome == VisualBiome::OvergrownSanctuary ? 6 : 5;
        for (int i = 0; i < chunks; ++i) {
            const uint32_t hash = HashMix(seedBase, static_cast<uint32_t>(0xee10u + i * 59u));
            const bool leftSide = (i & 1) == 0;
            const float t = (static_cast<float>(i) + HashUnit(HashMix(hash, 0x21u)) * 0.38f) / static_cast<float>(chunks);
            const float x = leftSide
                ? x0 - stagePad * (0.30f - t * 0.08f) + sideSpan * (0.18f + t * 0.82f)
                : x1 + stagePad * (0.08f + t * 0.18f) - sideSpan * (0.16f + t * 0.70f);
            const float z = frontZ + (HashUnit(HashMix(hash, 0x33u)) - 0.5f) * 0.34f;
            const float sx = 0.18f + HashUnit(HashMix(hash, 0x47u)) * 0.22f;
            const float sz = 0.08f + HashUnit(HashMix(hash, 0x59u)) * 0.10f;
            const float height = h * (0.18f + HashUnit(HashMix(hash, 0x71u)) * 0.34f);
            AddBox(geo, Vec3{x - sx, 0.00f, z - sz}, Vec3{x + sx, height, z + sz}, kMaterialWall);
            if ((i & 1) == 0 || style.biome == VisualBiome::OvergrownSanctuary) {
                AddLeafClump(
                    geo,
                    Vec2{x + (HashUnit(HashMix(hash, 0x89u)) - 0.5f) * sx, z + (HashUnit(HashMix(hash, 0x9bu)) - 0.5f) * sz},
                    0.12f + style.moss * 0.10f + HashUnit(HashMix(hash, 0xa7u)) * 0.06f,
                    hash,
                    kMaterialEnemySkirmisher,
                    kMaterialWall);
            }
            if ((i % 3) == 1) {
                AddOctahedron(geo, Vec3{x, height + 0.045f, z}, 0.030f + style.glow * 0.012f, kMaterialProjectile);
            }
        }

        AddGuideStrip(
            geo,
            Vec3{room.center.x - room.halfSize.x * 0.16f, 0.012f, frontZ + 0.04f},
            Vec3{1.0f, 0.0f, 0.08f},
            room.halfSize.x * 1.42f,
            0.020f,
            kMaterialWall);
    } else {
        AddBox(geo, Vec3{x0 - stagePad * 0.38f, 0.00f, frontZ - 0.16f}, Vec3{x0 + sideSpan, h, frontZ + 0.18f}, kMaterialWall);
        AddBox(geo, Vec3{x1 - sideSpan, 0.00f, frontZ - 0.18f}, Vec3{x1 + stagePad * 0.34f, h * 0.82f, frontZ + 0.16f}, kMaterialWall);
    }

    const uint32_t relief = EnvReliefAccent(style);
    for (int i = 0; i < 4; ++i) {
        const uint32_t hash = HashMix(seedBase, static_cast<uint32_t>(0xf200u + i * 37u));
        const float t = HashUnit(hash);
        const float x = i < 2
            ? x0 - stagePad * 0.18f + sideSpan * t
            : x1 + stagePad * 0.10f - sideSpan * t;
        const float z = frontZ + (HashUnit(HashMix(hash, 0x31u)) - 0.5f) * 0.28f;
        AddBox(geo, Vec3{x - 0.055f, 0.00f, z - 0.055f}, Vec3{x + 0.055f, h * (0.55f + t * 0.45f), z + 0.055f}, kMaterialWall);
        if (style.biome == VisualBiome::AbyssCrypt && (i & 1) == 0) {
            AddOctahedron(geo, Vec3{x, h * 0.70f, z}, 0.045f + style.glow * 0.020f, relief);
        }
    }
}

void AddPerimeterArchitecture(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, float stagePad) {
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;
    const uint32_t accent = style.biome == VisualBiome::AbyssCrypt ? kMaterialHitSpark :
        (style.biome == VisualBiome::ArcaneLibrary ? kMaterialRoomPulse : kMaterialWall);

    const float backWallZ = z1 + stagePad * 0.72f;
    const float sideWallX = x1 + stagePad * 0.74f;
    const float wallHeight = 2.86f + style.decay * 0.62f + style.corruption * 0.58f + style.descent * 0.64f;
    AddBox(geo, Vec3{x0 - stagePad * 0.72f, 0.02f, backWallZ - 0.16f}, Vec3{x1 + stagePad * 0.78f, wallHeight, backWallZ + 0.16f}, kMaterialWall);
    AddBox(geo, Vec3{sideWallX - 0.16f, 0.02f, z0 - stagePad * 0.42f}, Vec3{sideWallX + 0.16f, wallHeight * 0.94f, z1 + stagePad * 0.70f}, kMaterialWall);
    AddBackdropRelief(geo, room, style, seedBase, stagePad);
    AddForegroundDepth(geo, room, style, seedBase, stagePad);

    const int pillarCount = 4 + static_cast<int>(style.descent * 2.0f);
    for (int i = 0; i < pillarCount; ++i) {
        const float t = pillarCount > 1 ? static_cast<float>(i) / static_cast<float>(pillarCount - 1) : 0.0f;
        const float x = x0 - stagePad * 0.42f + (room.halfSize.x * 2.0f + stagePad * 0.92f) * t;
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x6120u + i * 43u));
        const float height = wallHeight * (0.66f + HashUnit(HashMix(h, 0x3du)) * 0.40f);
        AddSquareColumn(geo, Vec2{x, backWallZ - 0.24f}, 0.13f + style.decay * 0.035f, height, kMaterialWall, accent);
        if (style.biome == VisualBiome::ArcaneLibrary || style.biome == VisualBiome::AbyssCrypt) {
            AddCandleCluster(geo, Vec2{x + (HashUnit(h) - 0.5f) * 0.22f, backWallZ - 0.48f}, 0.22f + style.glow * 0.10f, accent);
        }
    }

    if (style.biome == VisualBiome::ArcaneLibrary || style.biome == VisualBiome::AbyssCrypt) {
        for (int i = 0; i < 5; ++i) {
            const float t = static_cast<float>(i) / 4.0f;
            const float z = z0 - stagePad * 0.18f + (room.halfSize.y * 2.0f + stagePad * 0.62f) * t;
            AddPyramid(geo, Vec3{sideWallX - 0.08f, wallHeight * 0.72f, z}, 0.12f + style.corruption * 0.04f, 0.42f + style.descent * 0.22f, accent);
        }
    } else {
        AddVineCascade(geo, Vec2{sideWallX - 0.18f, room.center.y + room.halfSize.y * 0.36f}, room.halfSize.y * 0.80f, 0.12f, kMaterialEnemySkirmisher);
        AddVineCascade(geo, Vec2{room.center.x + room.halfSize.x * 0.46f, backWallZ - 0.14f}, room.halfSize.x * 0.62f, -0.16f, kMaterialEnemySkirmisher);
    }
}

void AddOuterArenaSetDressing(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style) {
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;
    const float apron = 1.25f + style.decay * 0.30f;
    const float y0 = -0.085f;
    const float y1 = -0.045f;
    const float stagePad = apron * (5.18f + style.fog * 1.28f + style.descent * 0.46f);
    const uint32_t seedBase = HashMix(VisualStyleHash(style), 0x8ab1u);

    AddBox(
        geo,
        Vec3{x0 - stagePad, -0.145f, z0 - stagePad},
        Vec3{x1 + stagePad, -0.058f, z1 + stagePad},
        kMaterialCorridor);
    AddRectFrame(
        geo,
        room.center,
        Vec2{room.halfSize.x + stagePad * 0.58f, room.halfSize.y + stagePad * 0.58f},
        0.20f,
        -0.035f,
        0.030f,
        kMaterialWall);
    AddRectFrame(
        geo,
        room.center,
        Vec2{room.halfSize.x + stagePad * 0.36f, room.halfSize.y + stagePad * 0.36f},
        0.16f,
        -0.028f,
        0.016f,
        kMaterialWall);

    AddBox(geo, Vec3{x0 - apron, y0, z0 - apron}, Vec3{x1 + apron, y1, z0 - 0.08f}, kMaterialWall);
    AddBox(geo, Vec3{x0 - apron, y0, z1 + 0.08f}, Vec3{x1 + apron, y1, z1 + apron}, kMaterialWall);
    AddBox(geo, Vec3{x0 - apron, y0, z0 - 0.08f}, Vec3{x0 - 0.08f, y1, z1 + 0.08f}, kMaterialWall);
    AddBox(geo, Vec3{x1 + 0.08f, y0, z0 - 0.08f}, Vec3{x1 + apron, y1, z1 + 0.08f}, kMaterialWall);

    AddPerimeterArchitecture(geo, room, style, seedBase, stagePad);
    AddWindowShadowLattice(geo, room, style, seedBase);
    AddRubbleScatter(geo, room, style, seedBase, stagePad);

    if (style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary) {
        const float channelWidth = 0.28f + style.wetness * 0.18f;
        AddBox(geo, Vec3{x0 - apron * 0.72f, -0.040f, z0 - apron * 0.42f}, Vec3{x1 + apron * 0.48f, -0.018f, z0 - apron * 0.42f + channelWidth}, kMaterialProjectile);
        AddBox(geo, Vec3{x1 + apron * 0.28f, -0.040f, z0 - apron * 0.10f}, Vec3{x1 + apron * 0.28f + channelWidth, -0.018f, z1 + apron * 0.62f}, kMaterialProjectile);
        AddVineCascade(geo, Vec2{x0 - apron * 0.45f, room.center.y + room.halfSize.y * 0.28f}, std::min(room.halfSize.x, room.halfSize.y) * 0.55f, 0.10f, kMaterialEnemySkirmisher);
        AddVineCascade(geo, Vec2{x1 + apron * 0.42f, room.center.y - room.halfSize.y * 0.34f}, std::min(room.halfSize.x, room.halfSize.y) * 0.48f, -0.08f, kMaterialEnemySkirmisher);
        AddMossCarpetRuns(geo, room, style, seedBase);
        AddEdgeFoliageBeds(geo, room, style, seedBase, stagePad);
    } else {
        AddCandleCluster(geo, Vec2{x0 - apron * 0.52f, z1 + apron * 0.46f}, 0.46f + style.glow * 0.20f, kMaterialHitSpark);
        AddCandleCluster(geo, Vec2{x1 + apron * 0.42f, z0 - apron * 0.34f}, 0.40f + style.glow * 0.18f, kMaterialRoomPulse);
    }
}

void AddReferenceArenaScaffold(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, int roomIndex) {
    const uint32_t seedBase = HashMix(VisualStyleHash(style), static_cast<uint32_t>(roomIndex * 131u + 0x8721u));
    const float jitterA = HashUnit(HashMix(seedBase, 0x41u)) - 0.5f;
    AddRaisedStoneMosaic(geo, room, style, seedBase);
    AddStoneChipScatter(geo, room, style, seedBase);
    AddFloorMicroInlays(geo, room, style, seedBase);
    AddSlabGrid(geo, room, style, roomIndex);
    AddBrokenStoneSeams(geo, room, style, seedBase);
    AddCentralFloorOrnament(geo, room, style, seedBase);
    AddRaisedWallScaffold(geo, room, style);
    AddOuterArenaSetDressing(geo, room, style);

    const float columnHeight = 1.28f + style.decay * 0.30f + style.glow * 0.18f;
    const uint32_t accent = style.biome == VisualBiome::AbyssCrypt ? kMaterialHitSpark :
        (style.biome == VisualBiome::ArcaneLibrary ? kMaterialRoomPulse : kMaterialControl);
    const uint32_t windowAccent = style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary
        ? kMaterialWall
        : accent;
    AddSquareColumn(geo, RoomDecorPoint(room, -0.86f, -0.78f, 0.52f), 0.22f, columnHeight, kMaterialWall, accent);
    AddSquareColumn(geo, RoomDecorPoint(room, 0.86f, 0.78f, 0.52f), 0.22f, columnHeight * (0.86f + style.corruption * 0.18f), kMaterialWall, accent);
    AddSquareColumn(geo, RoomDecorPoint(room, -0.86f, 0.74f, 0.58f), 0.18f, columnHeight * 0.72f, kMaterialWall, accent);
    AddSquareColumn(geo, RoomDecorPoint(room, 0.82f, -0.74f, 0.58f), 0.18f, columnHeight * 0.78f, kMaterialWall, accent);
    AddWindowMullionWall(geo, room, style, windowAccent);
    const uint32_t paneMaterial = style.biome == VisualBiome::AbyssCrypt ? kMaterialPortal :
        (style.biome == VisualBiome::ArcaneLibrary ? kMaterialRoomPulse : kMaterialHitSpark);
    AddWindowGlowPanes(geo, room, style, paneMaterial);
    AddReferenceWindowBays(geo, room, style, paneMaterial);
    AddLowBenchRun(geo, room, -0.62f, -0.86f, true, kMaterialWall);
    AddLowBenchRun(geo, room, 0.86f, 0.08f, false, kMaterialWall);

    if (style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary) {
        AddWindowLightBeams(geo, room, style, jitterA);
        AddVineCascade(geo, RoomDecorPoint(room, -0.84f, 0.36f, 0.54f), std::min(room.halfSize.x, room.halfSize.y) * (0.62f + style.moss * 0.24f), jitterA * 0.18f, kMaterialEnemySkirmisher);
        AddCrystalCluster(geo, RoomDecorPoint(room, -0.82f, -0.72f, 0.54f), 0.26f + style.glow * 0.10f, kMaterialProjectile);
        AddCrystalCluster(geo, RoomDecorPoint(room, 0.82f, 0.72f, 0.54f), 0.24f + style.glow * 0.10f, kMaterialProjectile);
        AddCrystalCluster(geo, RoomDecorPoint(room, 0.78f, 0.66f, 0.64f), 0.30f + style.glow * 0.14f, kMaterialProjectile);
        AddPetalScatter(geo, room, style, seedBase);
    } else if (style.biome == VisualBiome::ArcaneLibrary) {
        AddCandleCluster(geo, RoomDecorPoint(room, 0.74f, -0.66f, 0.56f), 0.42f + style.glow * 0.16f, kMaterialHitSpark);
        AddCrystalCluster(geo, RoomDecorPoint(room, -0.74f, 0.62f, 0.62f), 0.36f + style.glow * 0.14f, kMaterialRoomPulse);
    } else {
        AddCandleCluster(geo, RoomDecorPoint(room, 0.74f, -0.66f, 0.56f), 0.40f + style.glow * 0.16f, kMaterialHitSpark);
        AddCrystalCluster(geo, RoomDecorPoint(room, -0.74f, 0.62f, 0.62f), 0.40f + style.glow * 0.18f, kMaterialPortal);
    }
}

void AddStyleFloorDetails(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, int roomIndex) {
    const Vec3 center = MakeVec3(room.center, 0.010f);
    const float roomRadius = std::min(room.halfSize.x, room.halfSize.y);
    AddReferenceArenaScaffold(geo, room, style, roomIndex);

    if (style.floorPatternId == 2u || style.biome == VisualBiome::ArcaneLibrary) {
        AddFlatRing(geo, center, roomRadius * 0.54f, 0.012f, kMaterialFloor);
        AddFlatRing(geo, Vec3{center.x, center.y + 0.006f, center.z}, roomRadius * 0.31f, 0.007f, kMaterialFloor);
    }

    if (style.wetness > 0.48f) {
        const Vec3 wetCenter{
            center.x + room.halfSize.x * 0.18f,
            center.y + 0.004f,
            center.z - room.halfSize.y * 0.16f
        };
        for (int i = 0; i < 10; ++i) {
            const uint32_t h = HashMix(VisualStyleHash(style), static_cast<uint32_t>(0x7380u + roomIndex * 31u + i * 17u));
            const float a = HashUnit(HashMix(h, 0x11u)) * 6.2831853f;
            const float r = roomRadius * HashUnit(HashMix(h, 0x23u)) * (0.13f + style.wetness * 0.10f);
            const float sx = 0.035f + HashUnit(HashMix(h, 0x35u)) * 0.075f;
            const float sz = 0.018f + HashUnit(HashMix(h, 0x47u)) * 0.052f;
            AddBox(
                geo,
                Vec3{wetCenter.x + std::cos(a) * r - sx, wetCenter.y - 0.010f, wetCenter.z + std::sin(a) * r - sz},
                Vec3{wetCenter.x + std::cos(a) * r + sx, wetCenter.y - 0.003f, wetCenter.z + std::sin(a) * r + sz},
                kMaterialProjectile);
        }
    }

    if (style.cracks > 0.42f) {
        const uint32_t crackMaterial = kMaterialFloor;
        const float phase = static_cast<float>((roomIndex * 37) & 63) * 0.035f;
        AddGuideStrip(
            geo,
            Vec3{center.x - room.halfSize.x * 0.08f, center.y + 0.006f, center.z + room.halfSize.y * 0.03f},
            Vec3{0.72f + phase, 0.0f, 0.44f},
            room.halfSize.x * (0.92f + style.cracks * 0.25f),
            0.007f,
            crackMaterial);
        AddGuideStrip(
            geo,
            Vec3{center.x + room.halfSize.x * 0.17f, center.y + 0.007f, center.z - room.halfSize.y * 0.18f},
            Vec3{-0.36f, 0.0f, 0.88f + phase},
            room.halfSize.y * (0.70f + style.decay * 0.28f),
            0.006f,
            crackMaterial);
    }

    if (style.moss > 0.32f) {
        const float patch = 0.30f + style.moss * 0.42f;
        AddCornerPatch(geo, room.center, room.halfSize, -0.76f, -0.70f, patch, kMaterialEnemySkirmisher);
        AddCornerPatch(geo, room.center, room.halfSize, 0.78f, 0.68f, patch * 0.78f, kMaterialEnemySkirmisher);
        if (style.moss > 0.62f) {
            AddCornerPatch(geo, room.center, room.halfSize, -0.70f, 0.72f, patch * 0.64f, kMaterialControl);
        }
    }
}

Vec2 RoomDecorPoint(const Room& room, float sx, float sy, float inset = 0.0f) {
    const float x = room.center.x + room.halfSize.x * sx - inset * (sx < 0.0f ? -1.0f : (sx > 0.0f ? 1.0f : 0.0f));
    const float y = room.center.y + room.halfSize.y * sy - inset * (sy < 0.0f ? -1.0f : (sy > 0.0f ? 1.0f : 0.0f));
    return Vec2{x, y};
}

void AddSquareColumn(GeneratedRTGeometry& geo, Vec2 position, float radius, float height, uint32_t materialId, uint32_t) {
    const float r = std::max(0.08f, radius);
    const float h = std::max(0.36f, height);
    AddBox(
        geo,
        Vec3{position.x - r * 1.16f, 0.0f, position.y - r * 1.16f},
        Vec3{position.x + r * 1.16f, 0.16f, position.y + r * 1.16f},
        materialId);
    AddCylinder(geo, Vec3{position.x, 0.16f, position.y}, r * 0.54f, h - 0.16f, materialId, 12);
    AddCylinder(geo, Vec3{position.x, h * 0.38f, position.y}, r * 0.61f, r * 0.10f, materialId, 12);
    AddCylinder(geo, Vec3{position.x, h * 0.70f, position.y}, r * 0.58f, r * 0.08f, materialId, 12);
    AddBox(
        geo,
        Vec3{position.x - r * 1.05f, h, position.y - r * 1.05f},
        Vec3{position.x + r * 1.05f, h + 0.16f, position.y + r * 1.05f},
        materialId);
    AddFlatRing(geo, Vec3{position.x, 0.018f, position.y}, r * 1.18f, 0.018f, materialId);
}

void AddCandleCluster(GeneratedRTGeometry& geo, Vec2 position, float scale, uint32_t flameMaterial) {
    const float s = std::max(0.08f, scale);
    for (int i = 0; i < 3; ++i) {
        const float dx = (static_cast<float>(i) - 1.0f) * s * 0.30f;
        const float dz = (i == 1 ? -0.20f : 0.10f) * s;
        const float height = s * (0.55f + static_cast<float>(i) * 0.13f);
        AddCylinder(geo, Vec3{position.x + dx, 0.0f, position.y + dz}, s * 0.055f, height, kMaterialWall, 8);
        AddEllipsoid(
            geo,
            Vec3{position.x + dx, height + s * 0.12f, position.y + dz},
            Vec3{s * 0.065f, s * 0.150f, s * 0.065f},
            flameMaterial,
            8,
            4);
    }
}

void AddCrystalCluster(GeneratedRTGeometry& geo, Vec2 position, float scale, uint32_t materialId) {
    const float s = std::max(0.12f, scale);
    AddEllipsoid(geo, Vec3{position.x, s * 0.34f, position.y}, Vec3{s * 0.24f, s * 0.34f, s * 0.22f}, materialId, 9, 4);
    AddEllipsoid(geo, Vec3{position.x - s * 0.35f, s * 0.23f, position.y + s * 0.16f}, Vec3{s * 0.14f, s * 0.24f, s * 0.12f}, materialId, 8, 3);
    AddEllipsoid(geo, Vec3{position.x + s * 0.32f, s * 0.20f, position.y - s * 0.22f}, Vec3{s * 0.12f, s * 0.20f, s * 0.11f}, materialId, 8, 3);
    AddEllipsoid(geo, Vec3{position.x, 0.024f, position.y}, Vec3{s * 0.48f, s * 0.018f, s * 0.40f}, materialId, 10, 2);
}

void AddVineCascade(GeneratedRTGeometry& geo, Vec2 anchor, float length, float phase, uint32_t materialId) {
    const Vec3 base{anchor.x, 0.055f, anchor.y};
    for (int i = 0; i < 6; ++i) {
        const float t = static_cast<float>(i) / 5.0f;
        AddEllipsoid(
            geo,
            Vec3{base.x + length * (0.05f + t * (0.18f + phase * 0.08f)), base.y + t * 0.015f, base.z + length * (t * 0.72f - 0.08f * std::sin(t * 3.14159265f))},
            Vec3{length * (0.032f - t * 0.010f), length * (0.024f - t * 0.006f), length * (0.038f - t * 0.010f)},
            materialId,
            7,
            3);
    }
    AddCornerPatch(geo, anchor, Vec2{length * 0.20f, length * 0.20f}, 0.0f, 0.0f, length * 0.070f, materialId);
}

void AddBookshelfRun(GeneratedRTGeometry& geo, Vec2 center, float length, bool horizontal, uint32_t accentMaterial) {
    const float halfLength = std::max(0.5f, length * 0.50f);
    const float depth = 0.18f;
    const float height = 1.18f;
    if (horizontal) {
        AddBox(
            geo,
            Vec3{center.x - halfLength, 0.0f, center.y - depth},
            Vec3{center.x + halfLength, height, center.y + depth},
            kMaterialWall);
        AddGuideStrip(geo, Vec3{center.x, height * 0.34f, center.y + depth + 0.012f}, Vec3{1.0f, 0.0f, 0.0f}, halfLength * 1.82f, 0.028f, accentMaterial);
        AddGuideStrip(geo, Vec3{center.x, height * 0.66f, center.y + depth + 0.014f}, Vec3{1.0f, 0.0f, 0.0f}, halfLength * 1.72f, 0.022f, accentMaterial);
    } else {
        AddBox(
            geo,
            Vec3{center.x - depth, 0.0f, center.y - halfLength},
            Vec3{center.x + depth, height, center.y + halfLength},
            kMaterialWall);
        AddGuideStrip(geo, Vec3{center.x + depth + 0.012f, height * 0.34f, center.y}, Vec3{0.0f, 0.0f, 1.0f}, halfLength * 1.82f, 0.028f, accentMaterial);
        AddGuideStrip(geo, Vec3{center.x + depth + 0.014f, height * 0.66f, center.y}, Vec3{0.0f, 0.0f, 1.0f}, halfLength * 1.72f, 0.022f, accentMaterial);
    }
}

void AddAbyssSpire(GeneratedRTGeometry& geo, Vec2 position, float scale, uint32_t materialId, uint32_t glowMaterial) {
    const float s = std::max(0.18f, scale);
    AddPyramid(geo, Vec3{position.x, 0.0f, position.y}, s * 0.46f, s * 1.80f, materialId);
    AddPyramid(geo, Vec3{position.x + s * 0.42f, 0.0f, position.y + s * 0.24f}, s * 0.25f, s * 1.10f, materialId);
    AddFlatRing(geo, Vec3{position.x, 0.020f, position.y}, s * 0.72f, 0.044f, glowMaterial);
    AddOctahedron(geo, Vec3{position.x, s * 1.32f, position.y}, s * 0.18f, glowMaterial);
}

void AddRubbleScatter(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, int count) {
    const uint32_t glowMat = style.biome == VisualBiome::AbyssCrypt ? kMaterialHitSpark :
        (style.biome == VisualBiome::ArcaneLibrary ? kMaterialRoomPulse : kMaterialControl);
    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x720u + i * 97u));
        const int side = static_cast<int>(h & 3u);
        const float along = HashUnit(HashMix(h, 0x11u)) * 1.78f - 0.89f;
        const float drift = HashUnit(HashMix(h, 0x22u)) * 0.20f - 0.10f;
        float sx = along;
        float sy = -0.88f + drift;
        if (side == 1) {
            sy = 0.88f + drift;
        } else if (side == 2) {
            sx = -0.88f + drift;
            sy = along;
        } else if (side == 3) {
            sx = 0.88f + drift;
            sy = along;
        }

        const Vec2 p = RoomDecorPoint(room, sx, sy, 0.28f + style.decay * 0.20f);
        const float s = 0.045f + HashUnit(HashMix(h, 0x45u)) * (0.080f + style.decay * 0.035f);
        const float hgt = s * (0.28f + HashUnit(HashMix(h, 0x73u)) * 0.60f);
        const uint32_t mat = ((i % 11) == 0 && style.corruption > 0.62f) ? glowMat : kMaterialWall;
        AddBox(
            geo,
            Vec3{p.x - s, -0.006f, p.y - s * 0.72f},
            Vec3{p.x + s, hgt, p.y + s * 0.72f},
            mat);
        if ((i & 3) == 0) {
            AddPyramid(geo, Vec3{p.x + s * 0.38f, hgt * 0.42f, p.y - s * 0.24f}, s * 0.55f, s * (0.70f + style.decay * 0.32f), mat);
        }
    }
}

void AddBotanicalEdgeDetail(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    const int clumpCount = style.biome == VisualBiome::OvergrownSanctuary ? 10 : 7;
    for (int i = 0; i < clumpCount; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xa40u + i * 131u));
        const float side = (h & 1u) == 0u ? -1.0f : 1.0f;
        const float sx = side * (0.70f + HashUnit(HashMix(h, 0x31u)) * 0.18f);
        const float sy = HashUnit(HashMix(h, 0x52u)) * 1.52f - 0.76f;
        const Vec2 p = RoomDecorPoint(room, sx, sy, 0.48f);
        const float s = 0.070f + HashUnit(HashMix(h, 0x81u)) * 0.055f + style.moss * 0.030f;
        AddCornerPatch(geo, p, Vec2{s * 0.58f, s * 0.50f}, 0.0f, 0.0f, s * 0.64f, kMaterialEnemySkirmisher);
        AddLeafBlade(geo, p, Vec2{0.48f + side * 0.18f, 0.88f}, s * (0.82f + style.moss * 0.34f), s * 0.080f, 0.050f + s * 0.32f, kMaterialEnemySkirmisher);
        if ((i % 5) == 0 && style.biome == VisualBiome::OvergrownSanctuary) {
            AddEllipsoid(geo, Vec3{p.x + side * s * 0.38f, 0.080f, p.y - s * 0.20f}, Vec3{s * 0.10f, s * 0.070f, s * 0.090f}, kMaterialWall, 7, 3);
        }
    }
}

void AddGrassTuft(GeneratedRTGeometry& geo, Vec2 position, float scale, uint32_t seed, uint32_t leafMaterial, uint32_t flowerMaterial) {
    const int blades = 5 + static_cast<int>(HashMix(seed, 0x19u) & 3u);
    AddCornerPatch(geo, position, Vec2{scale * 0.50f, scale * 0.42f}, 0.0f, 0.0f, scale * 0.26f, leafMaterial);
    for (int i = 0; i < blades; ++i) {
        const uint32_t h = HashMix(seed, static_cast<uint32_t>(0x270u + i * 47u));
        const float a = HashUnit(HashMix(h, 0x11u)) * 6.2831853f;
        const float len = scale * (0.36f + HashUnit(HashMix(h, 0x2bu)) * 0.56f);
        const float bend = (HashUnit(HashMix(h, 0x51u)) - 0.5f) * 0.52f;
        const Vec3 dir{std::cos(a + bend), 0.0f, std::sin(a + bend)};
        const Vec3 center{position.x + dir.x * len * 0.20f, 0.030f + HashUnit(HashMix(h, 0x63u)) * 0.014f, position.y + dir.z * len * 0.20f};
        AddEllipsoid(geo, center, Vec3{scale * 0.055f, scale * 0.024f, scale * 0.050f}, leafMaterial, 6, 3);
        AddLeafBlade(
            geo,
            Vec2{position.x + dir.x * len * 0.10f, position.y + dir.z * len * 0.10f},
            Vec2{dir.x, dir.z},
            len * 0.82f,
            scale * (0.020f + HashUnit(HashMix(h, 0x8du)) * 0.018f),
            0.034f + scale * (0.20f + HashUnit(HashMix(h, 0x9bu)) * 0.36f),
            leafMaterial);
        if ((i == 0 || i == blades - 1) && flowerMaterial != leafMaterial) {
            AddEllipsoid(geo, Vec3{position.x + dir.x * len * 0.44f, 0.050f, position.y + dir.z * len * 0.44f}, Vec3{scale * 0.10f, scale * 0.065f, scale * 0.090f}, flowerMaterial, 7, 3);
        }
    }
}

void AddFlowerbedScatter(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, int count) {
    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xb70u + i * 113u));
        const int side = static_cast<int>(h & 3u);
        const float along = HashUnit(HashMix(h, 0x23u)) * 1.64f - 0.82f;
        const float inward = HashUnit(HashMix(h, 0x41u)) * 0.18f;
        float sx = along;
        float sy = -0.76f + inward;
        if (side == 1) {
            sy = 0.76f - inward;
        } else if (side == 2) {
            sx = -0.76f + inward;
            sy = along;
        } else if (side == 3) {
            sx = 0.76f - inward;
            sy = along;
        }

        const Vec2 p = RoomDecorPoint(room, sx, sy, 0.54f);
        const float s = 0.060f + HashUnit(HashMix(h, 0x67u)) * 0.050f + style.moss * 0.030f;
        const uint32_t flowerMat = ((i % 7) == 0 && style.biome == VisualBiome::OvergrownSanctuary)
            ? kMaterialWall
            : kMaterialEnemySkirmisher;
        AddGrassTuft(geo, p, s, h, kMaterialEnemySkirmisher, flowerMat);
    }
}

void AddCandleRow(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, int count, uint32_t flameMaterial) {
    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xc90u + i * 83u));
        const bool backWall = (h & 1u) == 0u;
        const float t = HashUnit(HashMix(h, 0x35u)) * 1.42f - 0.71f;
        const float offset = HashUnit(HashMix(h, 0x5du)) * 0.16f;
        const float sx = backWall ? t : (HashUnit(HashMix(h, 0x7bu)) > 0.5f ? -0.80f + offset : 0.80f - offset);
        const float sy = backWall ? 0.80f - offset : t;
        const Vec2 p = RoomDecorPoint(room, sx, sy, 0.46f);
        const float s = 0.045f + HashUnit(HashMix(h, 0x91u)) * 0.030f;
        AddCylinder(geo, Vec3{p.x, 0.0f, p.y}, s * 0.24f, s * 1.45f, kMaterialWall, 8);
        AddEllipsoid(geo, Vec3{p.x, s * (1.68f + style.glow * 0.16f), p.y}, Vec3{s * 0.24f, s * 0.36f, s * 0.22f}, flameMaterial, 8, 4);
        if ((i % 3) == 0) {
            AddFlatRing(geo, Vec3{p.x, 0.014f, p.y}, s * (1.15f + style.glow * 0.35f), 0.010f, flameMaterial);
        }
    }
}

void AddBrokenArchDetail(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t accentMaterial) {
    const float z = room.center.y + room.halfSize.y - 0.24f;
    const float baseY = 0.42f;
    const float h = 0.92f + style.decay * 0.34f + style.corruption * 0.18f;
    const float span = room.halfSize.x * 0.42f;
    const float cx = room.center.x + (style.floorPatternId & 1u ? room.halfSize.x * 0.18f : -room.halfSize.x * 0.18f);
    const float w = 0.055f + style.decay * 0.016f;
    AddBox(geo, Vec3{cx - span - w, 0.02f, z - 0.050f}, Vec3{cx - span + w, baseY + h, z + 0.050f}, kMaterialWall);
    AddBox(geo, Vec3{cx + span - w, 0.02f, z - 0.050f}, Vec3{cx + span + w, baseY + h * 0.82f, z + 0.050f}, kMaterialWall);
    AddGuideStrip(geo, Vec3{cx, baseY + h, z - 0.020f}, Vec3{1.0f, 0.0f, 0.0f}, span * 1.76f, 0.040f, kMaterialWall);
    AddPyramid(geo, Vec3{cx - span * 0.12f, baseY + h + 0.030f, z}, span * 0.10f, 0.28f + style.decay * 0.10f, kMaterialWall);
    if (style.glow > 0.42f || style.corruption > 0.36f) {
        AddOctahedron(geo, Vec3{cx, baseY + h * 0.54f, z - 0.040f}, span * 0.065f, accentMaterial);
    }
}

void AddGothicRibDetail(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t accentMaterial) {
    const float zBack = room.center.y - room.halfSize.y + 0.18f;
    const float zFront = room.center.y + room.halfSize.y - 0.18f;
    const float ribHeight = 1.35f + style.decay * 0.36f + style.glow * 0.20f;
    for (int i = -2; i <= 2; ++i) {
        const float x = room.center.x + room.halfSize.x * static_cast<float>(i) * 0.30f;
        const float w = 0.035f + style.corruption * 0.014f;
        AddBox(geo, Vec3{x - w, 0.0f, zBack - 0.035f}, Vec3{x + w, ribHeight, zBack + 0.035f}, kMaterialWall);
        AddPyramid(geo, Vec3{x, ribHeight, zBack}, 0.070f + style.corruption * 0.035f, 0.28f + style.decay * 0.10f, kMaterialWall);
        if ((i & 1) == 0) {
            AddGuideStrip(geo, Vec3{x, 0.070f, zBack + 0.045f}, Vec3{0.0f, 0.0f, 1.0f}, 0.46f + style.glow * 0.24f, 0.018f, accentMaterial);
        }
    }

    if (style.biome == VisualBiome::AbyssCrypt) {
        AddGuideStrip(geo, Vec3{room.center.x, 0.055f, zFront - 0.10f}, Vec3{1.0f, 0.0f, -0.20f}, room.halfSize.x * 1.18f, 0.018f, kMaterialWall);
    }
}

void AddWallReliefPanels(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t accentMaterial) {
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z1 = room.center.y + room.halfSize.y;
    const bool lush = style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary;
    const float panelTop = (lush ? 2.12f : 1.36f) + style.decay * 0.34f + style.glow * 0.22f;
    const float panelBottom = 0.38f;
    const float wallZ = z1 - 0.16f;
    const int panelCount = lush ? 4 : 4;

    for (int i = 0; i < panelCount; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(panelCount);
        const float cx = x0 + (x1 - x0) * (0.18f + t * 0.64f);
        const float halfW = room.halfSize.x * (lush ? 0.082f : 0.070f);
        const float rib = 0.026f + style.corruption * 0.010f;
        AddBox(geo, Vec3{cx - halfW - rib, panelBottom, wallZ - 0.040f}, Vec3{cx - halfW + rib, panelTop, wallZ + 0.040f}, kMaterialWall);
        AddBox(geo, Vec3{cx + halfW - rib, panelBottom, wallZ - 0.040f}, Vec3{cx + halfW + rib, panelTop, wallZ + 0.040f}, kMaterialWall);
        AddBox(geo, Vec3{cx - halfW, panelTop - rib, wallZ - 0.042f}, Vec3{cx + halfW, panelTop + rib, wallZ + 0.042f}, kMaterialWall);
        AddBox(geo, Vec3{cx - halfW * 0.92f, panelBottom + 0.46f, wallZ - 0.044f}, Vec3{cx + halfW * 0.92f, panelBottom + 0.50f, wallZ + 0.044f}, kMaterialWall);
        if (lush) {
            AddBox(geo, Vec3{cx - halfW * 0.70f, panelTop + 0.24f, wallZ - 0.046f}, Vec3{cx + halfW * 0.70f, panelTop + 0.30f, wallZ + 0.046f}, kMaterialWall);
        }
        AddBox(geo, Vec3{cx - halfW * 0.72f, panelBottom + 0.11f, wallZ - 0.048f}, Vec3{cx + halfW * 0.72f, panelBottom + 0.15f, wallZ + 0.048f}, accentMaterial);
        if (style.biome == VisualBiome::ArcaneLibrary || style.biome == VisualBiome::AbyssCrypt) {
            AddPyramid(geo, Vec3{cx, panelTop + 0.02f, wallZ}, halfW * 0.28f, 0.34f + style.corruption * 0.12f, kMaterialWall);
            AddOctahedron(geo, Vec3{cx, panelBottom + 0.34f, wallZ - 0.020f}, halfW * 0.14f, accentMaterial);
        }
    }

    const float sideX = x1 - 0.14f;
    const int sidePanels = lush ? 5 : 3;
    for (int i = 0; i < sidePanels; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(sidePanels);
        const float cz = room.center.y - room.halfSize.y * 0.56f + room.halfSize.y * 1.12f * t;
        const float h = (lush ? 1.32f : 0.70f) + style.decay * (lush ? 0.34f : 0.20f) + static_cast<float>(i & 1) * (lush ? 0.24f : 0.16f);
        AddBox(geo, Vec3{sideX - 0.040f, 0.28f, cz - 0.050f}, Vec3{sideX + 0.040f, 0.28f + h, cz + 0.050f}, kMaterialWall);
        AddBox(geo, Vec3{sideX - 0.052f, 0.28f + h * 0.54f, cz - 0.24f}, Vec3{sideX + 0.052f, 0.28f + h * 0.58f, cz + 0.24f}, accentMaterial);
        if (lush && (i & 1) == 0) {
            AddBox(geo, Vec3{sideX - 0.058f, 0.28f + h * 0.72f, cz - 0.16f}, Vec3{sideX + 0.058f, 0.28f + h * 0.76f, cz + 0.16f}, kMaterialHitSpark);
        }
    }
}

void AddEdgeLanternRun(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, uint32_t accentMaterial) {
    const int count = 6 + static_cast<int>(style.glow * 3.0f) + (style.biome == VisualBiome::AbyssCrypt ? 2 : 0);
    const float inset = 0.38f + style.decay * 0.10f;
    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xd40u + i * 149u));
        const int side = static_cast<int>(h & 3u);
        const float along = HashUnit(HashMix(h, 0x2fu)) * 1.46f - 0.73f;
        const float stagger = HashUnit(HashMix(h, 0x83u)) * 0.10f - 0.05f;
        float sx = along;
        float sy = -0.80f + stagger;
        if (side == 1) {
            sy = 0.80f + stagger;
        } else if (side == 2) {
            sx = -0.82f + stagger;
            sy = along;
        } else if (side == 3) {
            sx = 0.82f + stagger;
            sy = along;
        }

        const Vec2 p = RoomDecorPoint(room, sx, sy, inset);
        const float s = 0.12f + HashUnit(HashMix(h, 0x51u)) * 0.08f + style.glow * 0.035f;
        AddCylinder(geo, Vec3{p.x, 0.0f, p.y}, s * 0.34f, s * 0.62f, kMaterialWall, 8);
        AddEllipsoid(geo, Vec3{p.x, s * 0.88f, p.y}, Vec3{s * 0.27f, s * 0.32f, s * 0.25f}, accentMaterial, 8, 4);
        AddFlatRing(geo, Vec3{p.x, 0.018f, p.y}, s * 0.74f, 0.018f, kMaterialWall);
    }
}

void AddProceduralStatue(
    GeneratedRTGeometry& geo,
    Vec2 position,
    Vec2 facing,
    float scale,
    uint32_t seed,
    uint32_t bodyMaterial,
    uint32_t accentMaterial) {
    const float s = std::max(0.18f, scale);
    const Vec2 dir2 = Normalize2(facing, Vec2{0.0f, -1.0f});
    const Vec2 right2{-dir2.y, dir2.x};
    const Vec3 dir{dir2.x, 0.0f, dir2.y};
    const Vec3 right{right2.x, 0.0f, right2.y};
    const Vec3 p{position.x, 0.0f, position.y};
    const bool broken = (seed & 3u) == 0u;

    AddBox(
        geo,
        Vec3{p.x - s * 0.42f, 0.0f, p.z - s * 0.34f},
        Vec3{p.x + s * 0.42f, s * 0.20f, p.z + s * 0.34f},
        bodyMaterial);
    AddCylinder(geo, Vec3{p.x, s * 0.20f, p.z}, s * 0.18f, s * (broken ? 0.64f : 0.88f), bodyMaterial, 9);
    AddEllipsoid(
        geo,
        Vec3{p.x, s * (broken ? 0.78f : 1.00f), p.z},
        Vec3{s * 0.26f, s * 0.20f, s * 0.18f},
        bodyMaterial,
        8,
        4);
    if (!broken) {
        AddEllipsoid(
            geo,
            Vec3{p.x + dir.x * s * 0.04f, s * 1.28f, p.z + dir.z * s * 0.04f},
            Vec3{s * 0.16f, s * 0.19f, s * 0.15f},
            bodyMaterial,
            8,
            4);
        AddPyramid(geo, Vec3{p.x - dir.x * s * 0.03f, s * 1.40f, p.z - dir.z * s * 0.03f}, s * 0.17f, s * 0.24f, bodyMaterial);
    } else {
        AddPyramid(geo, Vec3{p.x + right.x * s * 0.22f, s * 0.52f, p.z + right.z * s * 0.22f}, s * 0.12f, s * 0.22f, bodyMaterial);
    }

    const Vec3 shoulder{p.x, s * 0.92f, p.z};
    AddBlade(geo, Add(shoulder, Add(Scale(right, s * 0.28f), Scale(dir, s * 0.04f))), right, s * 0.28f, accentMaterial);
    AddBlade(geo, Add(shoulder, Add(Scale(right, -s * 0.28f), Scale(dir, s * 0.04f))), Scale(right, -1.0f), s * 0.24f, accentMaterial);
    AddGuideStrip(geo, Vec3{p.x + dir.x * s * 0.30f, s * 0.52f, p.z + dir.z * s * 0.30f}, dir, s * 0.62f, s * 0.018f, accentMaterial);

    if ((seed & 1u) == 0u) {
        AddFlatRing(geo, Vec3{p.x, 0.020f, p.z}, s * 0.58f, s * 0.024f, accentMaterial);
    }
}

void AddPlanterCluster(GeneratedRTGeometry& geo, Vec2 position, float scale, uint32_t seed, const RoomVisualStyle& style) {
    const float s = std::max(0.16f, scale);
    AddBox(
        geo,
        Vec3{position.x - s * 0.52f, 0.0f, position.y - s * 0.22f},
        Vec3{position.x + s * 0.52f, s * 0.22f, position.y + s * 0.22f},
        kMaterialWall);
    const int clumps = style.biome == VisualBiome::OvergrownSanctuary ? 5 : 3;
    for (int i = 0; i < clumps; ++i) {
        const uint32_t h = HashMix(seed, static_cast<uint32_t>(0xed0u + i * 47u));
        const float x = position.x + (HashUnit(HashMix(h, 0x17u)) - 0.5f) * s * 0.82f;
        const float z = position.y + (HashUnit(HashMix(h, 0x2bu)) - 0.5f) * s * 0.30f;
        AddLeafClump(
            geo,
            Vec2{x, z},
            s * (0.20f + HashUnit(HashMix(h, 0x41u)) * 0.18f + style.moss * 0.08f),
            h,
            kMaterialEnemySkirmisher,
            style.biome == VisualBiome::OvergrownSanctuary ? kMaterialWall : kMaterialEnemySkirmisher);
    }
}

void AddHangingVineCurtain(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    if (style.biome != VisualBiome::SunlitRuins && style.biome != VisualBiome::OvergrownSanctuary) {
        return;
    }

    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z = room.center.y + room.halfSize.y - 0.24f;
    const int count = style.biome == VisualBiome::OvergrownSanctuary ? 14 : 9;
    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xf510u + i * 71u));
        const float t = (static_cast<float>(i) + HashUnit(HashMix(h, 0x11u)) * 0.45f) / static_cast<float>(count);
        const float x = x0 + (x1 - x0) * (0.12f + t * 0.76f);
        const float yTop = 0.80f + HashUnit(HashMix(h, 0x23u)) * 0.40f;
        const float len = 0.42f + HashUnit(HashMix(h, 0x35u)) * (0.54f + style.moss * 0.36f);
        const float w = 0.008f + HashUnit(HashMix(h, 0x47u)) * 0.006f;
        AddBox(geo, Vec3{x - w, yTop - len, z - 0.030f}, Vec3{x + w, yTop, z + 0.030f}, kMaterialEnemySkirmisher);
        if ((i & 1) == 0) {
            AddLeafClump(geo, Vec2{x, z - 0.04f}, 0.070f + style.moss * 0.040f, h, kMaterialEnemySkirmisher, kMaterialWall);
        }
    }
}

void AddRitualSpineRun(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, uint32_t accentMaterial) {
    if (style.biome != VisualBiome::ArcaneLibrary && style.biome != VisualBiome::AbyssCrypt) {
        return;
    }

    const int count = style.biome == VisualBiome::AbyssCrypt ? 7 : 5;
    const float z = room.center.y + room.halfSize.y * 0.78f;
    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xfa20u + i * 61u));
        const float t = count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.0f;
        const float x = room.center.x - room.halfSize.x * 0.62f + room.halfSize.x * 1.24f * t;
        const float s = 0.12f + HashUnit(HashMix(h, 0x31u)) * 0.08f + style.corruption * 0.04f;
        AddPyramid(geo, Vec3{x, 0.0f, z + (HashUnit(HashMix(h, 0x47u)) - 0.5f) * 0.22f}, s, 0.74f + style.descent * 0.42f, kMaterialWall);
        if ((i & 1) == 0) {
            AddOctahedron(geo, Vec3{x, 0.72f + style.descent * 0.25f, z}, s * 0.38f, accentMaterial);
        }
    }
}

void AddCinematicWallMass(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style) {
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;
    const bool dark = style.biome == VisualBiome::ArcaneLibrary || style.biome == VisualBiome::AbyssCrypt;
    const float backHeight = 2.78f + style.decay * 0.54f + style.corruption * 0.46f + (dark ? 0.62f : 0.46f);
    const float sideHeight = 1.94f + style.decay * 0.36f + style.descent * 0.46f + (dark ? 0.38f : 0.28f);
    const float capHeight = backHeight + 0.16f + style.glow * 0.12f;

    AddBox(geo, Vec3{x0 - 0.22f, 0.0f, z1 + 0.035f}, Vec3{x1 + 0.22f, backHeight, z1 + 0.34f}, kMaterialWall);
    AddBox(geo, Vec3{x0 - 0.30f, backHeight - 0.080f, z1 - 0.035f}, Vec3{x1 + 0.30f, capHeight, z1 + 0.40f}, kMaterialWall);
    AddBox(geo, Vec3{x1 + 0.035f, 0.0f, z0 + room.halfSize.y * 0.14f}, Vec3{x1 + 0.34f, sideHeight, z1 + 0.24f}, kMaterialWall);
    AddBox(geo, Vec3{x0 - 0.34f, 0.0f, z0 + room.halfSize.y * 0.32f}, Vec3{x0 - 0.035f, sideHeight * 0.86f, z1 + 0.16f}, kMaterialWall);

    const uint32_t relief = dark ? (style.biome == VisualBiome::AbyssCrypt ? kMaterialHitSpark : kMaterialRoomPulse) : kMaterialWall;
    for (int i = 0; i < 8; ++i) {
        const float t = static_cast<float>(i) / 7.0f;
        const float x = x0 + (x1 - x0) * (0.08f + t * 0.84f);
        const float w = 0.030f + style.decay * 0.010f;
        const float h = backHeight * (0.58f + static_cast<float>(i & 1) * 0.14f + (dark ? 0.16f : 0.02f));
        AddBox(geo, Vec3{x - w, 0.08f, z1 - 0.010f}, Vec3{x + w, h, z1 + 0.050f}, kMaterialWall);
        if (dark && (i & 1) == 0) {
            AddPyramid(geo, Vec3{x, h, z1 + 0.016f}, 0.070f + style.corruption * 0.020f, 0.28f + style.descent * 0.12f, kMaterialWall);
            AddOctahedron(geo, Vec3{x, h * 0.58f, z1 - 0.020f}, 0.040f + style.glow * 0.012f, relief);
        }
    }

    for (int i = 0; i < 4; ++i) {
        const float t = static_cast<float>(i) / 3.0f;
        const float z = z0 + (z1 - z0) * (0.16f + t * 0.72f);
        const float h = sideHeight * (0.64f + static_cast<float>(i & 1) * 0.18f);
        AddBox(geo, Vec3{x1 + 0.075f, 0.04f, z - 0.040f}, Vec3{x1 + 0.37f, h, z + 0.040f}, kMaterialWall);
        if (!dark && (i & 1) == 0) {
            AddLeafClump(geo, Vec2{x1 + 0.22f, z}, 0.10f + style.moss * 0.05f, HashMix(VisualStyleHash(style), static_cast<uint32_t>(0x9d20u + i * 41u)), kMaterialEnemySkirmisher, kMaterialWall);
        }
    }
}

void AddReferenceShotLightRig(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;
    const bool lush = style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary;
    const uint32_t accent = lush ? kMaterialHitSpark :
        (style.biome == VisualBiome::AbyssCrypt ? kMaterialHitSpark : kMaterialRoomPulse);

    if (lush) {
        const float winX = x1 - room.halfSize.x * (0.36f + HashUnit(HashMix(seedBase, 0x51u)) * 0.10f);
        const float winZ = z1 + 0.075f;
        const float w = room.halfSize.x * 0.34f;
        const float y0 = 0.78f;
        const float y1 = 3.36f + style.glow * 0.44f;
        AddBox(geo, Vec3{winX - w, y0 - 0.035f, winZ - 0.038f}, Vec3{winX + w, y0 + 0.025f, winZ + 0.038f}, kMaterialWall);
        AddBox(geo, Vec3{winX - w, y1 - 0.025f, winZ - 0.038f}, Vec3{winX + w, y1 + 0.035f, winZ + 0.038f}, kMaterialWall);
        AddBox(geo, Vec3{winX - w - 0.030f, y0, winZ - 0.038f}, Vec3{winX - w + 0.026f, y1, winZ + 0.038f}, kMaterialWall);
        AddBox(geo, Vec3{winX + w - 0.026f, y0, winZ - 0.038f}, Vec3{winX + w + 0.030f, y1, winZ + 0.038f}, kMaterialWall);
        for (int i = 0; i < 4; ++i) {
            const float t = (static_cast<float>(i) + 1.0f) / 5.0f;
            const float x = winX - w + w * 2.0f * t;
            AddBox(geo, Vec3{x - 0.010f, y0 + 0.02f, winZ - 0.052f}, Vec3{x + 0.010f, y1 - 0.02f, winZ - 0.044f}, accent);
        }
        for (int i = 0; i < 4; ++i) {
            const float y = y0 + (y1 - y0) * (0.18f + static_cast<float>(i) * 0.20f);
            AddBox(geo, Vec3{winX - w + 0.050f, y - 0.010f, winZ - 0.053f}, Vec3{winX + w - 0.050f, y + 0.010f, winZ - 0.044f}, accent);
        }

        const Vec3 rayDir{-0.82f, 0.0f, -0.48f};
        for (int i = 0; i < 36; ++i) {
            const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x6710u + i * 23u));
            const float lane = (HashUnit(HashMix(h, 0x11u)) - 0.5f) * room.halfSize.x * 0.76f;
            const float along = HashUnit(HashMix(h, 0x23u)) * room.halfSize.x * (1.08f + style.glow * 0.24f);
            const float drift = (HashUnit(HashMix(h, 0x35u)) - 0.5f) * 0.18f;
            const Vec3 p{
                winX + lane * 0.32f + rayDir.x * along * 0.62f,
                0.046f + HashUnit(HashMix(h, 0x47u)) * (0.28f + style.glow * 0.12f),
                room.center.y + room.halfSize.y * 0.36f + rayDir.z * along * 0.58f + drift
            };
            AddOctahedron(geo, p, 0.012f + HashUnit(HashMix(h, 0x59u)) * (0.017f + style.glow * 0.008f), accent);
            if ((i % 4) == 0) {
                const float sx = 0.042f + HashUnit(HashMix(h, 0x71u)) * 0.075f;
                AddBox(
                    geo,
                    Vec3{p.x - sx, 0.002f, p.z - sx * 0.42f},
                    Vec3{p.x + sx, 0.006f, p.z + sx * 0.42f},
                    kMaterialFloor);
            }
        }
        AddLeafClump(geo, Vec2{winX + w * 0.84f, winZ - 0.04f}, 0.16f + style.moss * 0.08f, HashMix(seedBase, 0x61u), kMaterialEnemySkirmisher, kMaterialWall);
        return;
    }

    const float altarX = x1 - room.halfSize.x * 0.22f;
    const float altarZ = z1 - room.halfSize.y * 0.02f;
    AddBox(geo, Vec3{altarX - 0.46f, 0.00f, altarZ - 0.10f}, Vec3{altarX + 0.46f, 0.18f, altarZ + 0.22f}, kMaterialWall);
    AddBox(geo, Vec3{altarX - 0.32f, 0.18f, altarZ - 0.06f}, Vec3{altarX + 0.32f, 0.48f + style.descent * 0.18f, altarZ + 0.18f}, kMaterialWall);
    AddPyramid(geo, Vec3{altarX, 0.48f + style.descent * 0.18f, altarZ + 0.02f}, 0.24f + style.corruption * 0.06f, 0.64f + style.descent * 0.18f, kMaterialWall);
    AddOctahedron(geo, Vec3{altarX, 0.96f + style.glow * 0.18f, altarZ - 0.020f}, 0.12f + style.glow * 0.030f, accent);
    for (int i = 0; i < 6; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x6a20u + i * 31u));
        const float x = x0 + (x1 - x0) * (0.08f + HashUnit(HashMix(h, 0x11u)) * 0.84f);
        const float z = z0 + (z1 - z0) * (0.10f + HashUnit(HashMix(h, 0x21u)) * 0.74f);
        const float a = -0.85f + HashUnit(HashMix(h, 0x31u)) * 1.70f;
        AddGuideStrip(
            geo,
            Vec3{x, 0.050f + HashUnit(HashMix(h, 0x41u)) * 0.012f, z},
            Vec3{std::cos(a), 0.0f, std::sin(a)},
            room.halfSize.x * (0.28f + style.corruption * 0.20f),
            0.012f + style.glow * 0.006f,
            accent);
    }
}

void AddCathedralWindowRig(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    if (style.biome != VisualBiome::SunlitRuins && style.biome != VisualBiome::OvergrownSanctuary) {
        return;
    }

    const float z = room.center.y + room.halfSize.y + 0.018f;
    const float cx = room.center.x + room.halfSize.x * (0.33f + HashUnit(HashMix(seedBase, 0x74u)) * 0.12f);
    const float halfW = room.halfSize.x * 0.28f;
    const float y0 = 0.62f;
    const float y1 = 3.44f + style.glow * 0.42f;
    const float rib = 0.026f;
    const uint32_t warmMat = kMaterialHitSpark;

    AddBox(geo, Vec3{cx - halfW - rib, y0 - rib, z - 0.020f}, Vec3{cx + halfW + rib, y0 + rib, z + 0.020f}, kMaterialWall);
    AddBox(geo, Vec3{cx - halfW - rib, y1 - rib, z - 0.020f}, Vec3{cx + halfW + rib, y1 + rib, z + 0.020f}, kMaterialWall);
    AddBox(geo, Vec3{cx - halfW - rib, y0, z - 0.020f}, Vec3{cx - halfW + rib, y1, z + 0.020f}, kMaterialWall);
    AddBox(geo, Vec3{cx + halfW - rib, y0, z - 0.020f}, Vec3{cx + halfW + rib, y1, z + 0.020f}, kMaterialWall);

    for (int i = 0; i < 5; ++i) {
        const float t = static_cast<float>(i + 1) / 6.0f;
        const float x = cx - halfW + halfW * 2.0f * t;
        AddBox(geo, Vec3{x - rib * 0.56f, y0 + 0.02f, z - 0.030f}, Vec3{x + rib * 0.56f, y1 - 0.02f, z + 0.030f}, warmMat);
    }
    for (int i = 0; i < 4; ++i) {
        const float t = static_cast<float>(i + 1) / 5.0f;
        const float y = y0 + (y1 - y0) * t;
        AddBox(geo, Vec3{cx - halfW + 0.03f, y - rib * 0.48f, z - 0.030f}, Vec3{cx + halfW - 0.03f, y + rib * 0.48f, z + 0.030f}, warmMat);
    }

    const Vec3 beamDir{-0.86f, 0.0f, -0.50f};
    for (int i = 0; i < 28; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x7e20u + i * 19u));
        const float offset = (HashUnit(HashMix(h, 0x13u)) - 0.5f) * 0.76f;
        const float along = HashUnit(HashMix(h, 0x29u)) * room.halfSize.x * (0.82f + style.glow * 0.22f);
        const Vec3 p{
            cx - room.halfSize.x * 0.10f + offset + beamDir.x * along * 0.55f,
            0.048f + HashUnit(HashMix(h, 0x37u)) * 0.25f,
            room.center.y + room.halfSize.y * 0.30f + beamDir.z * along * 0.52f
        };
        AddOctahedron(geo, p, 0.011f + HashUnit(HashMix(h, 0x49u)) * 0.015f, warmMat);
    }
}

void AddInfernalBackRig(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    if (style.biome != VisualBiome::ArcaneLibrary && style.biome != VisualBiome::AbyssCrypt) {
        return;
    }

    const float z = room.center.y + room.halfSize.y + 0.010f;
    const float cx = room.center.x + (HashUnit(HashMix(seedBase, 0x87u)) - 0.5f) * room.halfSize.x * 0.20f;
    const float s = style.biome == VisualBiome::AbyssCrypt ? 1.0f : 0.82f;
    const uint32_t glowMat = style.biome == VisualBiome::AbyssCrypt ? kMaterialHitSpark : kMaterialRoomPulse;

    AddBox(geo, Vec3{cx - 0.46f * s, 0.0f, z - 0.055f}, Vec3{cx + 0.46f * s, 0.18f * s, z + 0.070f}, kMaterialWall);
    AddBox(geo, Vec3{cx - 0.34f * s, 0.18f * s, z - 0.045f}, Vec3{cx + 0.34f * s, 0.68f * s, z + 0.060f}, kMaterialWall);
    AddPyramid(geo, Vec3{cx, 0.68f * s, z}, 0.28f * s, 0.56f * s + style.corruption * 0.12f, kMaterialWall);
    AddOctahedron(geo, Vec3{cx, 0.74f * s, z - 0.030f}, 0.11f * s + style.glow * 0.030f, glowMat);
    AddBox(geo, Vec3{cx - 0.66f * s, 0.0f, z - 0.050f}, Vec3{cx - 0.58f * s, 1.16f * s, z + 0.055f}, kMaterialWall);
    AddBox(geo, Vec3{cx + 0.58f * s, 0.0f, z - 0.050f}, Vec3{cx + 0.66f * s, 1.16f * s, z + 0.055f}, kMaterialWall);

    for (int i = 0; i < 5; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xecd0u + i * 37u));
        const float x = room.center.x + (HashUnit(h) - 0.5f) * room.halfSize.x * 1.35f;
        const float y = 0.060f + static_cast<float>(i & 1) * 0.006f;
        const float angle = -0.95f + HashUnit(HashMix(h, 0x24u)) * 1.90f;
        AddGuideStrip(
            geo,
            Vec3{x, y, room.center.y + (HashUnit(HashMix(h, 0x51u)) - 0.5f) * room.halfSize.y * 0.86f},
            Vec3{std::cos(angle), 0.0f, std::sin(angle)},
            room.halfSize.x * (0.46f + style.corruption * 0.22f),
            0.014f + style.corruption * 0.008f,
            glowMat);
    }
}

void AddLushReferenceOvergrowth(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase) {
    if (style.biome != VisualBiome::SunlitRuins && style.biome != VisualBiome::OvergrownSanctuary) {
        return;
    }

    const int patchCount = style.biome == VisualBiome::OvergrownSanctuary ? 18 : 12;
    for (int i = 0; i < patchCount; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xfd10u + i * 67u));
        const int side = static_cast<int>(HashMix(h, 0x29u) & 3u);
        const float along = HashUnit(HashMix(h, 0x31u)) * 1.64f - 0.82f;
        const float inward = HashUnit(HashMix(h, 0x43u)) * 0.18f;
        float sx = along;
        float sy = 0.80f - inward;
        if (side == 1) {
            sy = -0.80f + inward;
        } else if (side == 2) {
            sx = -0.82f + inward;
            sy = along;
        } else if (side == 3) {
            sx = 0.82f - inward;
            sy = along;
        }

        const Vec2 p = RoomDecorPoint(room, sx, sy, 0.50f);
        const float s = 0.16f + HashUnit(HashMix(h, 0x5du)) * 0.11f + style.moss * 0.08f;
        AddCornerPatch(geo, p, Vec2{s * 0.70f, s * 0.56f}, 0.0f, 0.0f, s * (0.72f + style.moss * 0.16f), kMaterialEnemySkirmisher);
        AddLeafClump(geo, p, s * (0.62f + style.moss * 0.22f), h, kMaterialEnemySkirmisher, kMaterialWall);
        if ((i % 4) == 0) {
            AddLeafClump(
                geo,
                Vec2{p.x + (HashUnit(HashMix(h, 0x81u)) - 0.5f) * s, p.y + (HashUnit(HashMix(h, 0x91u)) - 0.5f) * s},
                s * 0.48f,
                HashMix(h, 0xa7u),
                kMaterialEnemySkirmisher,
                kMaterialControl);
        }
    }
}

void AddSegmentedBackArch(
    GeneratedRTGeometry& geo,
    Vec3 center,
    float halfSpan,
    float baseY,
    float archHeight,
    float depth,
    uint32_t stoneMaterial,
    uint32_t accentMaterial,
    uint32_t seed) {
    const float columnRadius = std::max(0.035f, halfSpan * 0.085f);
    AddCylinder(geo, Vec3{center.x - halfSpan, 0.0f, center.z}, columnRadius, baseY + archHeight * 0.62f, stoneMaterial, 8);
    AddCylinder(geo, Vec3{center.x + halfSpan, 0.0f, center.z}, columnRadius, baseY + archHeight * 0.56f, stoneMaterial, 8);

    constexpr int kSegments = 9;
    for (int i = 0; i < kSegments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSegments - 1);
        const float angle = 3.14159265f * (1.0f - t);
        const float x = center.x + std::cos(angle) * halfSpan;
        const float y = baseY + std::sin(angle) * archHeight;
        const uint32_t h = HashMix(seed, static_cast<uint32_t>(0xa110u + i * 31u));
        const float jitter = (HashUnit(HashMix(h, 0x13u)) - 0.5f) * halfSpan * 0.035f;
        const float blockW = halfSpan * (0.070f + HashUnit(HashMix(h, 0x27u)) * 0.022f);
        const float blockH = archHeight * (0.070f + HashUnit(HashMix(h, 0x3bu)) * 0.026f);
        AddBox(
            geo,
            Vec3{x - blockW + jitter, y - blockH, center.z - depth},
            Vec3{x + blockW + jitter, y + blockH, center.z + depth},
            stoneMaterial);
        if ((i == 2 || i == 6) && accentMaterial != stoneMaterial) {
            AddBox(
                geo,
                Vec3{x - blockW * 0.24f + jitter, y - blockH * 0.42f, center.z - depth * 1.18f},
                Vec3{x + blockW * 0.24f + jitter, y + blockH * 0.42f, center.z - depth * 1.08f},
                accentMaterial);
        }
    }
}

void AddReferenceArcadeLayer(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, uint32_t accentMaterial) {
    const float zBack = room.center.y + room.halfSize.y + 0.040f;
    const float zSide = room.center.y + room.halfSize.y * 0.20f;
    const float x0 = room.center.x - room.halfSize.x * 0.62f;
    const float bay = room.halfSize.x * 0.44f;
    const int bayCount = style.biome == VisualBiome::AbyssCrypt ? 4 : 3;
    const float baseY = style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary ? 0.66f : 0.56f;
    const float height = 0.46f + style.decay * 0.22f + style.corruption * 0.16f;
    const float depth = 0.040f + style.decay * 0.010f;

    for (int i = 0; i < bayCount; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xa520u + i * 79u));
        const float x = x0 + bay * static_cast<float>(i) + (HashUnit(HashMix(h, 0x21u)) - 0.5f) * bay * 0.08f;
        const uint32_t archAccent = (style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary)
            ? kMaterialWall
            : accentMaterial;
        AddSegmentedBackArch(
            geo,
            Vec3{x, 0.0f, zBack},
            room.halfSize.x * (0.110f + HashUnit(HashMix(h, 0x33u)) * 0.020f),
            baseY,
            height * (0.92f + HashUnit(HashMix(h, 0x45u)) * 0.18f),
            depth,
            kMaterialWall,
            archAccent,
            h);
    }

    const float sideX = room.center.x + room.halfSize.x + 0.055f;
    for (int i = 0; i < 3; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xa910u + i * 67u));
        const float z = zSide - room.halfSize.y * 0.56f + room.halfSize.y * 0.42f * static_cast<float>(i);
        const float s = 0.10f + HashUnit(HashMix(h, 0x11u)) * 0.030f + style.decay * 0.020f;
        AddCylinder(geo, Vec3{sideX, 0.0f, z}, s, 0.74f + style.decay * 0.28f + style.descent * 0.22f, kMaterialWall, 7);
        AddPyramid(geo, Vec3{sideX, 0.72f + style.decay * 0.26f, z}, s * 0.74f, 0.28f + style.corruption * 0.12f, kMaterialWall);
        if (style.biome == VisualBiome::ArcaneLibrary || style.biome == VisualBiome::AbyssCrypt) {
            AddOctahedron(geo, Vec3{sideX - 0.030f, 0.56f + style.glow * 0.12f, z}, s * 0.30f, accentMaterial);
        }
    }
}

void AddReferenceHeroSetpiece(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, uint32_t accentMaterial) {
    if (style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary) {
        const Vec2 fountain = RoomDecorPoint(room, -0.66f, -0.70f, 0.52f);
        const float s = 0.28f + style.moss * 0.08f;
        AddCylinder(geo, Vec3{fountain.x, 0.0f, fountain.y}, s, 0.11f, kMaterialWall, 12);
        AddFlatRing(geo, Vec3{fountain.x, 0.125f, fountain.y}, s * 1.10f, 0.030f, kMaterialWall);
        AddEllipsoid(geo, Vec3{fountain.x, 0.22f, fountain.y}, Vec3{s * 0.20f, s * 0.15f, s * 0.18f}, kMaterialProjectile, 8, 3);
        AddLeafClump(geo, Vec2{fountain.x + s * 0.48f, fountain.y - s * 0.20f}, s * 0.48f, HashMix(seedBase, 0xb113u), kMaterialEnemySkirmisher, kMaterialWall);
        AddLeafClump(geo, Vec2{fountain.x - s * 0.34f, fountain.y + s * 0.30f}, s * 0.40f, HashMix(seedBase, 0xb127u), kMaterialEnemySkirmisher, kMaterialWall);
        return;
    }

    const Vec2 altar = RoomDecorPoint(room, 0.62f, 0.70f, 0.50f);
    const float s = style.biome == VisualBiome::AbyssCrypt ? 0.54f : 0.44f;
    AddBox(geo, Vec3{altar.x - s * 0.46f, 0.0f, altar.y - s * 0.30f}, Vec3{altar.x + s * 0.46f, s * 0.26f, altar.y + s * 0.30f}, kMaterialWall);
    AddPyramid(geo, Vec3{altar.x, s * 0.26f, altar.y}, s * 0.30f, s * (0.62f + style.corruption * 0.22f), kMaterialWall);
    AddEllipsoid(geo, Vec3{altar.x, s * (0.78f + style.glow * 0.16f), altar.y}, Vec3{s * 0.18f, s * 0.16f, s * 0.16f}, accentMaterial, 8, 3);
    for (int i = 0; i < 4; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xb540u + i * 41u));
        const float a = static_cast<float>(i) * 1.5707963f + HashUnit(HashMix(h, 0x21u)) * 0.24f;
        AddGuideStrip(
            geo,
            Vec3{altar.x + std::cos(a) * s * 0.78f, 0.040f, altar.y + std::sin(a) * s * 0.78f},
            Vec3{std::cos(a + 0.40f), 0.0f, std::sin(a + 0.40f)},
            s * (0.76f + style.corruption * 0.28f),
            0.014f + style.glow * 0.004f,
            accentMaterial);
    }
}

void AddReferenceAccentField(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, uint32_t accentMaterial) {
    const float stagePad = (1.25f + style.decay * 0.30f) * (3.20f + style.fog * 0.55f + style.descent * 0.22f);
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;

    if (style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary) {
        const int lanterns = style.biome == VisualBiome::OvergrownSanctuary ? 8 : 6;
        for (int i = 0; i < lanterns; ++i) {
            const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xc120u + i * 53u));
            const float side = static_cast<float>(i & 3);
            const float t = (static_cast<float>(i) + HashUnit(HashMix(h, 0x31u)) * 0.40f) / static_cast<float>(lanterns);
            Vec2 p{};
            if (side < 1.0f) {
                p = Vec2{x0 + (x1 - x0) * (0.16f + t * 0.68f), z1 + 0.36f + HashUnit(HashMix(h, 0x44u)) * stagePad * 0.10f};
            } else if (side < 2.0f) {
                p = Vec2{x1 + 0.34f + HashUnit(HashMix(h, 0x55u)) * stagePad * 0.08f, z0 + (z1 - z0) * (0.16f + t * 0.66f)};
            } else if (side < 3.0f) {
                p = Vec2{x0 + (x1 - x0) * (0.14f + t * 0.70f), z0 - 0.30f - HashUnit(HashMix(h, 0x66u)) * stagePad * 0.07f};
            } else {
                p = Vec2{x0 - 0.30f - HashUnit(HashMix(h, 0x77u)) * stagePad * 0.08f, z0 + (z1 - z0) * (0.20f + t * 0.58f)};
            }

            const float s = 0.11f + HashUnit(HashMix(h, 0x89u)) * 0.030f;
            AddCylinder(geo, Vec3{p.x, 0.0f, p.y}, s * 0.82f, 0.16f, kMaterialWall, 8);
            AddEllipsoid(geo, Vec3{p.x, 0.22f + style.glow * 0.05f, p.y}, Vec3{s * 0.36f, s * 0.42f, s * 0.32f}, kMaterialProjectile, 8, 4);
            for (int m = 0; m < 5; ++m) {
                const uint32_t mh = HashMix(h, static_cast<uint32_t>(0xa0u + m * 13u));
                const float a = HashUnit(HashMix(mh, 0x17u)) * 6.2831853f;
                const float rr = s * (0.52f + HashUnit(HashMix(mh, 0x23u)) * 0.72f);
                AddEllipsoid(
                    geo,
                    Vec3{p.x + std::cos(a) * rr, 0.036f + HashUnit(HashMix(mh, 0x31u)) * 0.030f, p.y + std::sin(a) * rr},
                    Vec3{s * (0.055f + HashUnit(HashMix(mh, 0x47u)) * 0.040f), s * 0.040f, s * (0.050f + HashUnit(HashMix(mh, 0x59u)) * 0.030f)},
                    (m & 1) == 0 ? kMaterialControl : kMaterialHitSpark,
                    6,
                    3);
            }
        }

        const int blossoms = style.biome == VisualBiome::OvergrownSanctuary ? 52 : 34;
        for (int i = 0; i < blossoms; ++i) {
            const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xc520u + i * 29u));
            const float a = HashUnit(HashMix(h, 0x17u)) * 6.2831853f;
            const float r = 0.72f + HashUnit(HashMix(h, 0x29u)) * 0.28f;
            const Vec2 p{
                room.center.x + std::cos(a) * room.halfSize.x * r,
                room.center.y + std::sin(a) * room.halfSize.y * r
            };
            const float s = 0.018f + HashUnit(HashMix(h, 0x41u)) * 0.012f;
            AddEllipsoid(
                geo,
                Vec3{p.x, 0.022f + HashUnit(HashMix(h, 0x57u)) * 0.012f, p.y},
                Vec3{s, s * 0.62f, s * 0.85f},
                ((i % 5) == 0) ? kMaterialControl : kMaterialHitSpark,
                6,
                3);
        }
        return;
    }

    const int fissures = style.biome == VisualBiome::AbyssCrypt ? 16 : 10;
    for (int i = 0; i < fissures; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xc910u + i * 43u));
        const float a = -0.90f + HashUnit(HashMix(h, 0x21u)) * 1.80f;
        const float px = room.center.x + (HashUnit(HashMix(h, 0x31u)) - 0.5f) * room.halfSize.x * 1.35f;
        const float pz = room.center.y + (HashUnit(HashMix(h, 0x43u)) - 0.5f) * room.halfSize.y * 1.35f;
        AddGuideStrip(
            geo,
            Vec3{px, 0.036f + HashUnit(HashMix(h, 0x55u)) * 0.010f, pz},
            Vec3{std::cos(a), 0.0f, std::sin(a)},
            room.halfSize.x * (0.28f + HashUnit(HashMix(h, 0x67u)) * 0.22f),
            0.012f + style.corruption * 0.006f,
            accentMaterial);
    }

    const int shrineLights = style.biome == VisualBiome::AbyssCrypt ? 10 : 7;
    for (int i = 0; i < shrineLights; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xcd10u + i * 61u));
        const float t = static_cast<float>(i) / static_cast<float>(std::max(1, shrineLights - 1));
        const bool back = (i & 1) == 0;
        const Vec2 p{
            back ? (x0 + (x1 - x0) * (0.12f + t * 0.76f)) : (x1 + 0.40f + HashUnit(HashMix(h, 0x33u)) * stagePad * 0.08f),
            back ? (z1 + 0.48f + HashUnit(HashMix(h, 0x47u)) * stagePad * 0.08f) : (z0 + (z1 - z0) * (0.14f + t * 0.68f))
        };
        AddCandleCluster(geo, p, 0.16f + style.glow * 0.10f + HashUnit(HashMix(h, 0x59u)) * 0.08f, accentMaterial);
        if ((i % 3) == 0) {
            AddPyramid(geo, Vec3{p.x, 0.0f, p.y}, 0.10f + style.corruption * 0.035f, 0.34f + style.descent * 0.16f, kMaterialWall);
            AddOctahedron(geo, Vec3{p.x, 0.38f + style.glow * 0.08f, p.y}, 0.050f + style.glow * 0.018f, accentMaterial);
        }
    }
}

void AddOverheadSilhouetteLayer(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, uint32_t seedBase, uint32_t accentMaterial) {
    const float x0 = room.center.x - room.halfSize.x;
    const float x1 = room.center.x + room.halfSize.x;
    const float z0 = room.center.y - room.halfSize.y;
    const float z1 = room.center.y + room.halfSize.y;
    const bool lush = style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary;
    const bool dark = style.biome == VisualBiome::ArcaneLibrary || style.biome == VisualBiome::AbyssCrypt;
    const float topY = 1.44f + style.decay * 0.24f + style.descent * 0.32f + (dark ? 0.28f : 0.10f);
    const float backZ = z1 + 0.14f;
    const float frontZ = z0 - 0.16f;
    const float sideInset = 0.12f + style.decay * 0.06f;
    const uint32_t shadowMaterial = kMaterialWall;

    AddBox(
        geo,
        Vec3{x0 - 0.20f, topY - 0.045f, backZ - 0.070f},
        Vec3{x1 + 0.24f, topY + 0.035f, backZ + 0.080f},
        shadowMaterial);
    AddBox(
        geo,
        Vec3{x1 + 0.040f, topY * 0.22f, z0 + room.halfSize.y * 0.08f},
        Vec3{x1 + 0.130f + style.decay * 0.045f, topY + 0.020f, z1 + 0.20f},
        shadowMaterial);
    AddBox(
        geo,
        Vec3{x0 - 0.120f - style.decay * 0.040f, topY * 0.26f, z0 + room.halfSize.y * 0.22f},
        Vec3{x0 - 0.038f, topY * 0.84f, z1 + 0.12f},
        shadowMaterial);

    const int ribCount = dark ? 6 : 5;
    for (int i = 0; i < ribCount; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xd710u + i * 73u));
        const float t = ribCount > 1 ? static_cast<float>(i) / static_cast<float>(ribCount - 1) : 0.0f;
        const float x = x0 + (x1 - x0) * (0.08f + t * 0.84f) + (HashUnit(HashMix(h, 0x11u)) - 0.5f) * 0.14f;
        const float y = topY - HashUnit(HashMix(h, 0x21u)) * 0.10f;
        const float z = backZ - 0.10f - HashUnit(HashMix(h, 0x31u)) * 0.12f;
        const Vec3 slant = Normalize(Vec3{-0.34f + HashUnit(HashMix(h, 0x41u)) * 0.22f, 0.0f, -0.94f});
        AddGuideStrip(
            geo,
            Vec3{x, y, z - room.halfSize.y * 0.10f},
            slant,
            room.halfSize.y * (0.52f + HashUnit(HashMix(h, 0x51u)) * 0.18f),
            0.020f + style.decay * 0.006f + (dark ? style.corruption * 0.008f : 0.0f),
            shadowMaterial);
        if (dark && (i & 1) == 0) {
            AddOctahedron(geo, Vec3{x, y - 0.24f - style.descent * 0.08f, z - 0.18f}, 0.040f + style.glow * 0.020f, accentMaterial);
            AddBox(geo, Vec3{x - 0.010f, y - 0.32f, z - 0.188f}, Vec3{x + 0.010f, y, z - 0.172f}, shadowMaterial);
        }
    }

    if (lush) {
        const int strands = style.biome == VisualBiome::OvergrownSanctuary ? 13 : 9;
        for (int i = 0; i < strands; ++i) {
            const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xda40u + i * 59u));
            const float t = (static_cast<float>(i) + HashUnit(HashMix(h, 0x13u)) * 0.42f) / static_cast<float>(strands);
            const float x = x0 + (x1 - x0) * (0.10f + t * 0.78f);
            const float len = 0.36f + HashUnit(HashMix(h, 0x29u)) * (0.62f + style.moss * 0.30f);
            const float w = 0.007f + HashUnit(HashMix(h, 0x37u)) * 0.006f;
            AddBox(geo, Vec3{x - w, topY - len, backZ - 0.055f}, Vec3{x + w, topY + 0.020f, backZ - 0.035f}, kMaterialEnemySkirmisher);
            if ((i & 1) == 0) {
                AddLeafClump(
                    geo,
                    Vec2{x + (HashUnit(HashMix(h, 0x53u)) - 0.5f) * 0.08f, backZ - 0.07f},
                    0.070f + style.moss * 0.050f,
                    h,
                    kMaterialEnemySkirmisher,
                    kMaterialWall);
            }
        }
        AddGuideStrip(
            geo,
            Vec3{room.center.x + room.halfSize.x * 0.12f, topY - 0.10f, frontZ + 0.12f},
            Vec3{1.0f, 0.0f, 0.10f},
            room.halfSize.x * 1.32f,
            0.024f,
            kMaterialWall);
        return;
    }

    const int chains = style.biome == VisualBiome::AbyssCrypt ? 7 : 5;
    for (int i = 0; i < chains; ++i) {
        const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xdf20u + i * 67u));
        const bool side = (i & 1) != 0;
        const float t = HashUnit(HashMix(h, 0x17u)) * 1.48f - 0.74f;
        const float x = side ? (x1 + sideInset + HashUnit(HashMix(h, 0x31u)) * 0.12f) : (room.center.x + t * room.halfSize.x);
        const float z = side ? (room.center.y + t * room.halfSize.y) : (z1 + 0.05f + HashUnit(HashMix(h, 0x43u)) * 0.22f);
        const float len = 0.32f + HashUnit(HashMix(h, 0x59u)) * (0.48f + style.descent * 0.22f);
        AddBox(geo, Vec3{x - 0.012f, topY - len, z - 0.012f}, Vec3{x + 0.012f, topY + 0.020f, z + 0.012f}, shadowMaterial);
        if ((i % 3) != 1) {
            AddPyramid(geo, Vec3{x, topY - len - 0.12f, z}, 0.045f + style.corruption * 0.018f, 0.22f + style.descent * 0.08f, shadowMaterial);
        }
        if ((i & 1) == 0) {
            AddOctahedron(geo, Vec3{x, topY - len * 0.52f, z}, 0.035f + style.glow * 0.018f, accentMaterial);
        }
    }
}

void AddStyleProps(GeneratedRTGeometry& geo, const Room& room, const RoomVisualStyle& style, int roomIndex) {
    const uint32_t seedBase = HashMix(VisualStyleHash(style), static_cast<uint32_t>(roomIndex * 977 + 0x51u));
    const float jitterA = HashUnit(HashMix(seedBase, 0x11u)) - 0.5f;
    const float jitterB = HashUnit(HashMix(seedBase, 0x22u)) - 0.5f;
    const float roomRadius = std::min(room.halfSize.x, room.halfSize.y);
    const float edgeInset = 0.42f + roomRadius * 0.02f;
    const uint32_t darkAccent = style.biome == VisualBiome::AbyssCrypt ? kMaterialHitSpark :
        (style.biome == VisualBiome::ArcaneLibrary ? kMaterialRoomPulse : kMaterialProjectile);
    const uint32_t reliefAccent = EnvReliefAccent(style);

    AddCinematicWallMass(geo, room, style);
    AddReferenceShotLightRig(geo, room, style, seedBase);
    AddCathedralWindowRig(geo, room, style, seedBase);
    AddInfernalBackRig(geo, room, style, seedBase);
    AddLushReferenceOvergrowth(geo, room, style, seedBase);
    AddReferenceArcadeLayer(geo, room, style, seedBase, darkAccent);
    AddReferenceHeroSetpiece(geo, room, style, seedBase, darkAccent);
    AddReferenceAccentField(geo, room, style, seedBase, darkAccent);
    if (style.biome == VisualBiome::ArcaneLibrary || style.biome == VisualBiome::AbyssCrypt || style.descent > 0.62f) {
        AddOverheadSilhouetteLayer(geo, room, style, seedBase, darkAccent);
    }
    AddRubbleScatter(geo, room, style, seedBase, 12 + static_cast<int>(style.decay * 8.0f + style.descent * 6.0f));
    AddWallReliefPanels(geo, room, style, reliefAccent);
    AddEdgeLanternRun(geo, room, style, seedBase, darkAccent);
    AddBrokenArchDetail(geo, room, style, reliefAccent);
    AddHangingVineCurtain(geo, room, style, seedBase);
    AddRitualSpineRun(geo, room, style, seedBase, darkAccent);

    switch (style.biome) {
    case VisualBiome::SunlitRuins:
        AddSquareColumn(geo, RoomDecorPoint(room, -0.74f, -0.62f, edgeInset), 0.28f + style.decay * 0.08f, 1.16f + style.glow * 0.18f, kMaterialWall, kMaterialWall);
        AddSquareColumn(geo, RoomDecorPoint(room, 0.76f, 0.62f, edgeInset), 0.24f + style.cracks * 0.06f, 0.94f + style.decay * 0.22f, kMaterialWall, kMaterialWall);
        AddProceduralStatue(geo, RoomDecorPoint(room, -0.52f, 0.72f, edgeInset), Vec2{0.25f, -1.0f}, 0.42f + style.decay * 0.10f, seedBase, kMaterialWall, kMaterialControl);
        AddPlanterCluster(geo, RoomDecorPoint(room, 0.46f, -0.76f, edgeInset), 0.52f + style.moss * 0.16f, HashMix(seedBase, 0x991u), style);
        AddLowBenchRun(geo, room, 0.18f + jitterA * 0.08f, -0.78f, true, kMaterialWall);
        AddVineCascade(geo, RoomDecorPoint(room, -0.86f, 0.24f + jitterB * 0.18f, edgeInset), roomRadius * (0.72f + style.moss * 0.30f), jitterA * 0.25f, kMaterialEnemySkirmisher);
        AddBotanicalEdgeDetail(geo, room, style, seedBase);
        AddFlowerbedScatter(geo, room, style, seedBase, 34);
        break;
    case VisualBiome::OvergrownSanctuary:
        AddSquareColumn(geo, RoomDecorPoint(room, -0.78f, -0.68f, edgeInset), 0.24f + style.moss * 0.08f, 0.86f + style.moss * 0.30f, kMaterialWall, kMaterialWall);
        AddProceduralStatue(geo, RoomDecorPoint(room, -0.56f, 0.70f, edgeInset), Vec2{0.28f, -1.0f}, 0.38f + style.decay * 0.12f, seedBase, kMaterialWall, kMaterialEnemySkirmisher);
        AddPlanterCluster(geo, RoomDecorPoint(room, 0.38f, -0.76f, edgeInset), 0.62f + style.moss * 0.22f, HashMix(seedBase, 0x992u), style);
        AddPlanterCluster(geo, RoomDecorPoint(room, -0.28f, -0.80f, edgeInset), 0.48f + style.moss * 0.18f, HashMix(seedBase, 0x993u), style);
        AddVineCascade(geo, RoomDecorPoint(room, 0.78f, -0.18f + jitterA * 0.16f, edgeInset), roomRadius * (0.98f + style.moss * 0.38f), jitterB * 0.22f, kMaterialEnemySkirmisher);
        AddVineCascade(geo, RoomDecorPoint(room, -0.70f, 0.72f, edgeInset), roomRadius * (0.70f + style.decay * 0.30f), jitterA * 0.18f, kMaterialEnemySkirmisher);
        AddCrystalCluster(geo, RoomDecorPoint(room, 0.70f, 0.66f, edgeInset), 0.46f + style.glow * 0.22f, kMaterialProjectile);
        AddBotanicalEdgeDetail(geo, room, style, seedBase);
        AddFlowerbedScatter(geo, room, style, seedBase, 48);
        break;
    case VisualBiome::ArcaneLibrary:
        AddBookshelfRun(geo, RoomDecorPoint(room, 0.0f, -0.86f, edgeInset), room.halfSize.x * 1.24f, true, kMaterialRoomPulse);
        AddBookshelfRun(geo, RoomDecorPoint(room, -0.88f, 0.02f, edgeInset), room.halfSize.y * 0.92f, false, kMaterialControl);
        AddProceduralStatue(geo, RoomDecorPoint(room, 0.58f, 0.68f, edgeInset), Vec2{-0.22f, -1.0f}, 0.46f + style.glow * 0.08f, seedBase, kMaterialWall, kMaterialRoomPulse);
        AddCandleCluster(geo, RoomDecorPoint(room, 0.72f, -0.62f, edgeInset), 0.56f + style.glow * 0.22f, kMaterialHitSpark);
        AddCandleCluster(geo, RoomDecorPoint(room, 0.66f, 0.62f, edgeInset), 0.46f + style.glow * 0.18f, kMaterialRoomPulse);
        AddCandleRow(geo, room, style, seedBase, 10, kMaterialHitSpark);
        AddCrystalCluster(geo, RoomDecorPoint(room, 0.02f + jitterA * 0.10f, 0.76f, edgeInset), 0.42f + style.corruption * 0.20f, kMaterialEnemyCaster);
        AddGothicRibDetail(geo, room, style, darkAccent);
        break;
    case VisualBiome::AbyssCrypt:
        AddAbyssSpire(geo, RoomDecorPoint(room, -0.78f, -0.66f, edgeInset), 0.60f + style.corruption * 0.26f, kMaterialEnemyBoss, kMaterialHitSpark);
        AddAbyssSpire(geo, RoomDecorPoint(room, 0.78f, 0.64f, edgeInset), 0.50f + style.glow * 0.22f, kMaterialWall, kMaterialEnemyBoss);
        AddProceduralStatue(geo, RoomDecorPoint(room, 0.52f, 0.72f, edgeInset), Vec2{-0.20f, -1.0f}, 0.50f + style.corruption * 0.12f, seedBase, kMaterialWall, kMaterialHitSpark);
        AddCrystalCluster(geo, RoomDecorPoint(room, 0.70f, -0.58f, edgeInset), 0.52f + style.glow * 0.24f, kMaterialPortal);
        AddBrokenArchDetail(geo, room, style, kMaterialWall);
        AddGothicRibDetail(geo, room, style, darkAccent);
        AddCandleCluster(geo, RoomDecorPoint(room, -0.68f, 0.48f, edgeInset), 0.40f + style.glow * 0.16f, kMaterialHitSpark);
        AddCandleRow(geo, room, style, seedBase, 14, kMaterialHitSpark);
        break;
    }

    if (style.corruption > 0.62f) {
        const Vec2 stainCenter = RoomDecorPoint(room, -0.10f + jitterA * 0.16f, -0.04f + jitterB * 0.16f);
        for (int i = 0; i < 9; ++i) {
            const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xf120u + i * 19u));
            const float a = HashUnit(HashMix(h, 0x11u)) * 6.2831853f;
            const float r = roomRadius * HashUnit(HashMix(h, 0x23u)) * (0.13f + style.corruption * 0.08f);
            const float s = 0.035f + HashUnit(HashMix(h, 0x35u)) * 0.055f;
            AddBox(
                geo,
                Vec3{stainCenter.x + std::cos(a) * r - s, 0.006f, stainCenter.y + std::sin(a) * r - s * 0.52f},
                Vec3{stainCenter.x + std::cos(a) * r + s, 0.012f, stainCenter.y + std::sin(a) * r + s * 0.52f},
                kMaterialFloor);
        }
    }
}

uint32_t StatusMaterial(StatusKind status) {
    switch (status) {
    case StatusKind::Wet:
        return kMaterialProjectile;
    case StatusKind::Burning:
        return kMaterialHitSpark;
    case StatusKind::Charged:
        return kMaterialPlayerBlade;
    case StatusKind::Chilled:
        return kMaterialEnemyBulwark;
    case StatusKind::None:
        return kMaterialControl;
    }
    return kMaterialControl;
}

uint32_t ElementTellMaterial(Element element) {
    switch (element) {
    case Element::Water:
        return kMaterialProjectile;
    case Element::Fire:
        return kMaterialHitSpark;
    case Element::Stone:
        return kMaterialWall;
    case Element::Electricity:
        return kMaterialPlayerBlade;
    case Element::Ice:
        return kMaterialEnemyBulwark;
    case Element::Air:
        return kMaterialControl;
    case Element::None:
        return kMaterialRoomPulse;
    }
    return kMaterialHitSpark;
}

uint32_t PackPlayerVisualTag(
    WeaponId weapon,
    Element element,
    WeaponActionIndex action = WeaponActionIndex::Action1,
    AttackShape shape = AttackShape::Cone,
    uint32_t variant = 0u) {
    return static_cast<uint32_t>(weapon) |
        (static_cast<uint32_t>(element) << 8u) |
        (static_cast<uint32_t>(action) << 16u) |
        (static_cast<uint32_t>(shape) << 20u) |
        ((variant & 0xffu) << 24u);
}

uint32_t PackEnemyVisualTag(
    EnemyKind kind,
    WeaponId weapon,
    Element element,
    WeaponActionIndex action = WeaponActionIndex::Action1,
    AttackShape shape = AttackShape::Cone,
    uint32_t variant = 0u) {
    return static_cast<uint32_t>(kind) |
        (static_cast<uint32_t>(weapon) << 8u) |
        (static_cast<uint32_t>(element) << 16u) |
        (static_cast<uint32_t>(action) << 24u) |
        (static_cast<uint32_t>(shape) << 25u) |
        ((variant & 0x7u) << 29u);
}

uint32_t PackActionVisualTag(WeaponId weapon, Element element, uint32_t flavor = 0u) {
    return static_cast<uint32_t>(weapon) |
        (static_cast<uint32_t>(element) << 16u) |
        (flavor << 24u);
}

Element ElementFromPackedValue(uint32_t value) {
    return value <= static_cast<uint32_t>(Element::Air) ? static_cast<Element>(value) : Element::None;
}

Element PlayerVisualTagElement(uint32_t visualTag) {
    return ElementFromPackedValue((visualTag >> 8u) & 0xffu);
}

WeaponActionIndex PlayerVisualTagAction(uint32_t visualTag) {
    return ((visualTag >> 16u) & 0x1u) != 0u ? WeaponActionIndex::Action2 : WeaponActionIndex::Action1;
}

AttackShape PlayerVisualTagShape(uint32_t visualTag) {
    const uint32_t shape = (visualTag >> 20u) & 0xfu;
    return shape <= static_cast<uint32_t>(AttackShape::TargetArea) ? static_cast<AttackShape>(shape) : AttackShape::Cone;
}

uint32_t PlayerVisualTagVariant(uint32_t visualTag) {
    return (visualTag >> 24u) & 0xffu;
}

Element EnemyVisualTagElement(uint32_t visualTag) {
    return ElementFromPackedValue((visualTag >> 16u) & 0xffu);
}

WeaponActionIndex EnemyVisualTagAction(uint32_t visualTag) {
    return ((visualTag >> 24u) & 0x1u) != 0u ? WeaponActionIndex::Action2 : WeaponActionIndex::Action1;
}

AttackShape EnemyVisualTagShape(uint32_t visualTag) {
    const uint32_t shape = (visualTag >> 25u) & 0xfu;
    return shape <= static_cast<uint32_t>(AttackShape::TargetArea) ? static_cast<AttackShape>(shape) : AttackShape::Cone;
}

uint32_t EnemyVisualTagVariant(uint32_t visualTag) {
    return (visualTag >> 29u) & 0x7u;
}

Element ActionVisualTagElement(uint32_t visualTag) {
    return ElementFromPackedValue((visualTag >> 16u) & 0xffu);
}

WeaponId ActionVisualTagWeapon(uint32_t visualTag) {
    const uint32_t weapon = visualTag & 0xffu;
    return weapon < static_cast<uint32_t>(WeaponId::Count) ? static_cast<WeaponId>(weapon) : WeaponId::Pistol;
}

Element ElementFromMaterialId(uint32_t materialId) {
    if (materialId == kMaterialProjectile) {
        return Element::Water;
    }
    if (materialId == kMaterialHitSpark) {
        return Element::Fire;
    }
    if (materialId == kMaterialWall) {
        return Element::Stone;
    }
    if (materialId == kMaterialPlayerBlade) {
        return Element::Electricity;
    }
    if (materialId == kMaterialEnemyBulwark) {
        return Element::Ice;
    }
    if (materialId == kMaterialControl) {
        return Element::Air;
    }
    return Element::None;
}

Element ElementFromStatusKind(StatusKind status) {
    switch (status) {
    case StatusKind::Wet:
        return Element::Water;
    case StatusKind::Burning:
        return Element::Fire;
    case StatusKind::Charged:
        return Element::Electricity;
    case StatusKind::Chilled:
        return Element::Ice;
    case StatusKind::None:
        return Element::None;
    }
    return Element::None;
}

EntityProxyKind EnemyTellKind(AttackShape shape) {
    switch (shape) {
    case AttackShape::Cone:
    case AttackShape::Burst:
        return EntityProxyKind::EnemyTellCone;
    case AttackShape::Projectile:
    case AttackShape::Wave:
    case AttackShape::Dash:
        return EntityProxyKind::EnemyTellLine;
    case AttackShape::Circle:
    case AttackShape::Orbit:
    case AttackShape::TargetArea:
        return EntityProxyKind::EnemyTellRing;
    }
    return EntityProxyKind::EnemyTellRing;
}

EntityProxyKind PlayerActionKind(AttackShape shape) {
    switch (shape) {
    case AttackShape::Cone:
        return EntityProxyKind::PlayerActionCone;
    case AttackShape::Projectile:
    case AttackShape::Wave:
    case AttackShape::Dash:
        return EntityProxyKind::PlayerActionLine;
    case AttackShape::Burst:
        return EntityProxyKind::PlayerActionBurst;
    case AttackShape::Circle:
    case AttackShape::Orbit:
    case AttackShape::TargetArea:
        return EntityProxyKind::PlayerActionBurst;
    }
    return EntityProxyKind::PlayerActionBurst;
}

Vec3 Direction3(Vec2 direction) {
    const Vec2 dir = Normalize2(direction, Vec2{1.0f, 0.0f});
    return Vec3{dir.x, 0.0f, dir.y};
}

float ActiveActionProgress(float timer, float duration) {
    return duration > 0.0001f ? Saturate(1.0f - timer / duration) : 1.0f;
}

float ActionPosePhase(float timer, float duration, float impactTime, float recovery) {
    if (duration <= 0.0001f || timer <= 0.0f) {
        return 0.0f;
    }

    const float elapsed = Clamp(duration - timer, 0.0f, duration);
    const float impact = impactTime > 0.0001f ? impactTime : std::max(0.0001f, duration * 0.34f);
    if (elapsed <= impact) {
        return Smooth01(elapsed / impact) * 0.68f;
    }

    const float tail = recovery > 0.0001f ? recovery : std::max(0.0001f, duration - impact);
    return 0.68f + Smooth01((elapsed - impact) / tail) * 0.32f;
}

float ActionImpactPulse(float timer, float duration, float impactTime) {
    if (duration <= 0.0001f || timer <= 0.0f) {
        return 0.0f;
    }

    const float elapsed = Clamp(duration - timer, 0.0f, duration);
    const float impact = impactTime > 0.0001f ? impactTime : std::max(0.0001f, duration * 0.34f);
    const float width = std::max(0.045f, duration * 0.16f);
    return Smooth01(1.0f - Saturate(std::abs(elapsed - impact) / width));
}

void PushActionProxy(
    std::vector<EntityRTProxy>& proxies,
    EntityProxyKind kind,
    Vec3 position,
    Vec3 direction,
    float radius,
    uint32_t materialId,
    float phase,
    uint32_t visualTag) {
    if (radius <= 0.0001f) {
        return;
    }
    proxies.push_back(EntityRTProxy{
        kind,
        position,
        direction,
        radius,
        materialId,
        phase,
        visualTag
    });
}

void AddPlayerActionProxy(std::vector<EntityRTProxy>& proxies, const PlayerState& player) {
    if (player.actionTimer <= 0.0f || player.actionDuration <= 0.0f) {
        return;
    }

    const WeaponSpec& spec = GetWeaponSpec(player.activeActionWeapon);
    const WeaponActionSpec& action = spec.actions[static_cast<std::size_t>(
        player.activeActionIndex == WeaponActionIndex::Action2 ? 1 : 0)];
    const float progress = ActionPosePhase(player.actionTimer, player.actionDuration, player.actionImpactTime, player.actionRecovery);
    const float impactProgress = ActionImpactPulse(player.actionTimer, player.actionDuration, player.actionImpactTime);
    const float pulse = 0.76f + impactProgress * 0.34f + progress * 0.08f;
    const float range = std::max(action.range, action.radius * 1.4f);
    const float radius = std::max(action.radius, 0.42f);
    const Vec3 dir = Direction3(player.activeActionDirection);
    const uint32_t materialId = ElementTellMaterial(player.activeActionElement);
    const uint32_t visualTag = PackActionVisualTag(player.activeActionWeapon, player.activeActionElement, spec.visualTag & 0xffu);
    const Vec3 origin = MakeVec3(player.activeActionOrigin, 0.045f + progress * 0.025f);
    const Vec3 target = MakeVec3(player.activeActionTarget, 0.046f + progress * 0.030f);

    auto push = [&](EntityProxyKind kind, Vec3 position, Vec3 direction, float proxyRadius, float scale = 1.0f) {
        PushActionProxy(proxies, kind, position, direction, proxyRadius * scale, materialId, progress, visualTag);
    };

    switch (player.activeActionWeapon) {
    case WeaponId::Hammer:
        push(EntityProxyKind::PlayerActionBurst, Add(origin, Scale(dir, radius * 0.12f)), dir, radius * 0.58f, pulse);
        push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, radius * 0.18f)), dir, radius * 0.46f, 0.42f + impactProgress * 0.16f);
        break;
    case WeaponId::Spear:
        push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.10f)), dir, range * 0.78f, 0.74f + impactProgress * 0.10f);
        push(EntityProxyKind::PlayerActionCone, Add(origin, Scale(dir, range * 0.82f)), dir, radius * 0.68f, 0.85f);
        break;
    case WeaponId::Katana: {
        const Vec2 left = Rotate2(player.activeActionDirection, -0.34f - progress * 0.12f);
        const Vec2 right = Rotate2(player.activeActionDirection, 0.30f + progress * 0.10f);
        if (player.activeActionShape == AttackShape::Wave) {
            push(EntityProxyKind::PlayerActionCone, Add(origin, Scale(dir, range * 0.18f)), dir, std::max(radius * 1.55f, range * 0.58f), 0.86f + impactProgress * 0.16f);
            push(EntityProxyKind::PlayerActionBurst, Add(origin, Scale(dir, radius * 0.16f)), dir, radius * 0.34f, 0.48f + progress * 0.14f);
            push(EntityProxyKind::PlayerActionCone, Add(origin, Scale(Direction3(left), range * 0.10f)), Direction3(left), std::max(radius * 1.08f, range * 0.32f), 0.42f);
            push(EntityProxyKind::PlayerActionCone, Add(origin, Scale(Direction3(right), range * 0.10f)), Direction3(right), std::max(radius * 0.98f, range * 0.28f), 0.36f);
            push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.08f)), Direction3(left), range * 0.14f, 0.24f);
            push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.08f)), Direction3(right), range * 0.12f, 0.22f);
        } else {
            push(EntityProxyKind::PlayerActionCone, origin, dir, std::max(radius * 1.55f, range * 0.54f), pulse);
            push(EntityProxyKind::PlayerActionCone, Add(origin, Scale(Direction3(left), range * 0.08f)), Direction3(left), range * 0.24f, 0.42f);
            push(EntityProxyKind::PlayerActionCone, Add(origin, Scale(Direction3(right), range * 0.08f)), Direction3(right), range * 0.20f, 0.36f);
            push(EntityProxyKind::PlayerActionLine, origin, Direction3(left), range * 0.18f, 0.26f);
            push(EntityProxyKind::PlayerActionLine, origin, Direction3(right), range * 0.16f, 0.24f);
        }
        break;
    }
    case WeaponId::Pistol:
        push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.14f)), dir, range * 0.70f, 0.64f);
        push(EntityProxyKind::PlayerActionBurst, Add(origin, Scale(dir, radius * 0.16f)), dir, radius * 0.34f, 0.42f + progress * 0.12f);
        break;
    case WeaponId::Rifle:
        push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.18f)), dir, range * 0.78f, 0.76f);
        push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.25f)), dir, range * 0.34f, 0.36f);
        break;
    case WeaponId::Machinegun:
        for (int i = -1; i <= 1; ++i) {
            const float angle = static_cast<float>(i) * (0.055f + progress * 0.025f);
            push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.10f)), Direction3(Rotate2(player.activeActionDirection, angle)), range * (0.44f + 0.08f * static_cast<float>(i + 1)), 0.58f);
        }
        break;
    case WeaponId::Shotgun:
        push(EntityProxyKind::PlayerActionBurst, origin, dir, range, 0.82f);
        push(EntityProxyKind::PlayerActionCone, origin, dir, range * 0.80f, 0.76f);
        push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.12f)), Direction3(Rotate2(player.activeActionDirection, -0.24f)), range * 0.34f, 0.48f);
        push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.12f)), Direction3(Rotate2(player.activeActionDirection, 0.24f)), range * 0.34f, 0.48f);
        break;
    case WeaponId::Staff:
        if (player.activeActionShape == AttackShape::Projectile) {
            push(EntityProxyKind::PlayerActionBurst, origin, dir, radius * 0.48f, 0.82f);
            push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.12f)), dir, range * 0.62f, 0.58f);
        } else {
            push(EntityProxyKind::PlayerActionBurst, Add(origin, Scale(dir, radius * 0.12f)), dir, std::min(range * 0.34f, radius * 1.18f), 0.72f + progress * 0.10f);
            push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.16f)), dir, range * 0.42f, 0.42f + impactProgress * 0.14f);
        }
        break;
    case WeaponId::Scepter:
        if (player.activeActionShape == AttackShape::TargetArea) {
            push(EntityProxyKind::PlayerActionBurst, target, dir, radius * 0.52f, 0.84f + progress * 0.08f);
            push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.12f)), dir, range * 0.44f, 0.34f);
        } else {
            push(EntityProxyKind::PlayerActionBurst, origin, dir, radius * 0.46f, 0.62f);
            push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, range * 0.12f)), dir, range * 0.58f, 0.64f);
        }
        break;
    case WeaponId::Gloves:
        push(EntityProxyKind::PlayerActionBurst, origin, dir, radius * 0.50f, 0.82f + impactProgress * 0.18f);
        push(EntityProxyKind::PlayerActionLine, Add(origin, Scale(dir, radius * 0.18f)), dir, radius * 0.44f, 0.44f + progress * 0.12f);
        break;
    case WeaponId::Count:
        push(PlayerActionKind(player.activeActionShape), origin, dir, std::max(range, radius), pulse);
        break;
    }
}

void AddEnemyActionAccentProxies(std::vector<EntityRTProxy>& proxies, const EnemyState& enemy) {
    if (enemy.actionTimer <= 0.0f || enemy.actionDuration <= 0.0f) {
        return;
    }

    const WeaponSpec& spec = GetWeaponSpec(enemy.activeActionWeapon);
    const WeaponActionSpec& action = spec.actions[static_cast<std::size_t>(
        enemy.activeActionIndex == WeaponActionIndex::Action2 ? 1 : 0)];
    const float progress = ActionPosePhase(enemy.actionTimer, enemy.actionDuration, enemy.actionImpactTime, enemy.actionRecovery);
    const float impactProgress = ActionImpactPulse(enemy.actionTimer, enemy.actionDuration, enemy.actionImpactTime);
    const Vec3 origin = MakeVec3(enemy.activeActionOrigin, 0.052f + progress * 0.020f);
    const Vec3 target = MakeVec3(enemy.activeActionTarget, 0.050f + progress * 0.018f);
    const Vec3 dir = Direction3(enemy.activeActionDirection);
    const float range = std::max(action.range, action.radius * 1.35f);
    const float radius = std::max(action.radius, 0.42f);
    const uint32_t materialId = ElementTellMaterial(enemy.activeActionElement);
    const uint32_t visualTag = PackActionVisualTag(enemy.activeActionWeapon, enemy.activeActionElement, static_cast<uint32_t>(enemy.kind));

    auto push = [&](EntityProxyKind kind, Vec3 position, Vec3 direction, float proxyRadius, float scale = 1.0f) {
        PushActionProxy(proxies, kind, position, direction, proxyRadius * scale, materialId, progress, visualTag);
    };

    switch (enemy.kind) {
    case EnemyKind::Brute:
        push(EntityProxyKind::EnemyTellCone, Add(origin, Scale(dir, radius * 0.12f)), dir, radius * 0.62f, 0.42f + impactProgress * 0.12f);
        push(EntityProxyKind::EnemyTellLine, Add(origin, Scale(dir, radius * 0.18f)), dir, radius * 0.42f, 0.34f + progress * 0.08f);
        break;
    case EnemyKind::Caster:
        push(EntityProxyKind::EnemyTellLine, Add(origin, Scale(dir, range * 0.16f)), dir, range * 0.24f, 0.48f);
        push(EntityProxyKind::EnemyTellLine, origin, Direction3(Rotate2(enemy.activeActionDirection, -0.18f)), radius * 0.36f, 0.28f);
        break;
    case EnemyKind::Skirmisher:
        push(EntityProxyKind::EnemyTellLine, Add(origin, Scale(dir, range * 0.14f)), dir, range * 0.22f, 0.42f);
        push(EntityProxyKind::EnemyTellLine, origin, Direction3(Rotate2(enemy.activeActionDirection, 0.22f)), range * 0.12f, 0.32f);
        break;
    case EnemyKind::Bulwark:
        push(EntityProxyKind::EnemyTellCone, origin, dir, range * 0.44f, 0.66f);
        push(EntityProxyKind::EnemyTellLine, Add(origin, Scale(dir, radius * 0.20f)), dir, radius * 0.36f, 0.28f);
        break;
    case EnemyKind::Boss:
        if (enemy.activeActionShape == AttackShape::TargetArea) {
            push(EntityProxyKind::EnemyTellRing, target, dir, radius * 0.62f, 0.42f + impactProgress * 0.12f);
            push(EntityProxyKind::EnemyTellLine, Vec3{target.x, target.y + 0.016f, target.z}, dir, radius * 0.38f, 0.34f);
        } else {
            push(EnemyTellKind(enemy.activeActionShape), Add(origin, Scale(dir, range * 0.10f)), dir, std::max(range * 0.30f, radius), 0.58f);
            push(EntityProxyKind::EnemyTellLine, origin, Direction3(Rotate2(enemy.activeActionDirection, 0.20f)), radius * 0.34f, 0.30f);
        }
        break;
    }
}

void AddEnemyTellProxy(
    std::vector<EntityRTProxy>& proxies,
    const EnemyState& enemy,
    const PlayerState& player) {
    const EnemyAttackIntent intent = EnemyAttackIntentFor(enemy, player);
    constexpr float kTellReadinessThreshold = 0.72f;
    if (!intent.active || !intent.inRange || intent.readiness < kTellReadinessThreshold) {
        return;
    }

    const Vec3 direction{intent.direction.x, 0.0f, intent.direction.y};
    const float urgencyScale = 0.82f + Saturate(intent.readiness) * 0.22f;
    const float range = std::max(intent.range, intent.radius * 1.2f) * urgencyScale;
    const float ringRadius = std::max(intent.radius, 0.36f) * urgencyScale;
    Vec3 position = MakeVec3(enemy.position, 0.035f);
    float radius = range;

    if (intent.shape == AttackShape::TargetArea) {
        position = MakeVec3(player.position, 0.040f);
        radius = ringRadius;
    } else if (intent.shape == AttackShape::Circle || intent.shape == AttackShape::Orbit) {
        radius = ringRadius;
    } else if (intent.shape == AttackShape::Projectile || intent.shape == AttackShape::Wave || intent.shape == AttackShape::Dash) {
        position = Add(position, Scale(direction, range * 0.18f));
        radius = range * 0.30f;
    } else if (intent.shape == AttackShape::Cone || intent.shape == AttackShape::Burst) {
        radius = range * 0.42f;
    }

    proxies.push_back(EntityRTProxy{
        EnemyTellKind(intent.shape),
        position,
        direction,
        radius,
        ElementTellMaterial(intent.element),
        Saturate(intent.readiness),
        PackActionVisualTag(intent.weapon, intent.element, static_cast<uint32_t>(enemy.kind))
    });
}

void AddEnemyReadabilityProxies(std::vector<EntityRTProxy>& proxies, const EnemyState& enemy, float bodyRadius) {
    const Vec3 barDirection{1.0f, 0.0f, 0.0f};
    const float hpRatio = enemy.maxHp > 0.0001f ? Saturate(enemy.hp / enemy.maxHp) : 0.0f;
    const float fullWidth = bodyRadius * 1.78f;
    const float y = bodyRadius * 2.05f + (enemy.kind == EnemyKind::Boss ? bodyRadius * 0.45f : 0.25f);
    const Vec3 center = MakeVec3(enemy.position, y);
    proxies.push_back(EntityRTProxy{
        EntityProxyKind::EnemyHealthBack,
        center,
        barDirection,
        fullWidth,
        kMaterialWall
    });
    proxies.push_back(EntityRTProxy{
        EntityProxyKind::EnemyHealthFill,
        Vec3{center.x - fullWidth * (1.0f - hpRatio) * 0.5f, center.y + 0.025f, center.z},
        barDirection,
        fullWidth * hpRatio,
        kMaterialRoomPulse
    });

    const float shieldRatio = enemy.kind == EnemyKind::Boss
        ? Saturate(enemy.shield / 56.0f)
        : Saturate(enemy.shield / 32.0f);
    if (shieldRatio > 0.001f) {
        proxies.push_back(EntityRTProxy{
            EntityProxyKind::EnemyShieldFill,
            Vec3{center.x - fullWidth * (1.0f - shieldRatio) * 0.5f, center.y + 0.115f, center.z},
            barDirection,
            fullWidth * shieldRatio,
            kMaterialProjectile
        });
    }

    int statusIndex = 0;
    for (const StatusInstance& status : enemy.statuses.slots) {
        if (!status.active || status.kind == StatusKind::None) {
            continue;
        }
        const float xOffset = -fullWidth * 0.48f + static_cast<float>(statusIndex) * bodyRadius * 0.30f;
        proxies.push_back(EntityRTProxy{
            EntityProxyKind::EnemyStatusPip,
            Vec3{center.x + xOffset, center.y + 0.235f, center.z},
            barDirection,
            bodyRadius * 0.085f,
            StatusMaterial(status.kind),
            Saturate(status.intensity * 0.62f + HashUnit(status.instanceId) * 0.38f),
            PackActionVisualTag(WeaponId::Pistol, ElementFromStatusKind(status.kind), static_cast<uint32_t>(status.kind))
        });
        ++statusIndex;
    }
}

}

std::vector<EntityRTProxy> BuildEntityProxies(const CombatSim& sim) {
    std::vector<EntityRTProxy> proxies;
    proxies.reserve(128);

    const PlayerState& player = sim.Player();
    const bool playerActing = player.actionTimer > 0.0f && player.actionDuration > 0.0f;
    const float playerMoveSpeed = std::sqrt(LengthSq(player.velocity));
    const float playerMoveIntensity = Saturate(playerMoveSpeed / 5.8f);
    const float playerIdlePhase = Clamp(
        0.055f + std::sin(player.movePhase + 0.35f) * 0.035f + playerMoveIntensity * 0.13f,
        0.0f,
        0.28f);
    float playerActionPhase = playerActing
        ? ActionPosePhase(player.actionTimer, player.actionDuration, player.actionImpactTime, player.actionRecovery)
        : playerIdlePhase;
    const float playerHitReact = Saturate(player.hitReactTimer / 0.26f);
    if (playerHitReact > 0.0f) {
        playerActionPhase = std::max(playerActionPhase, 0.74f + Smooth01(playerHitReact) * 0.20f);
    }
    const Vec2 playerActionFacing = playerActing ? player.activeActionDirection : player.facing;
    const Vec2 playerVisualPosition = player.position + player.hitReactDirection * (playerHitReact * 0.12f);
    const Vec3 facing{playerActionFacing.x, 0.0f, playerActionFacing.y};
    const int activeSlot = player.activeWeaponSlot >= 0 && player.activeWeaponSlot < kPlayerWeaponSlots ? player.activeWeaponSlot : 0;
    const WeaponId playerVisualWeapon = playerActing ? player.activeActionWeapon : player.weaponSlots[activeSlot].weapon;
    const Element playerVisualElement = playerActing ? player.activeActionElement : player.weaponSlots[activeSlot].element;
    const WeaponActionIndex playerVisualAction = playerActing ? player.activeActionIndex : WeaponActionIndex::Action1;
    const AttackShape playerVisualShape = playerActing ? player.activeActionShape : AttackShape::Cone;
    const uint32_t playerVariantBase = HashMix(
        static_cast<uint32_t>(playerVisualWeapon) * 37u + static_cast<uint32_t>(activeSlot),
        static_cast<uint32_t>(playerVisualElement) * 101u + static_cast<uint32_t>(playerVisualShape) * 17u) & 0x7u;
    const uint32_t playerVariant = playerVariantBase |
        (playerMoveIntensity > 0.08f ? 0x08u : 0u) |
        (playerHitReact > 0.0f ? 0x10u : 0u);
    proxies.push_back(EntityRTProxy{
        EntityProxyKind::PlayerCore,
        MakeVec3(playerVisualPosition, 0.05f + playerMoveIntensity * 0.035f),
        facing,
        0.88f,
        kMaterialPlayerCore,
        playerActionPhase,
        PackPlayerVisualTag(playerVisualWeapon, playerVisualElement, playerVisualAction, playerVisualShape, playerVariant)
    });
    proxies.push_back(EntityRTProxy{
        EntityProxyKind::PlayerBlade,
        MakeVec3(playerVisualPosition + playerActionFacing * (0.58f + playerActionPhase * 0.20f), 0.48f + playerActionPhase * 0.10f),
        facing,
        0.42f,
        kMaterialPlayerBlade,
        playerActionPhase,
        static_cast<uint32_t>(playerVisualWeapon)
    });
    AddPlayerActionProxy(proxies, player);

    uint32_t enemyOrdinal = 0u;
    for (const EnemyState& enemy : sim.Enemies()) {
        if (!enemy.active || enemy.hp <= 0.0f) {
            ++enemyOrdinal;
            continue;
        }
        EntityProxyKind kind = EntityProxyKind::EnemyBrute;
        float radius = 0.75f;
        uint32_t materialId = kMaterialEnemyBrute;
        switch (enemy.kind) {
        case EnemyKind::Brute:
            kind = EntityProxyKind::EnemyBrute;
            radius = 1.02f;
            materialId = kMaterialEnemyBrute;
            break;
        case EnemyKind::Caster:
            kind = EntityProxyKind::EnemyCaster;
            radius = 0.78f;
            materialId = kMaterialEnemyCaster;
            break;
        case EnemyKind::Skirmisher:
            kind = EntityProxyKind::EnemySkirmisher;
            radius = 0.70f;
            materialId = kMaterialEnemySkirmisher;
            break;
        case EnemyKind::Bulwark:
            kind = EntityProxyKind::EnemyBulwark;
            radius = 1.14f;
            materialId = kMaterialEnemyBulwark;
            break;
        case EnemyKind::Boss:
            kind = EntityProxyKind::EnemyBoss;
            radius = 1.78f;
            materialId = kMaterialEnemyBoss;
            break;
        }
        const bool enemyActing = enemy.actionTimer > 0.0f && enemy.actionDuration > 0.0f;
        const float enemyMoveSpeed = std::sqrt(LengthSq(enemy.velocity));
        const float enemyMoveIntensity = Saturate(enemyMoveSpeed / 3.8f);
        const float enemyFloat = enemy.kind == EnemyKind::Caster
            ? 0.070f + std::sin(enemy.movePhase + static_cast<float>(enemyOrdinal) * 1.37f) * 0.030f
            : 0.0f;
        const float enemyIdlePhase = Clamp(
            0.040f + std::sin(enemy.movePhase + static_cast<float>(enemyOrdinal) * 0.73f) * 0.030f + enemyMoveIntensity * 0.11f,
            0.0f,
            0.26f);
        float enemyActionPhase = enemyActing
            ? ActionPosePhase(enemy.actionTimer, enemy.actionDuration, enemy.actionImpactTime, enemy.actionRecovery)
            : enemyIdlePhase;
        const float enemyHitReact = Saturate(enemy.hitReactTimer / 0.24f);
        if (enemyHitReact > 0.0f) {
            enemyActionPhase = std::max(enemyActionPhase, 0.70f + Smooth01(enemyHitReact) * 0.23f);
        }
        const Vec2 enemyVisualPosition = enemy.position + enemy.hitReactDirection * (enemyHitReact * (enemy.kind == EnemyKind::Boss ? 0.06f : 0.10f));
        const Vec2 fallbackFacing = Normalize2(player.position - enemy.position, Vec2{1.0f, 0.0f});
        const Vec2 enemyFacing2 = enemyActing ? Normalize2(enemy.activeActionDirection, fallbackFacing) : fallbackFacing;
        const WeaponId enemyVisualWeapon = enemyActing ? enemy.activeActionWeapon : enemy.weapon.weapon;
        const Element enemyVisualElement = enemyActing ? enemy.activeActionElement : enemy.weapon.element;
        const WeaponActionIndex enemyVisualAction = enemyActing ? enemy.activeActionIndex : WeaponActionIndex::Action1;
        const AttackShape enemyVisualShape = enemyActing ? enemy.activeActionShape : AttackShape::Cone;
        const uint32_t enemyVariant = HashMix(
            enemyOrdinal * 131u + static_cast<uint32_t>(enemy.kind) * 17u + static_cast<uint32_t>(enemy.roomIndex) * 29u,
            static_cast<uint32_t>(enemyVisualWeapon) * 43u + static_cast<uint32_t>(enemyVisualElement) * 97u) & 0x7u;
        const uint32_t visualTag = PackEnemyVisualTag(enemy.kind, enemyVisualWeapon, enemyVisualElement, enemyVisualAction, enemyVisualShape, enemyVariant);
        proxies.push_back(EntityRTProxy{
            kind,
            MakeVec3(enemyVisualPosition, enemyFloat),
            Vec3{enemyFacing2.x, 0.0f, enemyFacing2.y},
            radius,
            materialId,
            enemyActionPhase,
            visualTag
        });
        EnemyState visualEnemy = enemy;
        visualEnemy.position = enemyVisualPosition;
        AddEnemyReadabilityProxies(proxies, visualEnemy, radius);
        AddEnemyActionAccentProxies(proxies, enemy);
        AddEnemyTellProxy(proxies, enemy, player);
        ++enemyOrdinal;
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
            ElementTellMaterial(projectile.element),
            0.65f,
            PackActionVisualTag(projectile.weapon, projectile.element)
        });
        proxies.push_back(EntityRTProxy{
            EntityProxyKind::ProjectileTrail,
            MakeVec3(projectile.position - trailDir * 0.55f, 0.15f),
            dir,
            projectile.radius * 1.35f,
            ElementTellMaterial(projectile.element),
            0.35f,
            PackActionVisualTag(projectile.weapon, projectile.element)
        });
    }

    return proxies;
}

std::vector<EntityRTProxy> BuildVfxProxies(std::span<const RenderVfxPulse> pulses) {
    std::vector<EntityRTProxy> proxies;
    proxies.reserve(pulses.size());

    for (const RenderVfxPulse& pulse : pulses) {
        if (pulse.ttl <= 0.0f || pulse.duration <= 0.0f || pulse.radius <= 0.0f) {
            continue;
        }

        const float progress = 1.0f - Saturate(pulse.ttl / pulse.duration);
        const float expansion = 1.0f + progress * (0.45f + pulse.intensity * 0.20f);
        Vec3 position = pulse.position;
        position.y += progress * 0.08f;

        EntityProxyKind proxyKind = EntityProxyKind::HitSpark;
        uint32_t materialId = kMaterialHitSpark;
        switch (pulse.kind) {
        case RenderVfxKind::HitSpark:
            proxyKind = EntityProxyKind::HitSpark;
            materialId = kMaterialHitSpark;
            break;
        case RenderVfxKind::RoomClearPulse:
            proxyKind = EntityProxyKind::RoomClearPulse;
            materialId = kMaterialRoomPulse;
            break;
        case RenderVfxKind::PortalPulse:
            proxyKind = EntityProxyKind::PortalPulse;
            materialId = kMaterialPortal;
            break;
        case RenderVfxKind::WeaponCone:
            proxyKind = EntityProxyKind::PlayerActionCone;
            materialId = kMaterialPlayerBlade;
            break;
        case RenderVfxKind::WeaponLine:
            proxyKind = EntityProxyKind::PlayerActionLine;
            materialId = kMaterialProjectile;
            break;
        case RenderVfxKind::WeaponRing:
            proxyKind = EntityProxyKind::PlayerActionBurst;
            materialId = kMaterialControl;
            break;
        case RenderVfxKind::WeaponBurst:
            proxyKind = EntityProxyKind::PlayerActionBurst;
            materialId = kMaterialHitSpark;
            break;
        }

        float proxyRadius = pulse.radius * expansion;
        if (pulse.kind == RenderVfxKind::WeaponRing || pulse.kind == RenderVfxKind::WeaponBurst) {
            proxyRadius *= 0.36f;
        }

        proxies.push_back(EntityRTProxy{
            proxyKind,
            position,
            pulse.direction,
            proxyRadius,
            materialId
        });
    }

    return proxies;
}

GeneratedRTGeometry GenerateWorldGeometry(
    const RoomGraph& world,
    const RoomVisualStyle* style,
    int focusRoomIndex,
    const ShotLayout* shotLayout) {
    GeneratedRTGeometry geo{};
    geo.triangles.reserve(static_cast<std::size_t>(world.roomCount) * 620u + static_cast<std::size_t>(world.portalCount) * 10u);
    const bool hasFocus = focusRoomIndex >= 0 && focusRoomIndex < world.roomCount;
    RoomVisualStyle stagedStyle{};
    const RoomVisualStyle* activeStyle = style;
    if (style) {
        stagedStyle = *style;
        if (shotLayout) {
            stagedStyle.moss = Clamp(stagedStyle.moss * (0.82f + shotLayout->foliageDensity * 0.34f), 0.0f, 1.0f);
            stagedStyle.decay = Clamp(stagedStyle.decay + shotLayout->edgeDensity * 0.05f, 0.0f, 1.0f);
            stagedStyle.glow = Clamp(stagedStyle.glow + shotLayout->heroVfxBias * 0.025f, 0.0f, 1.0f);
            stagedStyle.fog = Clamp(stagedStyle.fog + static_cast<float>(shotLayout->foregroundMask) * 0.012f, 0.0f, 1.0f);
        }
        activeStyle = &stagedStyle;
    }

    for (int i = 0; i < world.roomCount; ++i) {
        if (hasFocus && i != focusRoomIndex) {
            continue;
        }
        const Room& room = world.rooms[i];
        if (hasFocus) {
            const float edgeDensity = shotLayout ? shotLayout->edgeDensity : 0.76f;
            const float clearBias = shotLayout ? shotLayout->combatClearRadius : 0.64f;
            AddFloor(
                geo,
                room.center,
                Vec2{
                    room.halfSize.x + 6.60f + edgeDensity * 1.05f - clearBias * 0.26f,
                    room.halfSize.y + 5.70f + edgeDensity * 0.88f - clearBias * 0.20f
                },
                kMaterialCorridor);
        }
        AddFloor(geo, room.center, room.halfSize, kMaterialFloor);
        if (activeStyle) {
            AddStyleFloorDetails(geo, room, *activeStyle, i);
            AddStyleProps(geo, room, *activeStyle, i);
        }
        AddRoomBorders(geo, room);
        AddControlObjectiveMarker(geo, room);
    }

    for (int i = 0; i < world.portalCount; ++i) {
        const Portal& portal = world.portals[i];
        if (hasFocus && portal.a != focusRoomIndex && portal.b != focusRoomIndex) {
            continue;
        }
        if (hasFocus) {
            continue;
        }
        if (portal.a >= 0 && portal.a < world.roomCount && portal.b >= 0 && portal.b < world.roomCount) {
            AddCorridor(geo, world.rooms[portal.a].center, world.rooms[portal.b].center, portal.radius * 0.55f);
        }
        AddPortalMarker(geo, portal);
    }

    return geo;
}

Vec3 FlatDirection(Vec3 direction) {
    const float lenSq = direction.x * direction.x + direction.z * direction.z;
    if (lenSq <= 0.000001f) {
        return Vec3{1.0f, 0.0f, 0.0f};
    }
    const float invLen = 1.0f / std::sqrt(lenSq);
    return Vec3{direction.x * invLen, 0.0f, direction.z * invLen};
}

Vec3 FlatRight(Vec3 direction) {
    const Vec3 dir = FlatDirection(direction);
    return Vec3{-dir.z, 0.0f, dir.x};
}

void AddVerticalPanel(
    GeneratedRTGeometry& geo,
    Vec3 position,
    Vec3 direction,
    float width,
    float height,
    uint32_t materialId,
    float yOffset = 0.0f) {
    const Vec3 right = FlatRight(direction);
    const float halfWidth = std::max(0.035f, width * 0.50f);
    const float topWidth = halfWidth * 0.70f;
    const float h = std::max(0.08f, height);
    const Vec3 base{position.x, position.y + yOffset, position.z};
    const Vec3 top{position.x, position.y + yOffset + h, position.z};
    AddQuad(
        geo,
        Add(base, Scale(right, -halfWidth)),
        Add(base, Scale(right, halfWidth)),
        Add(top, Scale(right, topWidth)),
        Add(top, Scale(right, -topWidth)),
        materialId);
}

void AddCloakFan(GeneratedRTGeometry& geo, Vec3 position, Vec3 direction, float radius, uint32_t materialId) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const Vec3 back = Add(position, Scale(dir, -r * 0.36f));
    AddEllipsoid(geo, Add(back, Vec3{0.0f, r * 0.36f, 0.0f}), Vec3{r * 0.34f, r * 0.34f, r * 0.18f}, materialId, 12, 5);
    AddEllipsoid(geo, Add(back, Add(Scale(right, -r * 0.18f), Vec3{0.0f, r * 0.26f, 0.0f})), Vec3{r * 0.20f, r * 0.30f, r * 0.13f}, materialId, 11, 4);
    AddEllipsoid(geo, Add(back, Add(Scale(right, r * 0.18f), Vec3{0.0f, r * 0.26f, 0.0f})), Vec3{r * 0.20f, r * 0.30f, r * 0.13f}, materialId, 11, 4);
    AddEllipsoid(geo, Add(position, Add(Scale(dir, -r * 0.22f), Vec3{0.0f, r * 0.64f, 0.0f})), Vec3{r * 0.30f, r * 0.13f, r * 0.16f}, materialId, 11, 4);
}

void AddShoulderPlates(GeneratedRTGeometry& geo, Vec3 position, Vec3 direction, float radius, uint32_t materialId) {
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const Vec3 left = Add(position, Add(Scale(right, -r * 0.46f), Vec3{0.0f, r * 0.82f, 0.0f}));
    const Vec3 rightP = Add(position, Add(Scale(right, r * 0.46f), Vec3{0.0f, r * 0.82f, 0.0f}));
    AddEllipsoid(geo, left, Vec3{r * 0.16f, r * 0.080f, r * 0.11f}, materialId, 8, 3);
    AddEllipsoid(geo, rightP, Vec3{r * 0.16f, r * 0.080f, r * 0.11f}, materialId, 8, 3);
}

void AddTaperedCapsuleSegment(
    GeneratedRTGeometry& geo,
    Vec3 start,
    Vec3 end,
    float width,
    uint32_t materialId) {
    const Vec3 delta = Sub(end, start);
    const float lenSq = LengthSq(delta);
    if (lenSq <= 0.0001f) {
        return;
    }

    const Vec3 axis = Scale(delta, 1.0f / std::sqrt(lenSq));
    Vec3 side = Cross(axis, Vec3{0.0f, 1.0f, 0.0f});
    if (LengthSq(side) <= 0.00001f) {
        side = Cross(axis, Vec3{1.0f, 0.0f, 0.0f});
    }
    side = Normalize(side);
    const Vec3 lift = Normalize(Cross(side, axis));
    const float w = std::max(0.008f, width);

    constexpr int kRings = 5;
    constexpr int kSegments = 8;
    std::array<std::array<Vec3, kSegments>, kRings> rings{};
    for (int i = 0; i < kRings; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRings - 1);
        const float belly = 1.0f - std::abs(t - 0.50f) * 0.26f;
        const float endTaper = 0.84f + std::sin(t * 3.14159265f) * 0.18f;
        const Vec3 center = Add(start, Scale(delta, t));
        const float rx = w * (1.62f * belly * endTaper);
        const float ry = w * (1.42f * (0.92f + 0.14f * belly));
        for (int j = 0; j < kSegments; ++j) {
            const float a = static_cast<float>(j) * 6.28318530718f / static_cast<float>(kSegments);
            rings[i][j] = Add(center, Add(Scale(side, std::cos(a) * rx), Scale(lift, std::sin(a) * ry)));
        }
    }

    for (int i = 0; i < kRings - 1; ++i) {
        for (int j = 0; j < kSegments; ++j) {
            const int next = (j + 1) % kSegments;
            AddQuad(geo, rings[i][j], rings[i][next], rings[i + 1][next], rings[i + 1][j], materialId);
        }
    }
    for (int j = 0; j < kSegments; ++j) {
        const int next = (j + 1) % kSegments;
        AddTri(geo, start, rings[0][j], rings[0][next], materialId);
        AddTri(geo, end, rings[kRings - 1][next], rings[kRings - 1][j], materialId);
    }
}

void AddJointLimb(GeneratedRTGeometry& geo, Vec3 start, Vec3 end, float width, uint32_t materialId, uint32_t jointMaterial) {
    const Vec3 delta = Sub(end, start);
    const float len = std::sqrt(LengthSq(delta));
    if (len <= 0.010f) {
        return;
    }

    AddTaperedCapsuleSegment(geo, start, end, width, materialId);
    AddEllipsoid(geo, start, Vec3{width * 0.88f, width * 0.72f, width * 0.88f}, jointMaterial, 8, 3);
    AddEllipsoid(geo, end, Vec3{width * 0.94f, width * 0.70f, width * 0.94f}, jointMaterial, 8, 3);
}

void AddCharacterOuterShell(
    GeneratedRTGeometry& geo,
    Vec3 position,
    Vec3 direction,
    float radius,
    float heightScale,
    uint32_t bodyMaterial,
    uint32_t accentMaterial,
    bool enemy) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const float h = std::max(0.80f, heightScale);
    constexpr int kRings = 4;
    constexpr int kSegments = 8;
    std::array<std::array<Vec3, kSegments>, kRings> rings{};

    for (int i = 0; i < kRings; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRings - 1);
        const float y = position.y + r * ((enemy ? 0.16f : 0.18f) + t * (enemy ? (0.78f * h) : 0.82f));
        const float shoulder = std::sin(t * 3.14159265f);
        const float halfWidth = r * ((enemy ? 0.34f : 0.30f) - t * (enemy ? 0.090f : 0.070f) + shoulder * 0.050f);
        const float frontDepth = r * ((enemy ? 0.21f : 0.19f) - t * 0.030f);
        const float backDepth = r * ((enemy ? 0.24f : 0.23f) + shoulder * 0.030f);
        const Vec3 center = Add(position, Scale(dir, r * (-0.035f + t * 0.030f)));
        for (int j = 0; j < kSegments; ++j) {
            const float a = static_cast<float>(j) * 6.28318530718f / static_cast<float>(kSegments);
            const float cx = std::cos(a);
            const float sz = std::sin(a);
            const float depth = sz >= 0.0f ? backDepth : frontDepth;
            rings[i][j] = Add(
                Vec3{center.x, y, center.z},
                Add(Scale(right, cx * halfWidth), Scale(dir, sz * depth)));
        }
    }

    for (int i = 0; i < kRings - 1; ++i) {
        for (int j = 0; j < kSegments; ++j) {
            const int next = (j + 1) % kSegments;
            AddQuad(geo, rings[i][j], rings[i][next], rings[i + 1][next], rings[i + 1][j], bodyMaterial);
        }
    }
    for (int j = 0; j < kSegments; ++j) {
        const int next = (j + 1) % kSegments;
        AddTri(geo, position, rings[0][next], rings[0][j], bodyMaterial);
        AddTri(geo, Add(position, Vec3{0.0f, r * (enemy ? h + 0.04f : 1.03f), 0.0f}), rings[kRings - 1][j], rings[kRings - 1][next], bodyMaterial);
    }

    const Vec3 waist = Add(position, Add(Scale(dir, r * 0.13f), Vec3{0.0f, r * (enemy ? 0.44f : 0.42f), 0.0f}));
    const Vec3 chest = Add(position, Add(Scale(dir, r * 0.17f), Vec3{0.0f, r * (enemy ? h * 0.64f : 0.70f), 0.0f}));
    AddEllipsoid(geo, waist, Vec3{r * (enemy ? 0.25f : 0.22f), r * 0.026f, r * 0.060f}, accentMaterial, 8, 3);
    AddEllipsoid(geo, chest, Vec3{r * (enemy ? 0.18f : 0.16f), r * 0.024f, r * 0.048f}, accentMaterial, 8, 3);
}

void AddCharacterHeadShell(
    GeneratedRTGeometry& geo,
    Vec3 head,
    Vec3 direction,
    float radius,
    uint32_t bodyMaterial,
    uint32_t accentMaterial,
    uint32_t glowMaterial,
    bool enemy,
    uint32_t role,
    float phase) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const float action = Saturate(phase);
    const float boss = role == static_cast<uint32_t>(EnemyKind::Boss) ? 1.0f : 0.0f;
    const float caster = role == static_cast<uint32_t>(EnemyKind::Caster) ? 1.0f : 0.0f;

    constexpr int kRings = 5;
    constexpr int kSegments = 8;
    std::array<std::array<Vec3, kSegments>, kRings> rings{};
    for (int i = 0; i < kRings; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRings - 1);
        const float crown = std::sin(t * 3.14159265f);
        const float y = head.y + r * (-0.145f + t * (0.46f + boss * 0.12f + caster * 0.06f));
        const float halfW = r * (0.100f + crown * (enemy ? 0.115f : 0.095f) + boss * 0.030f);
        const float frontD = r * (0.105f + crown * 0.040f);
        const float backD = r * (0.135f + crown * (enemy ? 0.065f : 0.050f) + caster * 0.045f);
        const Vec3 center = Add(Vec3{head.x, y, head.z}, Scale(dir, r * (-0.030f + t * 0.045f)));
        for (int j = 0; j < kSegments; ++j) {
            const float a = static_cast<float>(j) * 6.28318530718f / static_cast<float>(kSegments);
            const float cx = std::cos(a);
            const float sz = std::sin(a);
            const float depth = sz >= 0.0f ? backD : frontD;
            rings[i][j] = Add(center, Add(Scale(right, cx * halfW), Scale(dir, sz * depth)));
        }
    }

    for (int i = 0; i < kRings - 1; ++i) {
        for (int j = 0; j < kSegments; ++j) {
            const int next = (j + 1) % kSegments;
            AddQuad(geo, rings[i][j], rings[i][next], rings[i + 1][next], rings[i + 1][j], bodyMaterial);
        }
    }
    for (int j = 0; j < kSegments; ++j) {
        const int next = (j + 1) % kSegments;
        AddTri(geo, Add(head, Vec3{0.0f, -r * 0.16f, 0.0f}), rings[0][next], rings[0][j], bodyMaterial);
        AddTri(geo, Add(head, Vec3{0.0f, r * (0.36f + boss * 0.12f), 0.0f}), rings[kRings - 1][j], rings[kRings - 1][next], bodyMaterial);
    }

    const Vec3 brow = Add(head, Add(Scale(dir, r * 0.175f), Vec3{0.0f, r * (0.060f + boss * 0.020f), 0.0f}));
    AddEllipsoid(geo, brow, Vec3{r * (0.125f + boss * 0.035f), r * 0.036f, r * 0.040f}, accentMaterial, 8, 3);
    AddEllipsoid(geo, Add(brow, Scale(right, r * 0.066f)), Vec3{r * 0.024f, r * 0.018f, r * 0.015f}, glowMaterial, 7, 3);
    AddEllipsoid(geo, Add(brow, Scale(right, -r * 0.066f)), Vec3{r * 0.024f, r * 0.018f, r * 0.015f}, glowMaterial, 7, 3);

    if (enemy || caster > 0.5f || boss > 0.5f) {
        AddPyramid(
            geo,
            Add(head, Add(Scale(dir, -r * (0.020f + caster * 0.045f)), Vec3{0.0f, r * (0.34f + boss * 0.10f + action * 0.020f), 0.0f})),
            r * (0.105f + boss * 0.035f + caster * 0.030f),
            r * (0.23f + boss * 0.10f + caster * 0.09f),
            accentMaterial);
    }
}

void AddFloatingRibbon(
    GeneratedRTGeometry& geo,
    Vec3 anchor,
    Vec3 direction,
    Vec3 side,
    float length,
    float lift,
    float width,
    uint32_t materialId) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatDirection(side);
    const float l = std::max(0.08f, length);
    const float w = std::max(0.004f, width);
    const Vec3 p0 = anchor;
    const Vec3 p1 = Add(anchor, Add(Scale(dir, -l * 0.38f), Add(Scale(right, l * 0.16f), Vec3{0.0f, lift * 0.34f, 0.0f})));
    const Vec3 p2 = Add(anchor, Add(Scale(dir, -l * 0.74f), Add(Scale(right, l * 0.05f), Vec3{0.0f, -lift * 0.18f, 0.0f})));
    for (int i = 0; i < 5; ++i) {
        const float t = static_cast<float>(i) / 4.0f;
        const float inv = 1.0f - t;
        const Vec3 p = Add(Add(Scale(p0, inv * inv), Scale(p1, 2.0f * inv * t)), Scale(p2, t * t));
        const float taper = 1.0f - t * 0.46f;
        AddEllipsoid(geo, p, Vec3{w * (3.8f * taper), w * (2.2f * taper), w * (3.0f * taper)}, materialId, 7, 3);
    }
}

void AddProceduralCapeTails(GeneratedRTGeometry& geo, Vec3 position, Vec3 direction, float radius, uint32_t materialId, uint32_t accentMaterial, float phase) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.14f);
    const float wave = std::sin(Saturate(phase) * 3.14159265f);
    const Vec3 neck = Add(position, Add(Scale(dir, -r * 0.30f), Vec3{0.0f, r * 0.92f, 0.0f}));
    AddFloatingRibbon(geo, Add(neck, Scale(right, -r * 0.20f)), dir, Scale(right, -1.0f), r * (0.80f + wave * 0.16f), r * 0.18f, r * 0.020f, materialId);
    AddFloatingRibbon(geo, Add(neck, Scale(right, r * 0.18f)), dir, right, r * (0.70f + wave * 0.12f), r * 0.14f, r * 0.016f, materialId);
    AddFloatingRibbon(geo, Add(neck, Scale(right, 0.0f)), dir, right, r * (0.62f + wave * 0.10f), r * 0.12f, r * 0.012f, accentMaterial);
}

void AddChestRunes(GeneratedRTGeometry& geo, Vec3 chest, Vec3 direction, float radius, uint32_t materialId) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const Vec3 front = Add(chest, Scale(dir, r * 0.27f));
    AddEllipsoid(geo, Add(front, Vec3{0.0f, r * 0.10f, 0.0f}), Vec3{r * 0.040f, r * 0.030f, r * 0.026f}, materialId, 7, 3);
    AddEllipsoid(geo, Add(front, Add(Scale(right, -r * 0.09f), Vec3{0.0f, -r * 0.02f, 0.0f})), Vec3{r * 0.034f, r * 0.024f, r * 0.022f}, materialId, 7, 3);
    AddEllipsoid(geo, Add(front, Add(Scale(right, r * 0.09f), Vec3{0.0f, -r * 0.02f, 0.0f})), Vec3{r * 0.034f, r * 0.024f, r * 0.022f}, materialId, 7, 3);
}

void AddArmorFacets(
    GeneratedRTGeometry& geo,
    Vec3 position,
    Vec3 direction,
    float radius,
    uint32_t armorMaterial,
    uint32_t trimMaterial,
    float phase) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const float action = Saturate(phase);
    const Vec3 chest = Add(position, Vec3{0.0f, r * 0.72f, 0.0f});
    const Vec3 frontChest = Add(chest, Scale(dir, r * 0.20f));

    AddEllipsoid(geo, Add(frontChest, Add(Scale(right, -r * 0.13f), Vec3{0.0f, r * 0.03f, 0.0f})), Vec3{r * 0.13f, r * 0.075f, r * 0.070f}, armorMaterial, 8, 3);
    AddEllipsoid(geo, Add(frontChest, Add(Scale(right, r * 0.13f), Vec3{0.0f, r * 0.03f, 0.0f})), Vec3{r * 0.13f, r * 0.075f, r * 0.070f}, armorMaterial, 8, 3);
    AddEllipsoid(geo, Add(frontChest, Vec3{0.0f, r * 0.16f, 0.0f}), Vec3{r * 0.18f, r * 0.030f, r * 0.040f}, trimMaterial, 8, 3);

    for (int sideIndex = -1; sideIndex <= 1; sideIndex += 2) {
        const Vec3 side = Scale(right, static_cast<float>(sideIndex));
        const Vec3 shoulder = Add(position, Add(Scale(side, r * 0.42f), Vec3{0.0f, r * (0.83f + action * 0.04f), 0.0f}));
        const Vec3 forearm = Add(position, Add(Scale(side, r * (0.56f + action * 0.06f)), Add(Scale(dir, r * (0.28f + action * 0.08f)), Vec3{0.0f, r * (0.54f + action * 0.04f), 0.0f})));
        const Vec3 knee = Add(position, Add(Scale(side, r * 0.22f), Add(Scale(dir, r * 0.05f), Vec3{0.0f, r * 0.25f, 0.0f})));
        const Vec3 boot = Add(position, Add(Scale(side, r * 0.27f), Add(Scale(dir, r * 0.04f), Vec3{0.0f, r * 0.095f, 0.0f})));

        AddEllipsoid(geo, shoulder, Vec3{r * 0.14f, r * 0.070f, r * 0.10f}, trimMaterial, 8, 3);
        AddEllipsoid(geo, forearm, Vec3{r * 0.10f, r * 0.055f, r * 0.070f}, armorMaterial, 8, 3);
        AddEllipsoid(geo, knee, Vec3{r * 0.072f, r * 0.045f, r * 0.062f}, armorMaterial, 8, 3);
        AddEllipsoid(geo, boot, Vec3{r * 0.11f, r * 0.052f, r * 0.060f}, armorMaterial, 8, 3);
        AddEllipsoid(geo, Add(shoulder, Scale(dir, r * 0.035f)), Vec3{r * 0.040f, r * 0.026f, r * 0.032f}, trimMaterial, 7, 3);
    }
}

void AddElementalParticleField(
    GeneratedRTGeometry& geo,
    Vec3 center,
    Vec3 direction,
    float radius,
    Element element,
    float phase,
    float intensity);

void AddBrokenTellBurst(GeneratedRTGeometry& geo, Vec3 center, Vec3 direction, float radius, float phase, uint32_t materialId, Element element) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.10f);
    const float action = Smooth01(phase);
    constexpr int kBursts = 5;
    for (int i = 0; i < kBursts; ++i) {
        const uint32_t h = HashMix(HashFloat(r), static_cast<uint32_t>(0x4a31u + i * 97u));
        const float lane = static_cast<float>(i) / static_cast<float>(kBursts);
        const float angle = lane * 6.28318530718f + action * 0.82f + (HashUnit(h) - 0.5f) * 0.22f;
        const Vec3 radial{std::cos(angle), 0.0f, std::sin(angle)};
        const Vec3 tangent{-radial.z, 0.0f, radial.x};
        const float distance = r * (0.16f + HashUnit(HashMix(h, 0x27u)) * 0.34f);
        const Vec3 p = Add(center, Add(Scale(radial, distance), Vec3{0.0f, r * (0.020f + HashUnit(HashMix(h, 0x51u)) * 0.055f), 0.0f}));
        const Vec3 flow = Normalize(Add(tangent, Scale(radial, 0.20f + action * 0.26f)));
        AddOrientedPrism(geo, p, flow, r * (0.024f + HashUnit(HashMix(h, 0x81u)) * 0.040f), r * 0.0022f, r * 0.0020f, materialId);
        if ((i & 1) == 0) {
            AddOctahedron(geo, Add(p, Scale(radial, r * 0.014f)), r * (0.0070f + action * 0.0040f), materialId);
        } else {
            AddOctahedron(geo, p, r * 0.0080f, materialId);
        }
    }
    AddElementalParticleField(geo, center, dir, r * 0.28f, element, phase, 0.42f);
}

void AddConeTellShards(GeneratedRTGeometry& geo, Vec3 origin, Vec3 direction, float range, float phase, uint32_t materialId, Element element) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(range, 0.10f);
    const float action = Smooth01(phase);
    constexpr int kRows = 6;
    for (int i = 0; i < kRows; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRows - 1);
        const float side = (t - 0.5f) * 2.0f;
        const float forward = r * (0.16f + t * (0.44f + action * 0.14f));
        const float lateral = side * r * (0.12f + t * 0.26f);
        const Vec3 p = Add(origin, Add(Add(Scale(dir, forward), Scale(right, lateral)), Vec3{0.0f, 0.020f + action * 0.018f, 0.0f}));
        const Vec3 flow = Normalize(Add(dir, Scale(right, side * (0.34f + action * 0.18f))));
        AddOctahedron(geo, p, r * (0.010f + t * 0.004f + action * 0.003f), materialId);
        if ((i & 1) == 0) {
            AddOrientedPrism(geo, Add(p, Scale(flow, r * 0.030f)), flow, r * (0.080f + t * 0.045f), r * 0.0035f, r * 0.0028f, materialId);
        }
    }
    AddElementalParticleField(geo, Add(origin, Scale(dir, r * 0.24f)), dir, r * 0.24f, element, phase, 0.68f);
}

void AddRibCage(GeneratedRTGeometry& geo, Vec3 chest, Vec3 direction, float radius, uint32_t boneMaterial, uint32_t glowMaterial, float openAmount) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    for (int i = 0; i < 4; ++i) {
        const float t = static_cast<float>(i) / 3.0f;
        const float y = r * (0.16f - t * 0.32f);
        const float span = r * (0.24f + t * 0.14f + openAmount * 0.03f);
        const Vec3 center = Add(chest, Add(Scale(dir, r * (0.23f + t * 0.030f)), Vec3{0.0f, y, 0.0f}));
        AddGuideStrip(geo, Add(center, Scale(right, -span * 0.50f)), Normalize(Add(right, Scale(dir, 0.18f))), span, r * 0.010f, boneMaterial);
        AddGuideStrip(geo, Add(center, Scale(right, span * 0.50f)), Normalize(Add(right, Scale(dir, -0.18f))), span, r * 0.010f, boneMaterial);
    }
    AddGuideStrip(geo, Add(chest, Add(Scale(dir, r * 0.30f), Vec3{0.0f, -r * 0.16f, 0.0f})), Vec3{0.0f, 0.0f, 1.0f}, r * 0.18f, r * 0.009f, glowMaterial);
}

void AddSkullMask(GeneratedRTGeometry& geo, Vec3 head, Vec3 direction, float radius, uint32_t skullMaterial, uint32_t eyeMaterial) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    AddEllipsoid(geo, Add(head, Scale(dir, r * 0.030f)), Vec3{r * 0.19f, r * 0.24f, r * 0.18f}, skullMaterial, 9, 4);
    AddCenteredBox(geo, Add(head, Add(Scale(dir, r * 0.22f), Vec3{0.0f, -r * 0.035f, 0.0f})), Vec3{r * 0.080f, r * 0.040f, r * 0.050f}, skullMaterial);
    AddOctahedron(geo, Add(head, Add(Scale(dir, r * 0.22f), Add(Scale(right, -r * 0.070f), Vec3{0.0f, r * 0.045f, 0.0f}))), r * 0.030f, eyeMaterial);
    AddOctahedron(geo, Add(head, Add(Scale(dir, r * 0.22f), Add(Scale(right, r * 0.070f), Vec3{0.0f, r * 0.045f, 0.0f}))), r * 0.030f, eyeMaterial);
    AddGuideStrip(geo, Add(head, Add(Scale(dir, r * 0.24f), Vec3{0.0f, -r * 0.105f, 0.0f})), right, r * 0.16f, r * 0.006f, skullMaterial);
}

void AddHoodCowl(GeneratedRTGeometry& geo, Vec3 head, Vec3 direction, float radius, uint32_t materialId, uint32_t glowMaterial, float phase) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const float pulse = 0.7f + std::sin(Saturate(phase) * 3.14159265f) * 0.3f;
    AddEllipsoid(geo, Add(head, Add(Scale(dir, -r * 0.08f), Vec3{0.0f, r * 0.18f, 0.0f})), Vec3{r * 0.36f, r * 0.30f, r * 0.30f}, materialId, 10, 4);
    AddOrientedPrism(geo, Add(head, Vec3{0.0f, r * 0.34f, 0.0f}), dir, r * 0.24f, r * 0.050f, r * 0.18f, materialId);
    AddVerticalPanel(geo, Add(head, Scale(dir, -r * 0.13f)), dir, r * 0.56f, r * 0.62f, materialId, -r * 0.20f);
    AddEllipsoid(geo, Add(head, Add(Scale(right, -r * 0.18f), Vec3{0.0f, r * (0.06f + pulse * 0.025f), 0.0f})), Vec3{r * 0.082f, r * 0.046f, r * 0.050f}, glowMaterial, 7, 3);
    AddEllipsoid(geo, Add(head, Add(Scale(right, r * 0.18f), Vec3{0.0f, r * (0.06f + pulse * 0.025f), 0.0f})), Vec3{r * 0.082f, r * 0.046f, r * 0.050f}, glowMaterial, 7, 3);
    AddOctahedron(geo, Add(head, Add(Scale(dir, r * 0.19f), Vec3{0.0f, r * (0.05f + pulse * 0.040f), 0.0f})), r * 0.040f, glowMaterial);
    AddEllipsoid(geo, Add(head, Add(Scale(right, -r * 0.14f), Vec3{0.0f, -r * 0.14f, 0.0f})), Vec3{r * 0.060f, r * 0.038f, r * 0.044f}, glowMaterial, 7, 3);
    AddEllipsoid(geo, Add(head, Add(Scale(right, r * 0.14f), Vec3{0.0f, -r * 0.14f, 0.0f})), Vec3{r * 0.060f, r * 0.038f, r * 0.044f}, glowMaterial, 7, 3);
}

void AddAntlerCrown(GeneratedRTGeometry& geo, Vec3 head, Vec3 direction, float radius, uint32_t materialId, uint32_t leafMaterial) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    for (int sideIndex = -1; sideIndex <= 1; sideIndex += 2) {
        const Vec3 side = Scale(right, static_cast<float>(sideIndex));
        const Vec3 root = Add(head, Add(Scale(side, r * 0.18f), Vec3{0.0f, r * 0.18f, 0.0f}));
        AddGuideStrip(geo, Add(root, Add(Scale(side, r * 0.10f), Vec3{0.0f, r * 0.16f, 0.0f})), Normalize(Add(side, Vec3{0.0f, 0.0f, 0.0f})), r * 0.34f, r * 0.010f, materialId);
        AddGuideStrip(geo, Add(root, Add(Scale(side, r * 0.18f), Add(Scale(dir, r * 0.09f), Vec3{0.0f, r * 0.10f, 0.0f}))), Normalize(Add(dir, Scale(side, 0.55f))), r * 0.26f, r * 0.008f, materialId);
        AddOctahedron(geo, Add(root, Add(Scale(side, r * 0.30f), Add(Scale(dir, r * 0.13f), Vec3{0.0f, r * 0.20f, 0.0f}))), r * 0.045f, leafMaterial);
    }
}

void AddSegmentedTail(GeneratedRTGeometry& geo, Vec3 position, Vec3 direction, float radius, uint32_t materialId, uint32_t glowMaterial, float phase) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const float wave = std::sin(Saturate(phase) * 3.14159265f);
    Vec3 prev = Add(position, Add(Scale(dir, -r * 0.34f), Vec3{0.0f, r * 0.42f, 0.0f}));
    for (int i = 0; i < 5; ++i) {
        const float t = static_cast<float>(i + 1);
        const Vec3 next = Add(position, Add(Scale(dir, -r * (0.38f + t * 0.13f)), Add(Scale(right, r * std::sin(t * 1.10f + wave) * 0.12f), Vec3{0.0f, r * (0.42f - t * 0.050f), 0.0f})));
        const Vec3 mid = Scale(Add(prev, next), 0.5f);
        const float taper = 1.0f - static_cast<float>(i) * 0.10f;
        AddEllipsoid(geo, mid, Vec3{r * 0.055f * taper, r * 0.046f * taper, r * 0.055f * taper}, materialId, 7, 3);
        AddEllipsoid(geo, next, Vec3{r * (0.044f * taper), r * (0.038f * taper), r * (0.044f * taper)}, i == 4 ? glowMaterial : materialId, 7, 3);
        prev = next;
    }
}

void AddPlayerHelmetCrest(GeneratedRTGeometry& geo, Vec3 head, Vec3 direction, float radius, uint32_t bodyMaterial, uint32_t accentMaterial, float phase) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const float action = Saturate(phase);
    AddEllipsoid(geo, Add(head, Add(Scale(dir, -r * 0.04f), Vec3{0.0f, r * 0.15f, 0.0f})), Vec3{r * 0.18f, r * 0.13f, r * 0.15f}, bodyMaterial, 8, 3);
    AddEllipsoid(geo, Add(head, Vec3{0.0f, r * 0.27f, 0.0f}), Vec3{r * 0.070f, r * 0.085f, r * 0.075f}, accentMaterial, 7, 3);
    AddEllipsoid(geo, Add(head, Add(Scale(dir, r * 0.18f), Vec3{0.0f, r * (0.08f + action * 0.03f), 0.0f})), Vec3{r * 0.044f, r * 0.036f, r * 0.034f}, accentMaterial, 6, 3);
    AddEllipsoid(geo, Add(head, Add(Scale(right, r * 0.11f), Vec3{0.0f, r * 0.03f, 0.0f})), Vec3{r * 0.036f, r * 0.028f, r * 0.030f}, accentMaterial, 6, 3);
    AddEllipsoid(geo, Add(head, Add(Scale(right, -r * 0.11f), Vec3{0.0f, r * 0.03f, 0.0f})), Vec3{r * 0.036f, r * 0.028f, r * 0.030f}, accentMaterial, 6, 3);
}

void AddElementalParticleField(
    GeneratedRTGeometry& geo,
    Vec3 center,
    Vec3 direction,
    float radius,
    Element element,
    float phase,
    float intensity);

void AddPlayerWeaponRig(GeneratedRTGeometry& geo, Vec3 position, Vec3 direction, float radius, uint32_t materialId, uint32_t accentMaterial, uint32_t weaponClass, float phase) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const float action = Saturate(phase);
    const float swing = std::sin(action * 3.14159265f);
    const float wind = Smooth01(Saturate(action / 0.68f));
    const float follow = Smooth01(Saturate((action - 0.68f) / 0.32f));
    const float contact = Smooth01(1.0f - Saturate(std::abs(action - 0.68f) / 0.20f));
    const Vec3 grip = Add(position, Add(Scale(dir, r * (0.34f + action * 0.16f)), Add(Scale(right, r * (0.22f + swing * 0.16f)), Vec3{0.0f, r * (0.58f + action * 0.10f), 0.0f})));

    if (weaponClass == static_cast<uint32_t>(WeaponId::Hammer)) {
        const Vec3 hammerDir = Normalize(Add(dir, Scale(right, 0.72f - follow * 1.18f)));
        const Vec3 haftCenter = Add(grip, Scale(hammerDir, r * (0.20f + contact * 0.12f)));
        const Vec3 head = Add(grip, Add(Scale(hammerDir, r * (0.72f + contact * 0.18f)), Vec3{0.0f, r * (0.20f - contact * 0.10f), 0.0f}));
        AddOrientedPrism(geo, haftCenter, hammerDir, r * (0.92f + wind * 0.18f), r * 0.024f, r * 0.020f, materialId);
        AddCenteredBox(geo, head, Vec3{r * (0.18f + contact * 0.04f), r * 0.12f, r * (0.16f + contact * 0.03f)}, accentMaterial);
        AddGuideStrip(geo, Add(head, Vec3{0.0f, -r * 0.18f, 0.0f}), FlatRight(hammerDir), r * (0.46f + contact * 0.24f), r * (0.016f + contact * 0.008f), accentMaterial);
    } else if (weaponClass == static_cast<uint32_t>(WeaponId::Spear)) {
        const Vec3 spearDir = Normalize(Add(dir, Scale(right, -0.08f + contact * 0.10f)));
        AddOrientedPrism(geo, Add(grip, Scale(spearDir, r * (0.48f + contact * 0.30f))), spearDir, r * (1.42f + contact * 0.44f), r * 0.013f, r * 0.013f, materialId);
        AddOctahedron(geo, Add(grip, Scale(spearDir, r * (1.18f + contact * 0.46f))), r * (0.070f + contact * 0.026f), accentMaterial);
        AddGuideStrip(geo, Add(grip, Scale(spearDir, r * (0.72f + contact * 0.28f))), spearDir, r * (0.82f + contact * 0.36f), r * 0.006f, accentMaterial);
    } else if (weaponClass == static_cast<uint32_t>(WeaponId::Katana)) {
        const Vec3 bladeDir = Normalize(Add(dir, Scale(right, -0.46f + wind * 1.08f - follow * 0.22f)));
        AddOrientedPrism(geo, Add(grip, Scale(bladeDir, r * 0.42f)), bladeDir, r * (0.88f + contact * 0.26f), r * 0.014f, r * 0.010f, materialId);
        AddBlade(geo, Add(grip, Scale(bladeDir, r * (0.76f + contact * 0.24f))), bladeDir, r * (0.48f + contact * 0.12f), accentMaterial);
        if (contact > 0.01f) {
            AddSlashCrescent(geo, Add(position, Vec3{0.0f, r * 0.40f, 0.0f}), bladeDir, r * (0.70f + contact * 0.24f), contact, accentMaterial);
        }
    } else if (weaponClass >= static_cast<uint32_t>(WeaponId::Pistol) && weaponClass <= static_cast<uint32_t>(WeaponId::Shotgun)) {
        const float barrel = weaponClass == static_cast<uint32_t>(WeaponId::Shotgun) ? 0.78f : (weaponClass == static_cast<uint32_t>(WeaponId::Rifle) ? 0.94f : 0.58f);
        AddOrientedPrism(geo, Add(grip, Scale(dir, r * barrel * 0.48f)), dir, r * barrel, r * 0.026f, r * 0.018f, accentMaterial);
        AddOrientedPrism(geo, Add(grip, Add(Scale(dir, r * 0.12f), Scale(right, -r * 0.08f))), right, r * 0.24f, r * 0.016f, r * 0.014f, materialId);
        AddOctahedron(geo, Add(grip, Scale(dir, r * (barrel + 0.08f))), r * (0.035f + contact * 0.020f), accentMaterial);
        if (contact > 0.01f) {
            AddElementalParticleField(geo, Add(grip, Scale(dir, r * (barrel + 0.18f))), dir, r * (0.18f + contact * 0.16f), ElementFromMaterialId(accentMaterial), contact, 0.72f);
        }
    } else {
        AddOctahedron(geo, Add(grip, Vec3{0.0f, r * (0.12f + action * 0.08f), 0.0f}), r * (0.15f + action * 0.05f), accentMaterial);
        AddBlade(geo, Add(grip, Add(Scale(dir, r * 0.22f), Vec3{0.0f, r * (0.12f + action * 0.06f), 0.0f})), Normalize(Add(dir, Scale(right, 0.24f))), r * (0.24f + action * 0.08f), accentMaterial);
        AddGuideStrip(geo, grip, right, r * 0.42f, r * 0.010f, materialId);
    }
}

void AddElementalParticleField(
    GeneratedRTGeometry& geo,
    Vec3 center,
    Vec3 direction,
    float radius,
    Element element,
    float phase,
    float intensity) {
    if (element == Element::None || radius <= 0.001f || intensity <= 0.001f) {
        return;
    }

    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.10f);
    const float action = Saturate(phase);
    const float amount = std::max(0.10f, intensity);
    const uint32_t materialId = ElementTellMaterial(element);
    uint32_t seed = HashMix(HashFloat(center.x), HashFloat(center.y));
    seed = HashMix(seed, HashFloat(center.z));
    seed = HashMix(seed, static_cast<uint32_t>(element) * 0x45d9f3bu);
    const int count = 8 + static_cast<int>(amount * 7.0f) +
        ((element == Element::Electricity || element == Element::Fire || element == Element::Air) ? 4 : 0);

    for (int i = 0; i < count; ++i) {
        const uint32_t h0 = HashMix(seed, static_cast<uint32_t>(0x9e37u + i * 71u));
        const uint32_t h1 = HashMix(seed, static_cast<uint32_t>(0x51edu + i * 47u));
        const uint32_t h2 = HashMix(seed, static_cast<uint32_t>(0xbabdu + i * 29u));
        const uint32_t h3 = HashMix(seed, static_cast<uint32_t>(0x7127u + i * 89u));
        const uint32_t h4 = HashMix(seed, static_cast<uint32_t>(0xd1b5u + i * 37u));
        const float lane = HashUnit(h0) * 2.0f - 1.0f;
        const float drift = HashUnit(h1) * 2.0f - 1.0f;
        const float forward = (HashUnit(h2) * 2.0f - 1.0f) * r * (0.38f + amount * 0.16f) + action * r * 0.12f;
        const float cross = lane * r * (0.24f + HashUnit(h3) * (0.26f + amount * 0.06f));
        const float height = r * (0.030f + HashUnit(h4) * (0.24f + amount * 0.07f));
        const Vec3 flowDir = Normalize(Add(dir, Scale(right, lane * 0.46f + std::sin(action * 3.7f + static_cast<float>(i)) * 0.10f)));
        const Vec3 p = Add(center, Add(Add(Scale(dir, forward), Scale(right, cross)), Vec3{0.0f, height, 0.0f}));
        const float s = r * (0.0060f + HashUnit(HashMix(h2, 0x21u)) * 0.0125f) * (0.82f + amount * 0.22f);

        switch (element) {
        case Element::Water: {
            const Vec3 drop{
                p.x + drift * r * 0.035f,
                std::max(center.y + s * 0.80f, p.y - r * (0.06f + action * 0.10f)),
                p.z + lane * r * 0.020f
            };
            AddEllipsoid(geo, drop, Vec3{s * 0.58f, s * (1.08f + HashUnit(h3) * 0.38f), s * 0.58f}, materialId, 6, 3);
            if ((i % 3) == 0) {
                AddGuideStrip(
                    geo,
                    Add(drop, Scale(flowDir, s * 1.4f)),
                    Normalize(Add(flowDir, Scale(right, drift * 0.18f))),
                    r * (0.070f + HashUnit(h4) * 0.070f),
                    s * 0.070f,
                    materialId);
            }
            break;
        }
        case Element::Fire: {
            const Vec3 ember{p.x, p.y + r * (0.05f + action * 0.18f + HashUnit(h3) * 0.16f), p.z};
            AddEllipsoid(geo, ember, Vec3{s * 0.38f, s * (0.82f + action * 0.34f), s * 0.34f}, materialId, 6, 3);
            if ((i & 1) == 0) {
                AddOrientedPrism(
                    geo,
                    Add(ember, Scale(flowDir, s * 1.2f)),
                    Normalize(Add(flowDir, Scale(right, drift * 0.32f))),
                    s * (1.15f + action * 0.42f),
                    s * 0.055f,
                    s * 0.050f,
                    materialId);
            } else {
                AddOctahedron(geo, ember, s * 0.40f, materialId);
            }
            break;
        }
        case Element::Stone: {
            const Vec3 chip{p.x, center.y + s * (0.60f + HashUnit(h3) * 1.8f), p.z};
            AddCenteredBox(geo, chip, Vec3{s * (0.50f + HashUnit(h0) * 0.35f), s * (0.34f + HashUnit(h1) * 0.24f), s * (0.44f + HashUnit(h2) * 0.30f)}, materialId);
            if ((i % 3) == 0) {
                AddOctahedron(geo, Add(chip, Vec3{0.0f, s * 0.45f, 0.0f}), s * 0.46f, materialId);
            }
            break;
        }
        case Element::Electricity: {
            const Vec3 zigA = Normalize(Add(flowDir, Scale(right, drift * 0.94f)));
            const Vec3 zigB = Normalize(Add(flowDir, Scale(right, -lane * 0.88f)));
            const float lenA = r * (0.070f + HashUnit(h3) * 0.095f);
            const float lenB = r * (0.050f + HashUnit(h4) * 0.085f);
            const Vec3 mid = Add(p, Add(Scale(zigA, lenA * 0.42f), Vec3{0.0f, s * (0.5f + action), 0.0f}));
            AddGuideStrip(geo, p, zigA, lenA, s * 0.055f, materialId);
            AddGuideStrip(geo, mid, zigB, lenB, s * 0.050f, materialId);
            AddOctahedron(geo, Add(mid, Scale(zigB, lenB * 0.34f)), s * 0.48f, materialId);
            break;
        }
        case Element::Ice: {
            const Vec3 shard = Add(p, Vec3{0.0f, r * (0.03f + HashUnit(h3) * 0.10f), 0.0f});
            AddOctahedron(geo, shard, s * (0.48f + HashUnit(h0) * 0.22f), materialId);
            if ((i & 1) == 0) {
                AddOrientedPrism(
                    geo,
                    Add(shard, Scale(flowDir, s * 1.5f)),
                    Normalize(Add(flowDir, Scale(right, lane * 0.28f))),
                    s * (0.92f + HashUnit(h4) * 0.42f),
                    s * 0.045f,
                    s * 0.030f,
                    materialId);
            } else {
                AddOctahedron(geo, shard, s * 0.38f, materialId);
            }
            break;
        }
        case Element::Air: {
            const Vec3 wisp = Add(p, Vec3{0.0f, r * (0.02f + action * 0.08f), 0.0f});
            AddGuideStrip(
                geo,
                wisp,
                Normalize(Add(flowDir, Scale(right, std::sin(action * 4.1f + static_cast<float>(i)) * 0.50f))),
                r * (0.095f + HashUnit(h3) * 0.145f + action * 0.035f),
                s * 0.045f,
                materialId);
            if ((i % 4) == 0) {
                AddEllipsoid(geo, Add(wisp, Scale(right, drift * s * 1.8f)), Vec3{s * 0.42f, s * 0.30f, s * 0.42f}, materialId, 6, 3);
            }
            break;
        }
        case Element::None:
            break;
        }
    }
}

Vec3 RotateFlatDirection(Vec3 direction, float radians) {
    const Vec3 dir = FlatDirection(direction);
    const Vec2 rotated = Rotate2(Vec2{dir.x, dir.z}, radians);
    return Vec3{rotated.x, 0.0f, rotated.y};
}

Element ResolveActionElement(uint32_t visualTag, uint32_t materialId) {
    const Element tagged = ActionVisualTagElement(visualTag);
    return tagged != Element::None ? tagged : ElementFromMaterialId(materialId);
}

void AddWeaponActionParticleField(
    GeneratedRTGeometry& geo,
    Vec3 origin,
    Vec3 direction,
    float radius,
    float phase,
    uint32_t materialId,
    uint32_t visualTag,
    EntityProxyKind proxyKind) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const float action = Saturate(phase);
    const float contact = Smooth01(1.0f - Saturate(std::abs(action - 0.68f) / 0.23f));
    const WeaponId weapon = ActionVisualTagWeapon(visualTag);
    const Element element = ResolveActionElement(visualTag, materialId);
    const uint32_t particleMaterial = element != Element::None ? ElementTellMaterial(element) : materialId;
    const uint32_t seedBase = HashMix(HashFloat(origin.x + r), HashMix(HashFloat(origin.z - r), visualTag ^ static_cast<uint32_t>(proxyKind)));

    auto mote = [&](Vec3 p, float size, uint32_t mat = 0u) {
        AddOctahedron(geo, p, std::max(0.004f, size), mat != 0u ? mat : particleMaterial);
    };

    auto shard = [&](Vec3 p, Vec3 flow, float length, float width, uint32_t mat = 0u) {
        AddOrientedPrism(
            geo,
            p,
            flow,
            std::max(0.018f, length),
            std::max(0.0016f, width),
            std::max(0.0014f, width * 0.76f),
            mat != 0u ? mat : particleMaterial);
    };

    switch (weapon) {
    case WeaponId::Katana: {
        const float arc = proxyKind == EntityProxyKind::PlayerActionLine ? 0.42f : 1.05f;
        const int count = 26;
        for (int i = 0; i < count; ++i) {
            const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x410u + i * 53u));
            const float t = static_cast<float>(i) / static_cast<float>(count - 1);
            const float a = -arc + arc * 2.0f * t + (HashUnit(HashMix(h, 0x11u)) - 0.5f) * 0.12f;
            const Vec3 flow = RotateFlatDirection(dir, a);
            const Vec3 tangent = RotateFlatDirection(dir, a + 1.5707963f);
            const float dist = r * (0.18f + t * (0.70f + contact * 0.18f));
            const Vec3 p = Add(origin, Add(Scale(flow, dist), Vec3{0.0f, r * (0.026f + contact * 0.030f + HashUnit(HashMix(h, 0x21u)) * 0.045f), 0.0f}));
            shard(p, Normalize(Add(tangent, Scale(flow, 0.18f))), r * (0.030f + contact * 0.070f + HashUnit(HashMix(h, 0x31u)) * 0.040f), r * (0.0020f + contact * 0.0025f));
            if ((i & 3) != 0) {
                mote(Add(p, Scale(flow, r * 0.018f)), r * (0.0045f + contact * 0.0055f + HashUnit(HashMix(h, 0x43u)) * 0.0040f));
            }
        }
        AddElementalParticleField(geo, Add(origin, Scale(dir, r * 0.36f)), dir, r * 0.32f, element, action, 1.10f + contact * 0.55f);
        break;
    }
    case WeaponId::Hammer: {
        const int count = 20;
        for (int i = 0; i < count; ++i) {
            const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x710u + i * 67u));
            const float a = (HashUnit(HashMix(h, 0x11u)) - 0.5f) * (1.05f + contact * 0.38f);
            const Vec3 flow = RotateFlatDirection(dir, a);
            const Vec3 tangent = Normalize(Add(Scale(right, (HashUnit(HashMix(h, 0x51u)) - 0.5f) * 0.8f), Scale(dir, 0.22f)));
            const float dist = r * (0.08f + HashUnit(HashMix(h, 0x21u)) * (0.48f + contact * 0.18f));
            const float lift = r * (0.012f + HashUnit(HashMix(h, 0x31u)) * (0.055f + contact * 0.055f));
            const Vec3 p = Add(origin, Add(Scale(flow, dist), Vec3{0.0f, lift, 0.0f}));
            if ((i % 3) == 0) {
                AddEllipsoid(
                    geo,
                    p,
                    Vec3{r * (0.014f + HashUnit(h) * 0.012f), r * (0.009f + contact * 0.006f), r * (0.012f + HashUnit(HashMix(h, 0x41u)) * 0.010f)},
                    element == Element::Stone ? kMaterialWall : particleMaterial,
                    6,
                    3);
            } else {
                shard(p, Normalize(Add(tangent, Scale(flow, 0.35f))), r * (0.026f + contact * 0.050f), r * 0.0025f, particleMaterial);
            }
        }
        AddElementalParticleField(geo, Add(origin, Scale(dir, r * 0.18f)), dir, r * 0.30f, element, action, 0.94f + contact * 0.62f);
        break;
    }
    case WeaponId::Spear: {
        const int count = 24;
        for (int i = 0; i < count; ++i) {
            const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x810u + i * 47u));
            const float t = static_cast<float>(i) / static_cast<float>(count - 1);
            const float side = (HashUnit(HashMix(h, 0x11u)) - 0.5f) * r * (0.13f + contact * 0.10f);
            const Vec3 p = Add(origin, Add(Add(Scale(dir, r * (0.10f + t * (0.80f + contact * 0.32f))), Scale(right, side)), Vec3{0.0f, r * (0.040f + t * 0.035f), 0.0f}));
            shard(p, Normalize(Add(dir, Scale(right, side * 0.32f))), r * (0.045f + contact * 0.062f + t * 0.025f), r * 0.0022f);
            if ((i % 4) == 0) {
                mote(p, r * (0.0060f + contact * 0.0045f));
            }
        }
        AddElementalParticleField(geo, Add(origin, Scale(dir, r * 0.42f)), dir, r * 0.24f, element, action, 0.92f + contact * 0.42f);
        break;
    }
    case WeaponId::Pistol:
    case WeaponId::Rifle:
    case WeaponId::Machinegun:
    case WeaponId::Shotgun: {
        const bool shotgun = weapon == WeaponId::Shotgun;
        const bool rifle = weapon == WeaponId::Rifle;
        const int count = shotgun ? 28 : (weapon == WeaponId::Machinegun ? 24 : 16);
        const float spread = shotgun ? 0.48f : (rifle ? 0.075f : 0.16f);
        for (int i = 0; i < count; ++i) {
            const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0x910u + i * 61u));
            const float t = HashUnit(HashMix(h, 0x21u));
            const float angle = (HashUnit(HashMix(h, 0x11u)) - 0.5f) * spread * (1.0f + contact * 0.42f);
            const Vec3 flow = RotateFlatDirection(dir, angle);
            const float travel = r * (0.12f + t * (0.70f + contact * 0.30f));
            const Vec3 p = Add(origin, Add(Scale(flow, travel), Vec3{0.0f, r * (0.060f + HashUnit(HashMix(h, 0x31u)) * 0.050f), 0.0f}));
            shard(p, flow, r * (0.030f + contact * 0.055f + (rifle ? 0.040f : 0.0f)), r * 0.0024f);
            if (shotgun || (i & 1) == 0) {
                mote(Add(p, Scale(flow, r * 0.020f)), r * (0.0050f + contact * 0.0050f));
            }
        }
        AddElementalParticleField(geo, Add(origin, Scale(dir, r * 0.22f)), dir, r * (shotgun ? 0.32f : 0.22f), element, action, 0.98f + contact * 0.48f);
        break;
    }
    case WeaponId::Staff:
    case WeaponId::Scepter:
    case WeaponId::Gloves: {
        const int count = weapon == WeaponId::Gloves ? 24 : 18;
        for (int i = 0; i < count; ++i) {
            const uint32_t h = HashMix(seedBase, static_cast<uint32_t>(0xa10u + i * 43u));
            const float t = static_cast<float>(i) / static_cast<float>(count);
            const float lane = (HashUnit(HashMix(h, 0x21u)) - 0.5f) * r * (0.30f + contact * 0.12f);
            const float advance = r * (0.08f + t * (0.58f + contact * 0.16f));
            const float wave = std::sin(t * 5.4f + action * 2.2f + HashUnit(HashMix(h, 0x11u))) * r * 0.055f;
            const Vec3 tangent = Normalize(Add(dir, Scale(right, lane >= 0.0f ? 0.36f : -0.36f)));
            const Vec3 p = Add(origin, Add(Add(Scale(dir, advance), Scale(right, lane + wave)), Vec3{0.0f, r * (0.08f + t * 0.22f + contact * 0.06f), 0.0f}));
            if ((i % 3) == 0) {
                mote(p, r * (0.010f + contact * 0.006f + HashUnit(HashMix(h, 0x41u)) * 0.005f));
            } else {
                shard(p, tangent, r * (0.030f + contact * 0.040f), r * 0.0021f);
            }
        }
        AddElementalParticleField(geo, Add(origin, Add(Scale(dir, r * 0.18f), Vec3{0.0f, r * 0.12f, 0.0f})), dir, r * 0.28f, element, action, 1.02f + contact * 0.44f);
        break;
    }
    case WeaponId::Count:
        AddElementalParticleField(geo, origin, dir, r * 0.42f, element, action, 1.0f + contact * 0.50f);
        break;
    }
}

void AddEnemyWeaponRig(
    GeneratedRTGeometry& geo,
    Vec3 position,
    Vec3 direction,
    float radius,
    uint32_t bodyMaterial,
    uint32_t accentMaterial,
    uint32_t role,
    uint32_t weaponClass,
    float phase) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const float action = Saturate(phase);
    const float swing = std::sin(action * 3.14159265f);
    const Vec3 hand = Add(position, Add(Scale(dir, r * (0.30f + action * 0.14f)), Add(Scale(right, r * (0.48f + swing * 0.12f)), Vec3{0.0f, r * (0.66f + action * 0.12f), 0.0f})));

    if (role == static_cast<uint32_t>(EnemyKind::Caster) || weaponClass >= static_cast<uint32_t>(WeaponId::Staff)) {
        AddOrientedPrism(geo, Add(hand, Add(Scale(dir, r * 0.14f), Vec3{0.0f, r * 0.10f, 0.0f})), Normalize(Add(dir, Scale(right, -0.12f))), r * (0.98f + action * 0.16f), r * 0.015f, r * 0.014f, bodyMaterial);
        AddEllipsoid(geo, Add(hand, Add(Scale(dir, r * 0.58f), Vec3{0.0f, r * (0.38f + action * 0.10f), 0.0f})), Vec3{r * (0.13f + action * 0.04f), r * (0.11f + action * 0.03f), r * (0.12f + action * 0.03f)}, accentMaterial, 8, 3);
        AddBrokenTellBurst(geo, Vec3{hand.x, hand.y + r * 0.32f, hand.z}, dir, r * (0.33f + action * 0.10f), phase, accentMaterial, ElementFromMaterialId(accentMaterial));
        return;
    }

    if (role == static_cast<uint32_t>(EnemyKind::Bulwark)) {
        AddVerticalPanel(geo, Add(position, Scale(dir, r * (0.56f + action * 0.14f))), dir, r * 0.88f, r * (1.10f + action * 0.16f), accentMaterial, r * 0.10f);
        AddEllipsoid(geo, Add(position, Add(Scale(dir, r * 0.74f), Vec3{0.0f, r * 0.74f, 0.0f})), Vec3{r * 0.34f, r * 0.070f, r * 0.070f}, bodyMaterial, 8, 3);
        AddEllipsoid(geo, Add(position, Add(Scale(dir, r * 0.78f), Vec3{0.0f, r * 1.12f, 0.0f})), Vec3{r * 0.080f, r * 0.070f, r * 0.070f}, accentMaterial, 7, 3);
        return;
    }

    if (role == static_cast<uint32_t>(EnemyKind::Skirmisher)) {
        AddBlade(geo, Add(hand, Scale(right, r * 0.12f)), Normalize(Add(dir, Scale(right, 0.74f))), r * (0.54f + action * 0.12f), accentMaterial);
        AddBlade(geo, Add(position, Add(Scale(right, -r * 0.46f), Vec3{0.0f, r * (0.62f + action * 0.08f), 0.0f})), Normalize(Add(dir, Scale(right, -0.68f))), r * (0.46f + action * 0.10f), accentMaterial);
        AddSegmentedTail(geo, position, dir, r * 0.82f, bodyMaterial, accentMaterial, phase);
        return;
    }

    if (role == static_cast<uint32_t>(EnemyKind::Boss)) {
        AddOrientedPrism(geo, Add(hand, Scale(dir, r * 0.30f)), Normalize(Add(dir, Scale(right, 0.22f))), r * (1.14f + action * 0.28f), r * 0.024f, r * 0.022f, bodyMaterial);
        AddBlade(geo, Add(hand, Scale(dir, r * (0.88f + action * 0.20f))), Normalize(Add(dir, Scale(right, 0.20f))), r * (0.72f + action * 0.16f), accentMaterial);
        AddBrokenTellBurst(geo, Add(position, Vec3{0.0f, r * (0.42f + action * 0.08f), 0.0f}), dir, r * (1.08f + action * 0.24f), phase, accentMaterial, ElementFromMaterialId(accentMaterial));
        return;
    }

    AddOrientedPrism(geo, Add(hand, Scale(right, r * 0.16f)), Normalize(Add(right, Scale(dir, 0.18f))), r * (0.82f + action * 0.24f), r * 0.017f, r * 0.016f, bodyMaterial);
    AddBlade(geo, Add(hand, Scale(right, r * (0.48f + action * 0.16f))), right, r * (0.52f + action * 0.14f), accentMaterial);
}

void AddEnemyRoleDetails(
    GeneratedRTGeometry& geo,
    Vec3 position,
    Vec3 direction,
    float radius,
    float heightScale,
    uint32_t bodyMaterial,
    uint32_t accentMaterial,
    uint32_t role,
    uint32_t weaponClass,
    float phase) {
    const Vec3 dir = FlatDirection(direction);
    const Vec3 right = FlatRight(direction);
    const float r = std::max(radius, 0.12f);
    const float h = std::max(0.70f, heightScale);
    const float action = Saturate(phase);
    const Vec3 chest{position.x, position.y + r * (h * 0.62f), position.z};
    const Vec3 head{position.x, position.y + r * (h + 0.30f), position.z};

    if (role == static_cast<uint32_t>(EnemyKind::Brute)) {
        AddSkullMask(geo, head, dir, r * 1.04f, kMaterialEnemyBulwark, accentMaterial);
        AddRibCage(geo, chest, dir, r, kMaterialEnemyBulwark, accentMaterial, action);
        AddOrientedPrism(geo, Add(head, Add(Scale(right, r * 0.30f), Vec3{0.0f, r * 0.22f, 0.0f})), Normalize(Add(right, Scale(dir, 0.18f))), r * 0.34f, r * 0.025f, r * 0.035f, accentMaterial);
        AddOrientedPrism(geo, Add(head, Add(Scale(right, -r * 0.30f), Vec3{0.0f, r * 0.22f, 0.0f})), Normalize(Add(Scale(right, -1.0f), Scale(dir, 0.18f))), r * 0.34f, r * 0.025f, r * 0.035f, accentMaterial);
        AddEllipsoid(geo, Add(position, Add(Scale(right, r * 0.32f), Vec3{0.0f, r * 0.98f, 0.0f})), Vec3{r * 0.14f, r * 0.070f, r * 0.16f}, bodyMaterial, 8, 3);
        AddEllipsoid(geo, Add(position, Add(Scale(right, -r * 0.32f), Vec3{0.0f, r * 0.96f, 0.0f})), Vec3{r * 0.14f, r * 0.070f, r * 0.16f}, bodyMaterial, 8, 3);
    } else if (role == static_cast<uint32_t>(EnemyKind::Caster)) {
        AddHoodCowl(geo, head, dir, r * 1.08f, bodyMaterial, accentMaterial, phase);
        AddChestRunes(geo, chest, dir, r * 1.05f, accentMaterial);
        AddProceduralCapeTails(geo, position, dir, r * 1.08f, bodyMaterial, accentMaterial, phase);
        for (int i = 0; i < 3; ++i) {
            const float a = static_cast<float>(i) * 2.0943951f + phase * 1.8f;
            const Vec3 orb = Add(head, Add(Scale(right, std::cos(a) * r * 0.42f), Add(Scale(dir, std::sin(a) * r * 0.24f), Vec3{0.0f, r * (0.34f + 0.05f * std::sin(a)), 0.0f})));
            AddEllipsoid(geo, orb, Vec3{r * 0.065f, r * 0.055f, r * 0.060f}, accentMaterial, 7, 3);
        }
    } else if (role == static_cast<uint32_t>(EnemyKind::Skirmisher)) {
        AddAntlerCrown(geo, head, dir, r * 1.06f, bodyMaterial, accentMaterial);
        AddChestRunes(geo, chest, dir, r * 0.82f, accentMaterial);
        AddProceduralCapeTails(geo, position, dir, r * 0.82f, bodyMaterial, accentMaterial, phase);
        AddEllipsoid(geo, Add(position, Add(Scale(dir, r * 0.18f), Vec3{0.0f, r * 1.05f, 0.0f})), Vec3{r * 0.070f, r * 0.060f, r * 0.060f}, accentMaterial, 7, 3);
    } else if (role == static_cast<uint32_t>(EnemyKind::Bulwark)) {
        AddSkullMask(geo, head, dir, r * 1.10f, kMaterialEnemyBulwark, accentMaterial);
        AddRibCage(geo, chest, dir, r * 1.02f, kMaterialEnemyBulwark, accentMaterial, action);
        AddEllipsoid(geo, Add(chest, Add(Scale(dir, r * 0.12f), Vec3{0.0f, r * 0.12f, 0.0f})), Vec3{r * 0.27f, r * 0.12f, r * 0.13f}, bodyMaterial, 8, 3);
        AddEllipsoid(geo, Add(position, Add(Scale(right, r * 0.42f), Vec3{0.0f, r * 0.84f, 0.0f})), Vec3{r * 0.17f, r * 0.10f, r * 0.18f}, bodyMaterial, 8, 3);
        AddEllipsoid(geo, Add(position, Add(Scale(right, -r * 0.42f), Vec3{0.0f, r * 0.84f, 0.0f})), Vec3{r * 0.17f, r * 0.10f, r * 0.18f}, bodyMaterial, 8, 3);
    } else if (role == static_cast<uint32_t>(EnemyKind::Boss)) {
        AddHoodCowl(geo, head, dir, r * 1.16f, bodyMaterial, accentMaterial, phase);
        AddSkullMask(geo, head, dir, r * 1.12f, bodyMaterial, accentMaterial);
        AddRibCage(geo, chest, dir, r * 1.08f, kMaterialEnemyBulwark, accentMaterial, action);
        AddChestRunes(geo, chest, dir, r * 1.20f, accentMaterial);
        AddProceduralCapeTails(geo, position, dir, r * 1.28f, bodyMaterial, accentMaterial, phase);
        AddEllipsoid(geo, Add(head, Vec3{0.0f, r * 0.42f, 0.0f}), Vec3{r * 0.30f, r * 0.28f, r * 0.26f}, accentMaterial, 10, 4);
        AddEllipsoid(geo, Add(head, Vec3{0.0f, r * 0.72f, 0.0f}), Vec3{r * 0.12f, r * 0.18f, r * 0.11f}, accentMaterial, 8, 3);
        AddEllipsoid(geo, Add(chest, Add(Scale(right, r * 0.70f), Vec3{0.0f, r * 0.32f, 0.0f})), Vec3{r * 0.18f, r * 0.30f, r * 0.10f}, bodyMaterial, 8, 3);
        AddEllipsoid(geo, Add(chest, Add(Scale(right, -r * 0.70f), Vec3{0.0f, r * 0.30f, 0.0f})), Vec3{r * 0.18f, r * 0.30f, r * 0.10f}, bodyMaterial, 8, 3);
    }

    AddEnemyWeaponRig(geo, position, dir, r, bodyMaterial, accentMaterial, role, weaponClass, phase);
}

struct CharacterKit {
    float shoulderScale = 1.0f;
    float hipScale = 1.0f;
    float limbScale = 1.0f;
    float torsoScale = 1.0f;
    float headScale = 1.0f;
    float footScale = 1.0f;
    float cloakScale = 1.0f;
    float armorScale = 1.0f;
    float ornamentScale = 1.0f;
    float maskScale = 1.0f;
    float clothScale = 1.0f;
    float glowScale = 1.0f;
    float silhouetteScale = 1.0f;
    uint32_t bodyMaterial = kMaterialPlayerCore;
    uint32_t accentMaterial = kMaterialPlayerBlade;
    uint32_t patternMaterial = kMaterialHitSpark;
    uint32_t variant = 0u;
};

struct CharacterRigPose {
    Vec3 dir{};
    Vec3 right{};
    Vec3 pelvis{};
    Vec3 chest{};
    Vec3 head{};
    Vec3 leftHip{};
    Vec3 rightHip{};
    Vec3 leftFoot{};
    Vec3 rightFoot{};
    Vec3 leftShoulder{};
    Vec3 rightShoulder{};
    Vec3 leftHand{};
    Vec3 rightHand{};
    float action = 0.0f;
    float swing = 0.0f;
    float wind = 0.0f;
    float follow = 0.0f;
    float contact = 0.0f;
};

float VariantSigned(uint32_t variant, uint32_t salt) {
    return HashUnit(HashMix(variant * 0x9e3779b9u, salt)) * 2.0f - 1.0f;
}

CharacterKit BuildCharacterKit(
    bool enemy,
    uint32_t role,
    uint32_t weaponClass,
    Element element,
    uint32_t bodyMaterial,
    uint32_t accentMaterial,
    uint32_t variant) {
    CharacterKit kit{};
    kit.variant = variant & 0xffu;
    kit.bodyMaterial = bodyMaterial;
    kit.accentMaterial = accentMaterial;
    kit.patternMaterial = element != Element::None ? ElementTellMaterial(element) : accentMaterial;

    kit.shoulderScale = 1.0f + VariantSigned(variant, 0x21u) * 0.055f;
    kit.hipScale = 1.0f + VariantSigned(variant, 0x31u) * 0.045f;
    kit.limbScale = 1.0f + VariantSigned(variant, 0x41u) * 0.060f;
    kit.torsoScale = 1.0f + VariantSigned(variant, 0x51u) * 0.050f;
    kit.headScale = 1.0f + VariantSigned(variant, 0x61u) * 0.040f;
    kit.footScale = 1.0f + VariantSigned(variant, 0x71u) * 0.055f;
    kit.cloakScale = 1.0f + VariantSigned(variant, 0x81u) * 0.080f;
    kit.armorScale = 1.0f + VariantSigned(variant, 0x91u) * 0.070f;
    kit.ornamentScale = 1.0f + VariantSigned(variant, 0xa1u) * 0.110f;
    kit.maskScale = 1.0f + VariantSigned(variant, 0xb1u) * 0.070f;
    kit.clothScale = 1.0f + VariantSigned(variant, 0xc1u) * 0.090f;
    kit.glowScale = 1.0f + VariantSigned(variant, 0xd1u) * 0.120f;
    kit.silhouetteScale = 1.0f + VariantSigned(variant, 0xe1u) * 0.075f;

    if (enemy) {
        if (role == static_cast<uint32_t>(EnemyKind::Brute)) {
            kit.shoulderScale *= 1.18f;
            kit.hipScale *= 1.08f;
            kit.limbScale *= 1.10f;
            kit.torsoScale *= 1.16f;
            kit.headScale *= 1.04f;
            kit.maskScale *= 1.14f;
            kit.silhouetteScale *= 1.16f;
            kit.patternMaterial = kMaterialEnemyBulwark;
        } else if (role == static_cast<uint32_t>(EnemyKind::Caster)) {
            kit.shoulderScale *= 0.88f;
            kit.hipScale *= 0.86f;
            kit.limbScale *= 0.92f;
            kit.torsoScale *= 0.94f;
            kit.headScale *= 1.08f;
            kit.cloakScale *= 1.22f;
            kit.clothScale *= 1.28f;
            kit.glowScale *= 1.18f;
            kit.patternMaterial = kMaterialRoomPulse;
        } else if (role == static_cast<uint32_t>(EnemyKind::Skirmisher)) {
            kit.shoulderScale *= 0.92f;
            kit.hipScale *= 0.90f;
            kit.limbScale *= 1.08f;
            kit.footScale *= 0.92f;
            kit.ornamentScale *= 1.18f;
            kit.silhouetteScale *= 1.12f;
            kit.patternMaterial = kMaterialControl;
        } else if (role == static_cast<uint32_t>(EnemyKind::Bulwark)) {
            kit.shoulderScale *= 1.10f;
            kit.hipScale *= 1.16f;
            kit.limbScale *= 1.04f;
            kit.torsoScale *= 1.20f;
            kit.armorScale *= 1.28f;
            kit.maskScale *= 1.10f;
            kit.patternMaterial = kMaterialProjectile;
        } else if (role == static_cast<uint32_t>(EnemyKind::Boss)) {
            kit.shoulderScale *= 1.24f;
            kit.hipScale *= 1.10f;
            kit.limbScale *= 1.12f;
            kit.torsoScale *= 1.18f;
            kit.headScale *= 1.10f;
            kit.cloakScale *= 1.32f;
            kit.ornamentScale *= 1.40f;
            kit.maskScale *= 1.22f;
            kit.glowScale *= 1.26f;
            kit.silhouetteScale *= 1.24f;
            kit.patternMaterial = kMaterialHitSpark;
        }
    } else {
        kit.patternMaterial = element != Element::None ? ElementTellMaterial(element) : kMaterialPlayerBlade;
        if (weaponClass == static_cast<uint32_t>(WeaponId::Hammer)) {
            kit.shoulderScale *= 1.08f;
            kit.hipScale *= 1.06f;
            kit.limbScale *= 1.06f;
        } else if (weaponClass == static_cast<uint32_t>(WeaponId::Katana) || weaponClass == static_cast<uint32_t>(WeaponId::Spear)) {
            kit.shoulderScale *= 0.96f;
            kit.hipScale *= 0.94f;
            kit.limbScale *= 1.04f;
            kit.cloakScale *= 1.10f;
        } else if (weaponClass >= static_cast<uint32_t>(WeaponId::Staff)) {
            kit.cloakScale *= 1.20f;
            kit.ornamentScale *= 1.18f;
            kit.clothScale *= 1.14f;
            kit.glowScale *= 1.12f;
        }
    }

    return kit;
}

CharacterRigPose BuildSharedCharacterPose(
    Vec3 position,
    Vec3 direction,
    float radius,
    float heightScale,
    float phase,
    uint32_t weaponClass,
    AttackShape actionShape,
    WeaponActionIndex actionIndex,
    uint32_t role,
    bool enemy,
    const CharacterKit& kit) {
    CharacterRigPose rig{};
    rig.dir = FlatDirection(direction);
    rig.right = FlatRight(direction);
    const float r = std::max(radius, 0.18f);
    const float h = std::max(0.70f, heightScale);
    const float baseY = position.y;
    rig.action = Saturate(phase);
    rig.swing = std::sin(rig.action * 3.14159265f);
    rig.wind = Smooth01(Saturate(rig.action / 0.68f));
    rig.follow = Smooth01(Saturate((rig.action - 0.68f) / 0.32f));
    rig.contact = Smooth01(1.0f - Saturate(std::abs(rig.action - 0.68f) / 0.20f));

    rig.pelvis = Vec3{position.x, baseY + r * (enemy ? 0.36f : 0.34f), position.z};
    rig.chest = Vec3{position.x, baseY + r * (enemy ? h * 0.62f : 0.62f) * kit.torsoScale, position.z};
    rig.head = Vec3{position.x, baseY + r * (enemy ? h + 0.30f : 1.18f) * kit.headScale, position.z};

    const float hipSpread = r * (enemy ? 0.20f : 0.18f) * kit.hipScale;
    rig.leftHip = Add(position, Add(Scale(rig.right, -hipSpread), Vec3{0.0f, r * (enemy ? 0.32f : 0.34f), 0.0f}));
    rig.rightHip = Add(position, Add(Scale(rig.right, hipSpread), Vec3{0.0f, r * (enemy ? 0.32f : 0.34f), 0.0f}));

    const bool hammerBody = role == static_cast<uint32_t>(EnemyKind::Brute) || weaponClass == static_cast<uint32_t>(WeaponId::Hammer);
    const bool dashBody = role == static_cast<uint32_t>(EnemyKind::Skirmisher) || weaponClass == static_cast<uint32_t>(WeaponId::Spear) || actionShape == AttackShape::Dash;
    const bool bulwarkBody = enemy && role == static_cast<uint32_t>(EnemyKind::Bulwark);
    if (enemy) {
        rig.leftFoot = Add(position, Add(Scale(rig.right, -r * (0.28f + h * 0.020f) * kit.footScale), Add(Scale(rig.dir, r * (0.14f + rig.swing * 0.04f)), Vec3{0.0f, r * 0.060f, 0.0f})));
        rig.rightFoot = Add(position, Add(Scale(rig.right, r * (0.28f + h * 0.018f) * kit.footScale), Add(Scale(rig.dir, -r * (0.16f + rig.swing * 0.04f)), Vec3{0.0f, r * 0.060f, 0.0f})));
        if (hammerBody) {
            rig.leftFoot = Add(position, Add(Scale(rig.right, -r * (0.38f + rig.contact * 0.05f) * kit.footScale), Add(Scale(rig.dir, r * (0.08f + rig.contact * 0.08f)), Vec3{0.0f, r * 0.060f, 0.0f})));
            rig.rightFoot = Add(position, Add(Scale(rig.right, r * (0.40f + rig.contact * 0.06f) * kit.footScale), Add(Scale(rig.dir, -r * (0.28f + rig.wind * 0.10f)), Vec3{0.0f, r * 0.060f, 0.0f})));
        } else if (dashBody) {
            rig.leftFoot = Add(position, Add(Scale(rig.right, -r * (0.22f + rig.contact * 0.04f) * kit.footScale), Add(Scale(rig.dir, r * (0.36f + rig.contact * 0.18f)), Vec3{0.0f, r * 0.060f, 0.0f})));
            rig.rightFoot = Add(position, Add(Scale(rig.right, r * (0.30f + rig.wind * 0.06f) * kit.footScale), Add(Scale(rig.dir, -r * (0.32f + rig.wind * 0.12f)), Vec3{0.0f, r * 0.060f, 0.0f})));
        } else if (bulwarkBody) {
            rig.leftFoot = Add(position, Add(Scale(rig.right, -r * 0.44f * kit.footScale), Add(Scale(rig.dir, r * 0.04f), Vec3{0.0f, r * 0.060f, 0.0f})));
            rig.rightFoot = Add(position, Add(Scale(rig.right, r * 0.44f * kit.footScale), Add(Scale(rig.dir, -r * 0.10f), Vec3{0.0f, r * 0.060f, 0.0f})));
        }
    } else {
        rig.leftFoot = Add(position, Add(Scale(rig.right, -r * (0.24f + rig.action * 0.05f) * kit.footScale), Add(Scale(rig.dir, r * (0.22f - rig.swing * 0.06f)), Vec3{0.0f, r * 0.070f, 0.0f})));
        rig.rightFoot = Add(position, Add(Scale(rig.right, r * (0.24f + rig.action * 0.04f) * kit.footScale), Add(Scale(rig.dir, -r * (0.10f + rig.swing * 0.04f)), Vec3{0.0f, r * 0.070f, 0.0f})));
        if (weaponClass == static_cast<uint32_t>(WeaponId::Hammer)) {
            rig.leftFoot = Add(position, Add(Scale(rig.right, -r * (0.34f + rig.contact * 0.04f) * kit.footScale), Add(Scale(rig.dir, r * (0.12f + rig.contact * 0.08f)), Vec3{0.0f, r * 0.070f, 0.0f})));
            rig.rightFoot = Add(position, Add(Scale(rig.right, r * (0.36f + rig.contact * 0.06f) * kit.footScale), Add(Scale(rig.dir, -r * (0.24f + rig.wind * 0.10f)), Vec3{0.0f, r * 0.070f, 0.0f})));
        } else if (weaponClass == static_cast<uint32_t>(WeaponId::Spear) || actionShape == AttackShape::Dash) {
            rig.leftFoot = Add(position, Add(Scale(rig.right, -r * (0.20f + rig.contact * 0.03f) * kit.footScale), Add(Scale(rig.dir, r * (0.40f + rig.contact * 0.16f)), Vec3{0.0f, r * 0.070f, 0.0f})));
            rig.rightFoot = Add(position, Add(Scale(rig.right, r * (0.26f + rig.wind * 0.04f) * kit.footScale), Add(Scale(rig.dir, -r * (0.32f + rig.wind * 0.10f)), Vec3{0.0f, r * 0.070f, 0.0f})));
        } else if (weaponClass == static_cast<uint32_t>(WeaponId::Katana)) {
            const float side = actionIndex == WeaponActionIndex::Action2 ? -1.0f : 1.0f;
            rig.leftFoot = Add(position, Add(Scale(rig.right, -r * (0.28f + rig.contact * 0.04f * side) * kit.footScale), Add(Scale(rig.dir, r * (0.24f + rig.contact * 0.10f)), Vec3{0.0f, r * 0.070f, 0.0f})));
            rig.rightFoot = Add(position, Add(Scale(rig.right, r * (0.30f - rig.contact * 0.03f * side) * kit.footScale), Add(Scale(rig.dir, -r * (0.18f + rig.wind * 0.05f)), Vec3{0.0f, r * 0.070f, 0.0f})));
        }
    }

    const float shoulderSpread = r * (enemy ? 0.42f : 0.34f) * kit.shoulderScale;
    rig.leftShoulder = enemy
        ? Add(position, Add(Scale(rig.right, -shoulderSpread), Vec3{0.0f, r * (0.76f + h * 0.08f), 0.0f}))
        : Add(rig.chest, Add(Scale(rig.right, -shoulderSpread), Vec3{0.0f, r * 0.080f, 0.0f}));
    rig.rightShoulder = enemy
        ? Add(position, Add(Scale(rig.right, shoulderSpread), Vec3{0.0f, r * (0.76f + h * 0.08f), 0.0f}))
        : Add(rig.chest, Add(Scale(rig.right, shoulderSpread), Vec3{0.0f, r * 0.080f, 0.0f}));

    if (enemy) {
        rig.leftHand = Add(position, Add(Scale(rig.right, -r * (0.58f + rig.swing * 0.08f) * kit.limbScale), Add(Scale(rig.dir, r * (0.24f + rig.action * 0.08f)), Vec3{0.0f, r * (0.46f + rig.action * 0.08f), 0.0f})));
        rig.rightHand = Add(position, Add(Scale(rig.right, r * (0.58f + rig.swing * 0.12f) * kit.limbScale), Add(Scale(rig.dir, r * (0.30f + rig.action * 0.14f)), Vec3{0.0f, r * (0.48f + rig.action * 0.10f), 0.0f})));
        if (hammerBody) {
            const Vec3 grip = Add(position, Add(Scale(rig.dir, r * (0.18f + rig.contact * 0.34f)), Add(Scale(rig.right, r * (0.60f - rig.follow * 0.82f)), Vec3{0.0f, r * (0.72f + rig.wind * 0.34f - rig.contact * 0.24f), 0.0f})));
            rig.leftHand = Add(grip, Scale(rig.right, -r * 0.16f));
            rig.rightHand = Add(grip, Scale(rig.right, r * 0.14f));
        } else if (role == static_cast<uint32_t>(EnemyKind::Caster) || weaponClass >= static_cast<uint32_t>(WeaponId::Staff)) {
            rig.leftHand = Add(position, Add(Scale(rig.right, -r * (0.52f + rig.wind * 0.06f) * kit.limbScale), Add(Scale(rig.dir, r * (0.20f + rig.contact * 0.08f)), Vec3{0.0f, r * (0.86f + rig.wind * 0.26f), 0.0f})));
            rig.rightHand = Add(position, Add(Scale(rig.right, r * (0.52f + rig.wind * 0.06f) * kit.limbScale), Add(Scale(rig.dir, r * (0.22f + rig.contact * 0.10f)), Vec3{0.0f, r * (0.88f + rig.wind * 0.24f), 0.0f})));
        } else if (role == static_cast<uint32_t>(EnemyKind::Skirmisher) || weaponClass == static_cast<uint32_t>(WeaponId::Katana)) {
            const Vec3 grip = Add(position, Add(Scale(rig.dir, r * (0.44f + rig.contact * 0.30f)), Add(Scale(rig.right, r * (0.52f - rig.follow * 0.64f)), Vec3{0.0f, r * (0.58f + rig.wind * 0.18f), 0.0f})));
            rig.rightHand = Add(grip, Scale(rig.right, r * 0.12f));
            rig.leftHand = Add(grip, Add(Scale(rig.right, -r * 0.12f), Scale(rig.dir, -r * 0.08f)));
        } else if (bulwarkBody) {
            rig.leftHand = Add(position, Add(Scale(rig.dir, r * 0.58f), Add(Scale(rig.right, -r * 0.34f), Vec3{0.0f, r * 0.70f, 0.0f})));
            rig.rightHand = Add(position, Add(Scale(rig.dir, r * 0.60f), Add(Scale(rig.right, r * 0.34f), Vec3{0.0f, r * 0.72f, 0.0f})));
        }
    } else {
        rig.leftHand = Add(rig.chest, Add(Scale(rig.right, -r * (0.48f + rig.swing * 0.08f) * kit.limbScale), Add(Scale(rig.dir, r * (0.26f + rig.action * 0.10f)), Vec3{0.0f, -r * 0.030f + rig.action * r * 0.080f, 0.0f})));
        rig.rightHand = Add(rig.chest, Add(Scale(rig.right, r * (0.48f + rig.swing * 0.12f) * kit.limbScale), Add(Scale(rig.dir, r * (0.34f + rig.action * 0.18f)), Vec3{0.0f, -r * 0.020f + rig.action * r * 0.10f, 0.0f})));
        if (weaponClass == static_cast<uint32_t>(WeaponId::Hammer)) {
            const Vec3 grip = Add(rig.chest, Add(Scale(rig.dir, r * (0.18f + rig.contact * 0.36f)), Add(Scale(rig.right, r * (0.54f - rig.follow * 0.72f)), Vec3{0.0f, r * (0.34f + rig.wind * 0.38f - rig.contact * 0.28f), 0.0f})));
            rig.leftHand = Add(grip, Scale(rig.right, -r * 0.18f));
            rig.rightHand = Add(grip, Scale(rig.right, r * 0.12f));
        } else if (weaponClass == static_cast<uint32_t>(WeaponId::Spear) || actionShape == AttackShape::Dash) {
            rig.rightHand = Add(rig.chest, Add(Scale(rig.dir, r * (0.62f + rig.contact * 0.38f)), Add(Scale(rig.right, r * 0.18f), Vec3{0.0f, r * (0.02f + rig.wind * 0.08f), 0.0f})));
            rig.leftHand = Add(rig.chest, Add(Scale(rig.dir, r * (0.18f + rig.contact * 0.18f)), Add(Scale(rig.right, -r * 0.25f), Vec3{0.0f, -r * 0.02f, 0.0f})));
        } else if (weaponClass == static_cast<uint32_t>(WeaponId::Katana)) {
            const float side = actionIndex == WeaponActionIndex::Action2 ? -1.0f : 1.0f;
            const Vec3 grip = Add(rig.chest, Add(Scale(rig.dir, r * (0.34f + rig.contact * 0.26f)), Add(Scale(rig.right, side * r * (0.40f - rig.follow * 0.54f)), Vec3{0.0f, r * (0.10f + rig.wind * 0.18f - rig.contact * 0.05f), 0.0f})));
            rig.rightHand = Add(grip, Scale(rig.right, side * r * 0.12f));
            rig.leftHand = Add(grip, Add(Scale(rig.right, -side * r * 0.10f), Scale(rig.dir, -r * 0.10f)));
        } else if (weaponClass >= static_cast<uint32_t>(WeaponId::Pistol) && weaponClass <= static_cast<uint32_t>(WeaponId::Shotgun)) {
            rig.rightHand = Add(rig.chest, Add(Scale(rig.dir, r * (0.56f + rig.contact * 0.16f)), Add(Scale(rig.right, r * 0.20f), Vec3{0.0f, r * (0.04f + rig.contact * 0.04f), 0.0f})));
            rig.leftHand = Add(rig.chest, Add(Scale(rig.dir, r * (0.42f + rig.contact * 0.12f)), Add(Scale(rig.right, -r * 0.18f), Vec3{0.0f, r * 0.01f, 0.0f})));
        }
    }

    return rig;
}

Vec3 RigOffsetPoint(const CharacterRigPose& rig, Vec3 center, float side, float forward, float yOffset) {
    return Add(
        Add(Vec3{center.x, center.y + yOffset, center.z}, Scale(rig.right, side)),
        Scale(rig.dir, forward));
}

void AddRigBodySurface(
    GeneratedRTGeometry& geo,
    const CharacterRigPose& rig,
    float radius,
    const CharacterKit& kit,
    bool enemy,
    uint32_t role) {
    const float r = std::max(radius, 0.12f);
    const float brute = enemy && role == static_cast<uint32_t>(EnemyKind::Brute) ? 1.0f : 0.0f;
    const float caster = enemy && role == static_cast<uint32_t>(EnemyKind::Caster) ? 1.0f : 0.0f;
    const float boss = enemy && role == static_cast<uint32_t>(EnemyKind::Boss) ? 1.0f : 0.0f;
    constexpr int kRings = 5;
    constexpr int kSegments = 8;
    std::array<std::array<Vec3, kSegments>, kRings> rings{};

    const float lowY = rig.pelvis.y - r * (enemy ? 0.24f : 0.20f);
    const float waistY = rig.pelvis.y + r * (0.08f + caster * 0.06f);
    const float ribsY = rig.pelvis.y * 0.44f + rig.chest.y * 0.56f;
    const float chestY = rig.chest.y + r * (0.14f + boss * 0.04f);
    const float collarY = rig.head.y - r * (0.29f - caster * 0.03f);
    const std::array<float, kRings> ys{lowY, waistY, ribsY, chestY, collarY};
    const std::array<float, kRings> widths{
        r * (enemy ? 0.25f : 0.20f) * kit.hipScale,
        r * (enemy ? 0.34f : 0.25f) * kit.hipScale,
        r * (enemy ? (0.43f + brute * 0.10f) : 0.32f) * kit.torsoScale,
        r * (enemy ? (0.46f + brute * 0.12f + boss * 0.06f) : 0.35f) * kit.shoulderScale,
        r * (enemy ? (0.22f + caster * 0.05f) : 0.16f) * kit.headScale
    };
    const std::array<float, kRings> frontDepths{
        r * (enemy ? 0.12f : 0.10f),
        r * (enemy ? 0.17f : 0.13f),
        r * (enemy ? 0.22f : 0.17f),
        r * (enemy ? 0.22f : 0.18f),
        r * (enemy ? 0.12f : 0.10f)
    };
    const std::array<float, kRings> backDepths{
        r * (enemy ? 0.18f : 0.15f),
        r * (enemy ? 0.24f : 0.19f),
        r * (enemy ? 0.29f : 0.23f),
        r * (enemy ? (0.32f + caster * 0.08f + boss * 0.07f) : 0.27f),
        r * (enemy ? 0.18f : 0.16f)
    };

    for (int i = 0; i < kRings; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRings - 1);
        const Vec3 center = Add(
            Vec3{rig.pelvis.x, ys[i], rig.pelvis.z},
            Scale(rig.dir, r * (-0.030f + t * 0.060f - caster * 0.030f)));
        for (int j = 0; j < kSegments; ++j) {
            const float a = static_cast<float>(j) * 6.28318530718f / static_cast<float>(kSegments);
            const float cx = std::cos(a);
            const float sz = std::sin(a);
            const float taper = 0.94f + (j & 1 ? 0.06f : -0.02f);
            const float depth = sz >= 0.0f ? backDepths[i] : frontDepths[i];
            rings[i][j] = Add(center, Add(Scale(rig.right, cx * widths[i] * taper), Scale(rig.dir, sz * depth)));
        }
    }

    for (int i = 0; i < kRings - 1; ++i) {
        for (int j = 0; j < kSegments; ++j) {
            const int next = (j + 1) % kSegments;
            AddQuad(geo, rings[i][j], rings[i][next], rings[i + 1][next], rings[i + 1][j], kit.bodyMaterial);
        }
    }
    for (int j = 0; j < kSegments; ++j) {
        const int next = (j + 1) % kSegments;
        AddTri(geo, Vec3{rig.pelvis.x, lowY - r * 0.025f, rig.pelvis.z}, rings[0][next], rings[0][j], kit.bodyMaterial);
        AddTri(geo, Vec3{rig.pelvis.x, collarY + r * 0.035f, rig.pelvis.z}, rings[kRings - 1][j], rings[kRings - 1][next], kit.bodyMaterial);
    }

    const float armorLift = enemy ? 0.0f : 0.015f;
    const Vec3 frontLow = RigOffsetPoint(rig, rig.pelvis, 0.0f, r * (0.18f + armorLift), r * 0.02f);
    const Vec3 frontHigh = RigOffsetPoint(rig, rig.chest, 0.0f, r * (0.23f + armorLift), r * 0.13f);
    const float panelW = r * (enemy ? 0.18f : 0.12f) * kit.armorScale;
    AddQuad(
        geo,
        Add(frontLow, Scale(rig.right, -panelW)),
        Add(frontLow, Scale(rig.right, panelW)),
        Add(frontHigh, Scale(rig.right, panelW * 0.72f)),
        Add(frontHigh, Scale(rig.right, -panelW * 0.72f)),
        kit.accentMaterial);

    const uint32_t lineMaterial = enemy ? kit.patternMaterial : kit.accentMaterial;
    AddQuad(
        geo,
        RigOffsetPoint(rig, rig.pelvis, -r * 0.024f, r * 0.235f, r * 0.05f),
        RigOffsetPoint(rig, rig.pelvis, r * 0.024f, r * 0.235f, r * 0.05f),
        RigOffsetPoint(rig, rig.chest, r * 0.018f, r * 0.270f, r * 0.20f),
        RigOffsetPoint(rig, rig.chest, -r * 0.018f, r * 0.270f, r * 0.20f),
        lineMaterial);
}

void AddDrapedClothSurface(
    GeneratedRTGeometry& geo,
    const CharacterRigPose& rig,
    float radius,
    const CharacterKit& kit,
    bool enemy,
    uint32_t role,
    float phase) {
    const float r = std::max(radius, 0.12f);
    const float action = Saturate(phase);
    const float caster = enemy && role == static_cast<uint32_t>(EnemyKind::Caster) ? 1.0f : 0.0f;
    const float boss = enemy && role == static_cast<uint32_t>(EnemyKind::Boss) ? 1.0f : 0.0f;
    const float cloth = kit.clothScale * (enemy ? (0.92f + caster * 0.34f + boss * 0.32f) : 0.78f);
    const float back = r * (0.30f + cloth * 0.15f);
    const float shoulder = r * (enemy ? 0.40f : 0.30f) * kit.shoulderScale;
    const float hem = r * (enemy ? (0.42f + caster * 0.20f + boss * 0.24f) : 0.30f) * kit.cloakScale;
    const float fall = r * (enemy ? (0.54f + caster * 0.24f + boss * 0.28f) : 0.40f);
    const Vec3 collarCenter = Add(rig.chest, Add(Scale(rig.dir, -r * 0.13f), Vec3{0.0f, r * 0.21f, 0.0f}));
    const Vec3 hemCenter = Add(rig.pelvis, Add(Scale(rig.dir, -back - action * r * 0.04f), Vec3{0.0f, -fall, 0.0f}));
    const Vec3 midCenter = Add(rig.pelvis, Add(Scale(rig.dir, -back * 0.78f), Vec3{0.0f, -fall * 0.35f, 0.0f}));
    const Vec3 topL = Add(collarCenter, Scale(rig.right, -shoulder));
    const Vec3 topR = Add(collarCenter, Scale(rig.right, shoulder));
    const Vec3 midL = Add(midCenter, Scale(rig.right, -hem * (0.78f + action * 0.06f)));
    const Vec3 midR = Add(midCenter, Scale(rig.right, hem * (0.78f - action * 0.04f)));
    const Vec3 botL = Add(hemCenter, Scale(rig.right, -hem));
    const Vec3 botR = Add(hemCenter, Scale(rig.right, hem * (0.94f + action * 0.05f)));
    AddQuad(geo, topL, topR, midR, midL, kit.bodyMaterial);
    AddQuad(geo, midL, midR, botR, botL, kit.bodyMaterial);
    AddQuad(
        geo,
        Add(topL, Scale(rig.right, r * 0.030f)),
        Add(topR, Scale(rig.right, -r * 0.030f)),
        Add(midR, Scale(rig.right, -r * 0.024f)),
        Add(midL, Scale(rig.right, r * 0.024f)),
        kit.bodyMaterial);
    AddGuideStrip(geo, Add(botL, Scale(rig.right, hem * 0.50f)), rig.right, hem * 1.00f, r * 0.008f, kit.accentMaterial);
    if (enemy && (caster > 0.5f || boss > 0.5f)) {
        AddFloatingRibbon(geo, Add(collarCenter, Scale(rig.right, -shoulder * 0.72f)), rig.dir, Scale(rig.right, -1.0f), r * (0.92f + boss * 0.28f), r * 0.18f, r * 0.018f, kit.patternMaterial);
        AddFloatingRibbon(geo, Add(collarCenter, Scale(rig.right, shoulder * 0.70f)), rig.dir, rig.right, r * (0.88f + boss * 0.22f), r * 0.17f, r * 0.017f, kit.patternMaterial);
    }
}

void AddRigLimbCloth(
    GeneratedRTGeometry& geo,
    const CharacterRigPose& rig,
    Vec3 start,
    Vec3 end,
    float radius,
    uint32_t materialId) {
    const Vec3 axis = Normalize(Sub(end, start));
    Vec3 side = Cross(axis, Vec3{0.0f, 1.0f, 0.0f});
    if (LengthSq(side) <= 0.00001f) {
        side = rig.right;
    }
    side = Normalize(side);
    const float w0 = radius * 0.040f;
    const float w1 = radius * 0.026f;
    const Vec3 s0 = Add(start, Scale(side, -w0));
    const Vec3 s1 = Add(start, Scale(side, w0));
    const Vec3 e0 = Add(end, Scale(side, -w1));
    const Vec3 e1 = Add(end, Scale(side, w1));
    AddQuad(geo, s0, s1, e1, e0, materialId);
}

void AddProceduralKitMarks(
    GeneratedRTGeometry& geo,
    const CharacterRigPose& rig,
    float radius,
    const CharacterKit& kit,
    Element element,
    bool enemy) {
    const float r = std::max(radius, 0.12f);
    const uint32_t mat = element != Element::None ? ElementTellMaterial(element) : kit.patternMaterial;
    const int count = enemy ? 5 + static_cast<int>(kit.variant & 3u) : 4 + static_cast<int>(kit.variant & 1u);
    for (int i = 0; i < count; ++i) {
        const uint32_t h = HashMix(kit.variant * 97u + static_cast<uint32_t>(i) * 53u, enemy ? 0x9d1u : 0x6b5u);
        const float side = (i & 1) == 0 ? -1.0f : 1.0f;
        const float lane = (0.05f + HashUnit(HashMix(h, 0x11u)) * 0.18f) * side;
        const float height = 0.12f + HashUnit(HashMix(h, 0x21u)) * 0.30f;
        const Vec3 p = Add(rig.chest, Add(Scale(rig.right, r * lane), Add(Scale(rig.dir, r * (0.18f + HashUnit(HashMix(h, 0x31u)) * 0.12f)), Vec3{0.0f, r * (height - 0.18f), 0.0f})));
        AddEllipsoid(
            geo,
            p,
            Vec3{r * (0.030f + HashUnit(HashMix(h, 0x41u)) * 0.020f) * kit.ornamentScale, r * 0.018f, r * (0.024f + HashUnit(HashMix(h, 0x51u)) * 0.018f) * kit.ornamentScale},
            mat,
            7,
            3);
    }

    AddEllipsoid(geo, Add(rig.leftShoulder, Scale(rig.dir, r * 0.050f)), Vec3{r * 0.060f * kit.ornamentScale, r * 0.026f, r * 0.044f}, kit.accentMaterial, 7, 3);
    AddEllipsoid(geo, Add(rig.rightShoulder, Scale(rig.dir, r * 0.050f)), Vec3{r * 0.060f * kit.ornamentScale, r * 0.026f, r * 0.044f}, kit.accentMaterial, 7, 3);
}

void AddReferenceCharacterKitDetails(
    GeneratedRTGeometry& geo,
    const CharacterRigPose& rig,
    float radius,
    const CharacterKit& kit,
    Element element,
    bool enemy,
    uint32_t role,
    uint32_t weaponClass,
    float phase) {
    const float r = std::max(radius, 0.12f);
    const Vec3 dir = rig.dir;
    const Vec3 right = rig.right;
    const uint32_t glowMaterial = element != Element::None ? ElementTellMaterial(element) : kit.patternMaterial;
    const float action = Saturate(phase);
    const float pulse = 0.72f + std::sin(action * 3.14159265f) * 0.28f;
    const Vec3 frontChest = Add(rig.chest, Scale(dir, r * 0.27f));
    const Vec3 waist = Add(rig.pelvis, Add(Scale(dir, r * 0.17f), Vec3{0.0f, r * 0.10f, 0.0f}));
    const Vec3 collar = Add(rig.chest, Vec3{0.0f, r * 0.20f, 0.0f});
    const float robeWidth = enemy ? r * (0.46f + kit.clothScale * 0.08f) : r * (0.30f + kit.clothScale * 0.045f);
    const float robeHeight = enemy ? r * (0.72f + kit.clothScale * 0.10f) : r * (0.50f + kit.clothScale * 0.055f);
    const float runeWidth = enemy ? r * (0.18f + kit.ornamentScale * 0.035f) : r * (0.055f + kit.ornamentScale * 0.016f);
    const float runeHeight = enemy ? r * (0.56f + kit.clothScale * 0.06f) : r * (0.28f + kit.clothScale * 0.035f);

    AddVerticalPanel(
        geo,
        Add(frontChest, Vec3{0.0f, enemy ? -r * 0.42f : -r * 0.34f, 0.0f}),
        dir,
        robeWidth,
        robeHeight,
        kit.bodyMaterial,
        0.0f);
    AddVerticalPanel(
        geo,
        Add(frontChest, Add(Scale(dir, r * 0.012f), Vec3{0.0f, enemy ? -r * 0.32f : -r * 0.18f, 0.0f})),
        dir,
        runeWidth,
        runeHeight,
        glowMaterial,
        0.0f);
    AddEllipsoid(geo, Add(waist, Vec3{0.0f, r * 0.05f, 0.0f}), Vec3{r * 0.24f, r * 0.030f, r * 0.060f}, kit.accentMaterial, 8, 3);
    AddEllipsoid(geo, Add(collar, Scale(dir, r * 0.06f)), Vec3{r * 0.22f, r * 0.026f, r * 0.054f}, kit.accentMaterial, 8, 3);

    for (int sideIndex = -1; sideIndex <= 1; sideIndex += 2) {
        const float side = static_cast<float>(sideIndex);
        const Vec3 shoulder = side < 0.0f ? rig.leftShoulder : rig.rightShoulder;
        const Vec3 hand = side < 0.0f ? rig.leftHand : rig.rightHand;
        const Vec3 hip = side < 0.0f ? rig.leftHip : rig.rightHip;
        const Vec3 foot = side < 0.0f ? rig.leftFoot : rig.rightFoot;

        AddEllipsoid(
            geo,
            Add(shoulder, Add(Scale(dir, r * 0.060f), Scale(right, side * r * 0.035f))),
            Vec3{r * (0.105f + kit.armorScale * 0.038f), r * 0.050f, r * (0.080f + kit.silhouetteScale * 0.030f)},
            kit.accentMaterial,
            10,
            4);
        AddEllipsoid(
            geo,
            Add(hand, Add(Scale(dir, r * 0.032f), Vec3{0.0f, r * 0.020f, 0.0f})),
            Vec3{r * 0.040f * kit.glowScale, r * 0.030f, r * 0.034f * kit.glowScale},
            glowMaterial,
            8,
            3);
        AddEllipsoid(
            geo,
            Add(hip, Add(Scale(dir, r * 0.055f), Vec3{0.0f, -r * 0.030f, 0.0f})),
            Vec3{r * 0.074f, r * 0.038f, r * 0.058f},
            kit.accentMaterial,
            8,
            3);
        AddEllipsoid(
            geo,
            Add(foot, Add(Scale(dir, r * 0.055f), Vec3{0.0f, r * 0.035f, 0.0f})),
            Vec3{r * 0.062f, r * 0.030f, r * 0.046f},
            kit.accentMaterial,
            8,
            3);
    }

    const int gemCount = enemy ? 5 : 2;
    for (int i = 0; i < gemCount; ++i) {
        const uint32_t h = HashMix(kit.variant + static_cast<uint32_t>(i) * 0x45d9u, enemy ? 0x541u : 0x319u);
        const float side = (HashUnit(HashMix(h, 0x11u)) - 0.5f) * r * 0.42f;
        const float y = r * (-0.18f + HashUnit(HashMix(h, 0x23u)) * 0.42f);
        const Vec3 p = Add(frontChest, Add(Scale(right, side), Vec3{0.0f, y, 0.0f}));
        AddEllipsoid(
            geo,
            p,
            Vec3{r * (0.022f + kit.glowScale * 0.006f), r * 0.017f, r * (0.019f + kit.glowScale * 0.006f)},
            glowMaterial,
            7,
            3);
    }

    if (!enemy) {
        AddEllipsoid(geo, Add(rig.head, Add(Scale(dir, r * 0.18f), Vec3{0.0f, r * 0.08f, 0.0f})), Vec3{r * 0.19f * kit.maskScale, r * 0.070f, r * 0.072f}, kit.accentMaterial, 10, 4);
        AddEllipsoid(geo, Add(rig.head, Add(Scale(dir, r * 0.23f), Vec3{0.0f, r * 0.010f, 0.0f})), Vec3{r * 0.13f, r * 0.038f, r * 0.044f}, glowMaterial, 8, 3);
        if (weaponClass == static_cast<uint32_t>(WeaponId::Katana) || weaponClass == static_cast<uint32_t>(WeaponId::Spear)) {
            AddFloatingRibbon(geo, Add(collar, Scale(right, -r * 0.10f)), dir, Scale(right, -1.0f), r * 0.62f * kit.clothScale, r * 0.12f, r * 0.014f, kit.accentMaterial);
            AddFloatingRibbon(geo, Add(collar, Scale(right, r * 0.12f)), dir, right, r * 0.54f * kit.clothScale, r * 0.10f, r * 0.012f, kit.bodyMaterial);
        }
        return;
    }

    if (role == static_cast<uint32_t>(EnemyKind::Brute)) {
        AddEllipsoid(geo, Add(rig.head, Add(Scale(dir, r * 0.16f), Vec3{0.0f, r * 0.04f, 0.0f})), Vec3{r * 0.25f * kit.maskScale, r * 0.10f, r * 0.10f}, kit.patternMaterial, 10, 4);
        AddEllipsoid(geo, Add(frontChest, Vec3{0.0f, r * 0.08f, 0.0f}), Vec3{r * 0.26f, r * 0.040f, r * 0.070f}, kit.patternMaterial, 8, 3);
    } else if (role == static_cast<uint32_t>(EnemyKind::Caster)) {
        AddEllipsoid(geo, Add(rig.head, Vec3{0.0f, r * 0.24f, 0.0f}), Vec3{r * 0.30f * kit.maskScale, r * 0.18f, r * 0.25f}, kit.bodyMaterial, 11, 4);
        AddOctahedron(geo, Add(rig.head, Add(Scale(dir, r * 0.24f), Vec3{0.0f, r * (0.28f + pulse * 0.03f), 0.0f})), r * 0.060f * kit.glowScale, glowMaterial);
        AddFloatingRibbon(geo, Add(collar, Scale(right, -r * 0.24f)), dir, Scale(right, -1.0f), r * 0.76f * kit.clothScale, r * 0.16f, r * 0.015f, kit.bodyMaterial);
        AddFloatingRibbon(geo, Add(collar, Scale(right, r * 0.24f)), dir, right, r * 0.76f * kit.clothScale, r * 0.16f, r * 0.015f, kit.bodyMaterial);
    } else if (role == static_cast<uint32_t>(EnemyKind::Skirmisher)) {
        AddEllipsoid(geo, Add(rig.head, Add(Scale(dir, r * 0.17f), Vec3{0.0f, r * 0.12f, 0.0f})), Vec3{r * 0.18f * kit.maskScale, r * 0.070f, r * 0.070f}, glowMaterial, 9, 3);
        AddBlade(geo, Add(rig.leftShoulder, Add(Scale(right, -r * 0.16f), Vec3{0.0f, r * 0.06f, 0.0f})), Scale(right, -1.0f), r * 0.22f * kit.silhouetteScale, kit.accentMaterial);
        AddBlade(geo, Add(rig.rightShoulder, Add(Scale(right, r * 0.16f), Vec3{0.0f, r * 0.06f, 0.0f})), right, r * 0.22f * kit.silhouetteScale, kit.accentMaterial);
    } else if (role == static_cast<uint32_t>(EnemyKind::Bulwark)) {
        AddVerticalPanel(geo, Add(frontChest, Scale(dir, r * 0.08f)), dir, r * 0.56f * kit.armorScale, r * 0.78f, kit.patternMaterial, -r * 0.42f);
        AddEllipsoid(geo, Add(frontChest, Vec3{0.0f, r * 0.10f, 0.0f}), Vec3{r * 0.24f, r * 0.042f, r * 0.070f}, glowMaterial, 8, 3);
        AddEllipsoid(geo, Add(rig.head, Add(Scale(dir, r * 0.16f), Vec3{0.0f, r * 0.04f, 0.0f})), Vec3{r * 0.22f * kit.maskScale, r * 0.090f, r * 0.086f}, kit.patternMaterial, 10, 4);
    } else if (role == static_cast<uint32_t>(EnemyKind::Boss)) {
        AddEllipsoid(geo, Add(rig.head, Vec3{0.0f, r * 0.32f, 0.0f}), Vec3{r * 0.28f * kit.maskScale, r * 0.16f, r * 0.22f}, kit.accentMaterial, 12, 5);
        AddOctahedron(geo, Add(rig.head, Vec3{0.0f, r * (0.58f + pulse * 0.06f), 0.0f}), r * 0.10f * kit.glowScale, glowMaterial);
        AddFloatingRibbon(geo, Add(collar, Scale(right, -r * 0.30f)), dir, Scale(right, -1.0f), r * 0.96f * kit.clothScale, r * 0.22f, r * 0.020f, kit.bodyMaterial);
        AddFloatingRibbon(geo, Add(collar, Scale(right, r * 0.30f)), dir, right, r * 0.94f * kit.clothScale, r * 0.20f, r * 0.020f, kit.bodyMaterial);
        AddEllipsoid(geo, Add(frontChest, Vec3{0.0f, r * 0.15f, 0.0f}), Vec3{r * 0.30f, r * 0.048f, r * 0.082f}, glowMaterial, 9, 3);
    }
}

void AddHumanoidFigure(
    GeneratedRTGeometry& geo,
    Vec3 position,
    Vec3 direction,
    float radius,
    uint32_t bodyMaterial,
    uint32_t accentMaterial,
    float phase,
    uint32_t visualTag) {
    const float r = std::max(radius, 0.18f);
    const uint32_t weaponClass = visualTag & 0xffu;
    const Element element = PlayerVisualTagElement(visualTag);
    const WeaponActionIndex actionIndex = PlayerVisualTagAction(visualTag);
    const AttackShape actionShape = PlayerVisualTagShape(visualTag);
    const uint32_t variant = PlayerVisualTagVariant(visualTag);
    CharacterKit kit = BuildCharacterKit(false, 0u, weaponClass, element, bodyMaterial, accentMaterial, variant);
    bodyMaterial = kit.bodyMaterial;
    accentMaterial = kit.accentMaterial;
    const CharacterRigPose rig = BuildSharedCharacterPose(position, direction, r, 1.0f, phase, weaponClass, actionShape, actionIndex, 0u, false, kit);
    const Vec3 dir = rig.dir;
    const Vec3 right = rig.right;
    const float action = rig.action;
    const float swing = rig.swing;
    const float follow = rig.follow;
    const float contact = rig.contact;
    const Vec3 chest = rig.chest;
    const Vec3 head = rig.head;
    const Vec3 leftHip = rig.leftHip;
    const Vec3 rightHip = rig.rightHip;
    const Vec3 leftFoot = rig.leftFoot;
    const Vec3 rightFoot = rig.rightFoot;

    AddDrapedClothSurface(geo, rig, r * kit.cloakScale, kit, false, 0u, phase);
    AddProceduralCapeTails(geo, position, dir, r * kit.cloakScale * 0.76f, bodyMaterial, accentMaterial, phase);
    AddRigBodySurface(geo, rig, r, kit, false, 0u);
    AddShoulderPlates(geo, position, dir, r * kit.shoulderScale * 0.70f, accentMaterial);
    AddArmorFacets(geo, position, dir, r * kit.armorScale * 0.62f, bodyMaterial, accentMaterial, phase);
    AddJointLimb(geo, leftHip, leftFoot, r * 0.030f, bodyMaterial, bodyMaterial);
    AddJointLimb(geo, rightHip, rightFoot, r * 0.030f, bodyMaterial, bodyMaterial);
    AddEllipsoid(geo, Add(leftFoot, Scale(dir, r * 0.060f)), Vec3{r * 0.105f, r * 0.042f, r * 0.052f}, bodyMaterial, 8, 3);
    AddEllipsoid(geo, Add(rightFoot, Scale(dir, r * 0.060f)), Vec3{r * 0.105f, r * 0.042f, r * 0.052f}, bodyMaterial, 8, 3);
    const Vec3 leftShoulder = rig.leftShoulder;
    const Vec3 rightShoulder = rig.rightShoulder;
    const Vec3 leftHand = rig.leftHand;
    const Vec3 rightHand = rig.rightHand;
    AddJointLimb(geo, leftShoulder, leftHand, r * 0.027f, bodyMaterial, bodyMaterial);
    AddJointLimb(geo, rightShoulder, rightHand, r * 0.029f, bodyMaterial, bodyMaterial);
    AddRigLimbCloth(geo, rig, leftShoulder, leftHand, r, bodyMaterial);
    AddRigLimbCloth(geo, rig, rightShoulder, rightHand, r, bodyMaterial);
    AddEllipsoid(geo, leftHand, Vec3{r * 0.052f, r * 0.043f, r * 0.046f}, accentMaterial, 8, 3);
    AddEllipsoid(geo, rightHand, Vec3{r * 0.054f, r * 0.044f, r * 0.048f}, accentMaterial, 8, 3);
    AddEllipsoid(geo, Add(leftShoulder, Add(Scale(dir, r * 0.040f), Vec3{0.0f, r * 0.012f, 0.0f})), Vec3{r * 0.092f, r * 0.042f, r * 0.066f}, accentMaterial, 8, 3);
    AddEllipsoid(geo, Add(rightShoulder, Add(Scale(dir, r * 0.040f), Vec3{0.0f, r * 0.012f, 0.0f})), Vec3{r * 0.092f, r * 0.042f, r * 0.066f}, accentMaterial, 8, 3);
    AddCharacterHeadShell(geo, head, dir, r, bodyMaterial, accentMaterial, element != Element::None ? ElementTellMaterial(element) : accentMaterial, false, 0u, phase);
    AddChestRunes(geo, chest, dir, r, accentMaterial);
    AddProceduralKitMarks(geo, rig, r, kit, element, false);
    AddReferenceCharacterKitDetails(geo, rig, r, kit, element, false, 0u, weaponClass, phase);
    Vec3 weaponDir = dir;
    bool drawFineBladeAccent = false;
    if (weaponClass == static_cast<uint32_t>(WeaponId::Katana)) {
        weaponDir = Normalize(Add(Scale(dir, 1.0f), Scale(right, -0.28f + swing * 0.62f)));
        drawFineBladeAccent = true;
    } else if (weaponClass == static_cast<uint32_t>(WeaponId::Spear)) {
        weaponDir = Normalize(Add(Scale(dir, 1.0f), Scale(right, -0.06f + contact * 0.12f)));
        drawFineBladeAccent = true;
        AddGuideStrip(
            geo,
            Add(position, Add(Scale(weaponDir, r * (0.76f + contact * 0.36f)), Vec3{0.0f, r * (0.52f + contact * 0.04f), 0.0f})),
            weaponDir,
            r * (0.82f + contact * 0.40f),
            r * 0.008f,
            accentMaterial);
    } else if (weaponClass == static_cast<uint32_t>(WeaponId::Hammer)) {
        weaponDir = Normalize(Add(Scale(dir, 0.82f), Scale(right, 0.58f - follow * 0.90f)));
        AddElementalParticleField(
            geo,
            Add(position, Add(Scale(weaponDir, r * (0.50f + contact * 0.24f)), Vec3{0.0f, r * (0.18f + contact * 0.06f), 0.0f})),
            weaponDir,
            r * (0.30f + contact * 0.16f),
            element,
            phase,
            0.34f + contact * 0.44f);
    } else if (weaponClass <= static_cast<uint32_t>(WeaponId::Shotgun)) {
        weaponDir = Normalize(Add(Scale(dir, 1.0f), Scale(right, (swing - 0.5f) * 0.10f)));
        AddGuideStrip(
            geo,
            Add(position, Add(Scale(weaponDir, r * (0.70f + action * 0.32f)), Vec3{0.0f, r * (0.50f + action * 0.08f), 0.0f})),
            weaponDir,
            r * (0.90f + action * 0.40f),
            r * 0.024f,
            accentMaterial);
    } else {
        const Vec3 hand = Add(position, Add(Scale(right, r * (0.42f + swing * 0.18f)), Add(Scale(dir, r * 0.38f), Vec3{0.0f, r * (0.70f + action * 0.18f), 0.0f})));
        AddOctahedron(geo, hand, r * (0.12f + action * 0.05f), accentMaterial);
        AddGuideStrip(geo, Add(hand, Scale(dir, r * 0.20f)), Normalize(Add(dir, Scale(right, 0.42f))), r * (0.42f + action * 0.18f), r * 0.010f, accentMaterial);
        AddGuideStrip(geo, Add(hand, Scale(right, -r * 0.12f)), Normalize(Add(dir, Scale(right, -0.36f))), r * (0.32f + action * 0.14f), r * 0.008f, accentMaterial);
    }
    if (drawFineBladeAccent) {
        AddBlade(
            geo,
            Add(position, Add(Scale(weaponDir, r * (0.72f + action * 0.14f)), Vec3{0.0f, r * (0.50f + action * 0.08f), 0.0f})),
            weaponDir,
            r * (0.44f + action * 0.10f),
            accentMaterial);
    }
    AddPlayerWeaponRig(geo, position, dir, r, bodyMaterial, accentMaterial, weaponClass, phase);
    AddElementalParticleField(
        geo,
        Add(position, Add(Scale(dir, r * (0.22f + action * 0.18f)), Vec3{0.0f, r * (0.28f + action * 0.10f), 0.0f})),
        dir,
        r * (0.28f + action * 0.12f),
        element,
        phase,
        0.24f + action * 0.36f);
}

void AddEnemyFigure(
    GeneratedRTGeometry& geo,
    Vec3 position,
    Vec3 direction,
    float radius,
    uint32_t bodyMaterial,
    uint32_t accentMaterial,
    float heightScale,
    float phase,
    uint32_t visualTag) {
    const float r = std::max(radius, 0.18f);
    const float h = std::max(0.70f, heightScale);
    const float baseY = position.y;
    const uint32_t role = visualTag & 0xffu;
    const uint32_t weaponClass = (visualTag >> 8u) & 0xffu;
    const Element element = EnemyVisualTagElement(visualTag);
    const AttackShape actionShape = EnemyVisualTagShape(visualTag);
    const WeaponActionIndex actionIndex = EnemyVisualTagAction(visualTag);
    const uint32_t variant = EnemyVisualTagVariant(visualTag);
    CharacterKit kit = BuildCharacterKit(true, role, weaponClass, element, bodyMaterial, accentMaterial, variant);
    bodyMaterial = kit.bodyMaterial;
    accentMaterial = kit.accentMaterial;
    const CharacterRigPose rig = BuildSharedCharacterPose(position, direction, r, h, phase, weaponClass, actionShape, actionIndex, role, true, kit);
    const Vec3 dir = rig.dir;
    const Vec3 right = rig.right;
    const float action = rig.action;
    const float swing = rig.swing;
    const Vec3 leftHip = rig.leftHip;
    const Vec3 rightHip = rig.rightHip;
    const Vec3 leftFoot = rig.leftFoot;
    const Vec3 rightFoot = rig.rightFoot;

    AddDrapedClothSurface(geo, rig, r * kit.cloakScale, kit, true, role, phase);
    AddRigBodySurface(geo, rig, r, kit, true, role);
    AddShoulderPlates(geo, position, dir, r * (0.70f + h * 0.045f) * kit.shoulderScale, accentMaterial);
    AddArmorFacets(geo, position, dir, r * (0.66f + h * 0.030f) * kit.armorScale, bodyMaterial, accentMaterial, phase);
    AddJointLimb(geo, leftHip, leftFoot, r * (0.031f + h * 0.0015f), bodyMaterial, bodyMaterial);
    AddJointLimb(geo, rightHip, rightFoot, r * (0.031f + h * 0.0015f), bodyMaterial, bodyMaterial);
    AddEllipsoid(geo, Add(leftFoot, Scale(dir, r * 0.060f)), Vec3{r * (0.112f + h * 0.010f), r * 0.046f, r * 0.056f}, bodyMaterial, 8, 3);
    AddEllipsoid(geo, Add(rightFoot, Scale(dir, r * 0.060f)), Vec3{r * (0.112f + h * 0.010f), r * 0.046f, r * 0.056f}, bodyMaterial, 8, 3);
    const Vec3 leftShoulder = rig.leftShoulder;
    const Vec3 rightShoulder = rig.rightShoulder;
    const Vec3 leftHand = rig.leftHand;
    const Vec3 rightHand = rig.rightHand;
    AddJointLimb(geo, leftShoulder, leftHand, r * 0.028f, bodyMaterial, bodyMaterial);
    AddJointLimb(geo, rightShoulder, rightHand, r * 0.030f, bodyMaterial, bodyMaterial);
    AddRigLimbCloth(geo, rig, leftShoulder, leftHand, r, bodyMaterial);
    AddRigLimbCloth(geo, rig, rightShoulder, rightHand, r, bodyMaterial);
    AddEllipsoid(geo, leftHand, Vec3{r * 0.056f, r * 0.046f, r * 0.050f}, accentMaterial, 8, 3);
    AddEllipsoid(geo, rightHand, Vec3{r * 0.058f, r * 0.048f, r * 0.052f}, accentMaterial, 8, 3);
    AddEllipsoid(geo, Add(leftShoulder, Add(Scale(dir, r * 0.048f), Vec3{0.0f, r * 0.014f, 0.0f})), Vec3{r * 0.104f, r * 0.050f, r * 0.074f}, accentMaterial, 8, 3);
    AddEllipsoid(geo, Add(rightShoulder, Add(Scale(dir, r * 0.048f), Vec3{0.0f, r * 0.014f, 0.0f})), Vec3{r * 0.104f, r * 0.050f, r * 0.074f}, accentMaterial, 8, 3);
    AddCharacterHeadShell(geo, rig.head, dir, r, bodyMaterial, accentMaterial, element != Element::None ? ElementTellMaterial(element) : accentMaterial, true, role, phase);
    AddEllipsoid(
        geo,
        Add(position, Add(Scale(dir, r * (0.26f + action * 0.08f)), Vec3{0.0f, r * (0.42f + action * 0.05f), 0.0f})),
        Vec3{r * 0.052f, r * 0.070f, r * 0.046f},
        accentMaterial,
        7,
        3);
    AddEnemyRoleDetails(geo, position, dir, r, h, bodyMaterial, accentMaterial, role, weaponClass, phase);
    AddProceduralKitMarks(geo, rig, r, kit, element, true);
    AddReferenceCharacterKitDetails(geo, rig, r, kit, element, true, role, weaponClass, phase);
    AddElementalParticleField(
        geo,
        Add(position, Vec3{0.0f, r * (0.40f + h * 0.08f), 0.0f}),
        dir,
        r * (0.46f + h * 0.04f + action * 0.18f),
        element,
        phase,
        (role == static_cast<uint32_t>(EnemyKind::Boss) ? 1.15f : 0.62f) + action * 0.55f);

    if (role == static_cast<uint32_t>(EnemyKind::Brute)) {
        AddBlade(geo, Add(position, Add(Scale(right, r * (0.76f + swing * 0.18f)), Vec3{0.0f, r * (0.90f + action * 0.20f), 0.0f})), right, r * (0.64f + action * 0.18f), accentMaterial);
        AddEllipsoid(geo, Add(position, Add(Scale(dir, r * 0.34f), Vec3{0.0f, r * 0.20f, 0.0f})), Vec3{r * 0.13f, r * 0.050f, r * 0.10f}, accentMaterial, 8, 3);
    } else if (role == static_cast<uint32_t>(EnemyKind::Caster)) {
        AddEllipsoid(geo, Add(position, Add(Scale(dir, r * 0.24f), Vec3{0.0f, r * (h + 0.70f + action * 0.18f), 0.0f})), Vec3{r * (0.16f + action * 0.06f), r * (0.14f + action * 0.05f), r * (0.14f + action * 0.04f)}, accentMaterial, 8, 3);
        AddEllipsoid(geo, Add(position, Add(Scale(right, r * 0.26f), Vec3{0.0f, r * (h + 0.38f + action * 0.08f), 0.0f})), Vec3{r * 0.080f, r * 0.14f, r * 0.070f}, accentMaterial, 8, 3);
        AddEllipsoid(geo, Add(position, Add(Scale(right, -r * 0.26f), Vec3{0.0f, r * (h + 0.32f + action * 0.08f), 0.0f})), Vec3{r * 0.080f, r * 0.13f, r * 0.070f}, accentMaterial, 8, 3);
    } else if (role == static_cast<uint32_t>(EnemyKind::Skirmisher)) {
        AddBlade(geo, Add(position, Add(Scale(right, r * 0.50f), Vec3{0.0f, r * (0.72f + action * 0.12f), 0.0f})), Normalize(Add(dir, Scale(right, 0.56f))), r * 0.46f, accentMaterial);
        AddBlade(geo, Add(position, Add(Scale(right, -r * 0.50f), Vec3{0.0f, r * (0.68f + action * 0.12f), 0.0f})), Normalize(Add(dir, Scale(right, -0.56f))), r * 0.42f, accentMaterial);
        AddEllipsoid(geo, Add(position, Add(Scale(right, r * 0.34f), Vec3{0.0f, r * 0.34f, 0.0f})), Vec3{r * 0.070f, r * 0.12f, r * 0.060f}, accentMaterial, 8, 3);
        AddEllipsoid(geo, Add(position, Add(Scale(right, -r * 0.34f), Vec3{0.0f, r * 0.30f, 0.0f})), Vec3{r * 0.065f, r * 0.11f, r * 0.055f}, accentMaterial, 8, 3);
    } else if (role == static_cast<uint32_t>(EnemyKind::Bulwark)) {
        AddVerticalPanel(geo, Add(position, Scale(dir, r * (0.56f + action * 0.14f))), dir, r * 0.80f, r * (1.12f + action * 0.18f), accentMaterial, r * 0.12f);
        AddEllipsoid(geo, Add(position, Add(Scale(dir, r * 0.68f), Vec3{0.0f, r * 0.20f, 0.0f})), Vec3{r * 0.35f, r * 0.060f, r * 0.060f}, accentMaterial, 8, 3);
    } else if (role == static_cast<uint32_t>(EnemyKind::Boss)) {
        AddEllipsoid(geo, Vec3{position.x, baseY + r * (h + 0.68f), position.z}, Vec3{r * (0.22f + action * 0.04f), r * (0.24f + action * 0.05f), r * 0.20f}, accentMaterial, 10, 4);
        AddOrientedPrism(geo, Vec3{position.x, baseY + r * (h + 0.98f), position.z}, dir, r * 0.34f, r * 0.040f, r * (0.15f + action * 0.04f), accentMaterial);
        AddBlade(geo, Add(position, Add(Scale(right, r * 0.36f), Vec3{0.0f, r * (0.48f + action * 0.08f), 0.0f})), Normalize(Add(dir, Scale(right, 0.54f))), r * (0.58f + action * 0.14f), accentMaterial);
        AddBlade(geo, Add(position, Add(Scale(right, -r * 0.34f), Vec3{0.0f, r * (0.42f + action * 0.08f), 0.0f})), Normalize(Add(dir, Scale(right, -0.50f))), r * (0.52f + action * 0.12f), accentMaterial);
    }
}

GeneratedRTGeometry GenerateRTGeometry(std::span<const EntityRTProxy> proxies) {
    GeneratedRTGeometry geo{};
    geo.triangles.reserve(proxies.size() * 18);

    for (const EntityRTProxy& proxy : proxies) {
        switch (proxy.kind) {
        case EntityProxyKind::PlayerCore:
            AddHumanoidFigure(
                geo,
                proxy.position,
                proxy.direction,
                proxy.radius * 1.12f,
                proxy.materialId,
                kMaterialPlayerBlade,
                proxy.phase,
                proxy.visualTag);
            break;
        case EntityProxyKind::PlayerBlade:
            AddBlade(geo, proxy.position, proxy.direction, proxy.radius, proxy.materialId);
            break;
        case EntityProxyKind::PlayerPrimaryGuide:
            AddBrokenTellBurst(geo, proxy.position, proxy.direction, proxy.radius, proxy.phase, proxy.materialId, ElementFromMaterialId(proxy.materialId));
            break;
        case EntityProxyKind::PlayerAbilityLine:
            AddGuideStrip(geo, proxy.position, proxy.direction, proxy.radius * 0.72f, 0.014f, proxy.materialId);
            AddBrokenTellBurst(geo, proxy.position, proxy.direction, proxy.radius * 0.22f, proxy.phase, proxy.materialId, ElementFromMaterialId(proxy.materialId));
            break;
        case EntityProxyKind::PlayerAbilityRing:
            AddBrokenTellBurst(geo, proxy.position, proxy.direction, proxy.radius, proxy.phase, proxy.materialId, ElementFromMaterialId(proxy.materialId));
            break;
        case EntityProxyKind::PlayerAbilityCooldown:
            AddBrokenTellBurst(geo, proxy.position, proxy.direction, proxy.radius, proxy.phase, proxy.materialId, ElementFromMaterialId(proxy.materialId));
            AddBrokenTellBurst(geo, Vec3{proxy.position.x, proxy.position.y + 0.010f, proxy.position.z}, proxy.direction, proxy.radius * 0.62f, proxy.phase, proxy.materialId, ElementFromMaterialId(proxy.materialId));
            break;
        case EntityProxyKind::PlayerActionCone:
            AddWeaponActionParticleField(geo, proxy.position, proxy.direction, proxy.radius, proxy.phase, proxy.materialId, proxy.visualTag, proxy.kind);
            AddElementalParticleField(
                geo,
                proxy.position,
                proxy.direction,
                proxy.radius * 0.34f,
                ActionVisualTagElement(proxy.visualTag),
                proxy.phase,
                0.72f);
            break;
        case EntityProxyKind::PlayerActionLine:
            AddWeaponActionParticleField(geo, proxy.position, proxy.direction, proxy.radius, proxy.phase, proxy.materialId, proxy.visualTag, proxy.kind);
            AddElementalParticleField(
                geo,
                Add(proxy.position, Scale(proxy.direction, proxy.radius * 0.22f)),
                proxy.direction,
                proxy.radius * 0.24f,
                ActionVisualTagElement(proxy.visualTag),
                proxy.phase,
                0.66f);
            break;
        case EntityProxyKind::PlayerActionRing:
            AddWeaponActionParticleField(geo, proxy.position, proxy.direction, proxy.radius, proxy.phase, proxy.materialId, proxy.visualTag, proxy.kind);
            AddElementalParticleField(
                geo,
                proxy.position,
                proxy.direction,
                proxy.radius * 0.30f,
                ActionVisualTagElement(proxy.visualTag),
                proxy.phase,
                0.70f);
            break;
        case EntityProxyKind::PlayerActionBurst:
            AddWeaponActionParticleField(geo, proxy.position, proxy.direction, proxy.radius, proxy.phase, proxy.materialId, proxy.visualTag, proxy.kind);
            AddElementalParticleField(
                geo,
                proxy.position,
                proxy.direction,
                proxy.radius * 0.36f,
                ActionVisualTagElement(proxy.visualTag),
                proxy.phase,
                0.82f);
            break;
        case EntityProxyKind::EnemyBrute: {
            AddEnemyFigure(geo, proxy.position, proxy.direction, proxy.radius * 1.14f, proxy.materialId, kMaterialHitSpark, 1.38f, proxy.phase, proxy.visualTag);
            break;
        }
        case EntityProxyKind::EnemyCaster: {
            AddEnemyFigure(geo, proxy.position, proxy.direction, proxy.radius * 1.12f, proxy.materialId, kMaterialRoomPulse, 1.72f, proxy.phase, proxy.visualTag);
            AddOctahedron(geo, Vec3{proxy.position.x, proxy.position.y + proxy.radius * 2.00f, proxy.position.z}, proxy.radius * 0.22f, kMaterialPlayerBlade);
            break;
        }
        case EntityProxyKind::EnemySkirmisher: {
            AddEnemyFigure(geo, proxy.position, proxy.direction, proxy.radius * 1.12f, proxy.materialId, kMaterialControl, 1.24f, proxy.phase, proxy.visualTag);
            break;
        }
        case EntityProxyKind::EnemyBulwark: {
            AddEnemyFigure(geo, proxy.position, proxy.direction, proxy.radius * 1.10f, proxy.materialId, kMaterialProjectile, 1.16f, proxy.phase, proxy.visualTag);
            const Vec3 front = Add(proxy.position, Scale(FlatDirection(proxy.direction), proxy.radius * 0.58f));
            AddVerticalPanel(geo, front, proxy.direction, proxy.radius * 0.82f, proxy.radius * 1.32f, kMaterialEnemyBulwark, proxy.radius * 0.18f);
            break;
        }
        case EntityProxyKind::EnemyBoss: {
            AddEnemyFigure(geo, proxy.position, proxy.direction, proxy.radius * 1.14f, proxy.materialId, kMaterialHitSpark, 1.48f, proxy.phase, proxy.visualTag);
            AddEllipsoid(geo, Vec3{proxy.position.x, proxy.position.y + proxy.radius * 2.30f, proxy.position.z}, Vec3{proxy.radius * 0.24f, proxy.radius * 0.22f, proxy.radius * 0.20f}, kMaterialEnemyBoss, 10, 4);
            AddOrientedPrism(geo, Vec3{proxy.position.x, proxy.position.y + proxy.radius * 2.66f, proxy.position.z}, proxy.direction, proxy.radius * 0.38f, proxy.radius * 0.045f, proxy.radius * 0.18f, kMaterialEnemyBoss);
            break;
        }
        case EntityProxyKind::EnemyHealthBack:
        case EntityProxyKind::EnemyHealthFill:
        case EntityProxyKind::EnemyShieldFill:
            AddEnemyReadabilityStrip(geo, proxy.position, proxy.direction, proxy.radius, proxy.materialId);
            break;
        case EntityProxyKind::EnemyStatusPip:
            AddOctahedron(geo, proxy.position, proxy.radius, proxy.materialId);
            AddElementalParticleField(
                geo,
                proxy.position,
                proxy.direction,
                proxy.radius * 2.40f,
                proxy.visualTag != 0u ? ActionVisualTagElement(proxy.visualTag) : ElementFromMaterialId(proxy.materialId),
                proxy.phase,
                0.36f);
            break;
        case EntityProxyKind::EnemyTellCone:
            AddConeTellShards(geo, proxy.position, proxy.direction, proxy.radius, proxy.phase, proxy.materialId, proxy.visualTag != 0u ? ActionVisualTagElement(proxy.visualTag) : ElementFromMaterialId(proxy.materialId));
            AddElementalParticleField(
                geo,
                proxy.position,
                proxy.direction,
                proxy.radius * 0.28f,
                proxy.visualTag != 0u ? ActionVisualTagElement(proxy.visualTag) : ElementFromMaterialId(proxy.materialId),
                proxy.phase,
                0.78f);
            break;
        case EntityProxyKind::EnemyTellLine:
            AddGuideStrip(geo, Add(proxy.position, Scale(proxy.direction, proxy.radius * 0.34f)), proxy.direction, proxy.radius * 0.62f, 0.006f + proxy.phase * 0.004f, proxy.materialId);
            AddBrokenTellBurst(geo, proxy.position, proxy.direction, proxy.radius * 0.13f, proxy.phase, proxy.materialId, proxy.visualTag != 0u ? ActionVisualTagElement(proxy.visualTag) : ElementFromMaterialId(proxy.materialId));
            AddElementalParticleField(
                geo,
                Add(proxy.position, Scale(proxy.direction, proxy.radius * 0.24f)),
                proxy.direction,
                proxy.radius * 0.20f,
                proxy.visualTag != 0u ? ActionVisualTagElement(proxy.visualTag) : ElementFromMaterialId(proxy.materialId),
                proxy.phase,
                0.72f);
            break;
        case EntityProxyKind::EnemyTellRing:
            AddElementalParticleField(
                geo,
                proxy.position,
                proxy.direction,
                proxy.radius * 0.15f,
                proxy.visualTag != 0u ? ActionVisualTagElement(proxy.visualTag) : ElementFromMaterialId(proxy.materialId),
                proxy.phase,
                0.42f);
            break;
        case EntityProxyKind::Projectile:
            AddOctahedron(geo, proxy.position, proxy.radius * 0.58f, proxy.materialId);
            AddBlade(geo, Add(proxy.position, Scale(proxy.direction, proxy.radius * 0.18f)), proxy.direction, proxy.radius * 0.32f, proxy.materialId);
            AddElementalParticleField(
                geo,
                proxy.position,
                proxy.direction,
                proxy.radius * 1.10f,
                proxy.visualTag != 0u ? ActionVisualTagElement(proxy.visualTag) : ElementFromMaterialId(proxy.materialId),
                proxy.phase,
                0.82f);
            break;
        case EntityProxyKind::ProjectileTrail:
            AddBlade(geo, proxy.position, proxy.direction, proxy.radius, proxy.materialId);
            AddElementalParticleField(
                geo,
                proxy.position,
                proxy.direction,
                proxy.radius * 0.80f,
                proxy.visualTag != 0u ? ActionVisualTagElement(proxy.visualTag) : ElementFromMaterialId(proxy.materialId),
                proxy.phase,
                0.64f);
            break;
        case EntityProxyKind::ControlSpell:
            AddBrokenTellBurst(geo, proxy.position, proxy.direction, proxy.radius, proxy.phase, proxy.materialId, ElementFromMaterialId(proxy.materialId));
            break;
        case EntityProxyKind::HitSpark:
            AddOctahedron(geo, proxy.position, proxy.radius, proxy.materialId);
            AddElementalParticleField(geo, proxy.position, proxy.direction, proxy.radius * 1.35f, ElementFromMaterialId(proxy.materialId), proxy.phase, 0.55f);
            break;
        case EntityProxyKind::RoomClearPulse:
            AddBrokenTellBurst(geo, proxy.position, proxy.direction, proxy.radius, proxy.phase, proxy.materialId, ElementFromMaterialId(proxy.materialId));
            break;
        case EntityProxyKind::PortalPulse:
            AddBrokenTellBurst(geo, proxy.position, proxy.direction, proxy.radius, proxy.phase, proxy.materialId, ElementFromMaterialId(proxy.materialId));
            AddOctahedron(geo, Add(proxy.position, Vec3{0.0f, proxy.radius * 0.18f, 0.0f}), proxy.radius * 0.22f, proxy.materialId);
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
        packed.triangleMetadata.push_back(RtTriangleMetadata{tri.a.normal, tri.materialId, tri.styleTag});
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
        h = HashMix(h, triangle.styleTag);
    }
    return h;
}

}
