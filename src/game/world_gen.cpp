#include "game/world_gen.h"

#include "game/rng.h"

#include <algorithm>
#include <cmath>

namespace rogue {

namespace {

constexpr float kPi = 3.14159265359f;

uint32_t HashMix(uint32_t h, uint32_t v) {
    h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}

uint32_t FloatBitsHash(float v) {
    const int scaled = static_cast<int>(std::round(v * 100.0f));
    return static_cast<uint32_t>(scaled);
}

void AddPortal(RoomGraph& world, int a, int b) {
    if (world.portalCount >= kMaxPortals) {
        return;
    }

    const Room& ra = world.rooms[a];
    const Room& rb = world.rooms[b];
    Portal& portal = world.portals[world.portalCount++];
    portal.a = a;
    portal.b = b;
    portal.position = (ra.center + rb.center) * 0.5f;
    portal.radius = 1.75f;
    portal.open = a == 0;
}

int PlannedSpawnCount(int roomIndex, int roomCount) {
    if (roomIndex <= 0) {
        return 0;
    }
    if (roomIndex == roomCount - 1) {
        return 4;
    }
    if (roomIndex == 1 || roomIndex == 2) {
        return 3;
    }
    if (roomIndex == 3) {
        return 4;
    }
    return 5;
}

int PlannedArchetype(int roomIndex, int slot, int floorIndex) {
    const int depthShift = std::max(0, floorIndex) / 2;
    if (roomIndex == 1) {
        constexpr int pattern[] = {0, 1, 2};
        return pattern[(slot + depthShift) % 3];
    }
    if (roomIndex == 2) {
        constexpr int pattern[] = {0, 3, 1};
        return pattern[(slot + depthShift) % 3];
    }
    if (roomIndex == 3) {
        constexpr int pattern[] = {2, 0, 3, 1};
        return pattern[(slot + depthShift) % 4];
    }
    if (roomIndex == 4) {
        constexpr int pattern[] = {0, 1, 2, 3, 2};
        return pattern[(slot + depthShift) % 5];
    }
    constexpr int finalSupport[] = {0, 1, 3, 2};
    return finalSupport[(slot + depthShift) % 4];
}

void AddSpawn(RoomGraph& world, XorShift32& rng, int roomIndex, int archetype, int slot, int slotCount) {
    if (world.spawnCount >= kMaxSpawns) {
        return;
    }

    const Room& room = world.rooms[roomIndex];
    const float safeHalfX = std::max(1.0f, room.halfSize.x - 2.0f);
    const float safeHalfY = std::max(1.0f, room.halfSize.y - 2.0f);
    const float lane = static_cast<float>(slot) + 0.5f;
    const float lanes = static_cast<float>(std::max(slotCount, 1));
    const float jitter = (rng.NextFloat() - 0.5f) * 0.10f;
    const float angle = (lane / lanes + static_cast<float>(roomIndex) * 0.173f + jitter) * kPi * 2.0f;
    const float radiusScale = 0.46f + rng.NextFloat() * 0.17f;
    const float ox = std::cos(angle) * safeHalfX * radiusScale;
    const float oy = std::sin(angle) * safeHalfY * radiusScale;

    SpawnPoint& spawn = world.spawns[world.spawnCount++];
    spawn.position = room.center + Vec2{ox, oy};
    spawn.roomIndex = roomIndex;
    spawn.archetype = archetype;
}

void AddSpawnAt(RoomGraph& world, int roomIndex, Vec2 position, int archetype) {
    if (world.spawnCount >= kMaxSpawns) {
        return;
    }

    SpawnPoint& spawn = world.spawns[world.spawnCount++];
    spawn.position = position;
    spawn.roomIndex = roomIndex;
    spawn.archetype = archetype;
}

RoomObjective MakeObjective(int roomIndex) {
    RoomObjective objective{};
    if (roomIndex == 0) {
        objective.kind = RoomObjectiveKind::KillAll;
        objective.completed = true;
        return objective;
    }

    switch (roomIndex % 3) {
    case 1:
        objective.kind = RoomObjectiveKind::KillAll;
        break;
    case 2:
        objective.kind = RoomObjectiveKind::SurviveTimer;
        objective.targetSeconds = 1.25f;
        break;
    default:
        objective.kind = RoomObjectiveKind::ControlPoint;
        objective.targetSeconds = kControlObjectiveHoldSeconds;
        objective.controlRadius = kControlObjectiveRadius;
        break;
    }
    return objective;
}

Vec2 ControlPointForRoom(const Room& room, int roomIndex) {
    const float sx = (roomIndex & 1) != 0 ? 0.34f : -0.34f;
    const float sy = (roomIndex & 2) != 0 ? -0.26f : 0.26f;
    return room.center + Vec2{room.halfSize.x * sx, room.halfSize.y * sy};
}

}

RoomGraph GenerateWorld(uint32_t seed, int floorIndex) {
    const int clampedFloor = std::max(0, floorIndex);
    const uint32_t floorSeed = clampedFloor == 0
        ? seed
        : HashMix(seed, static_cast<uint32_t>(clampedFloor + 0x51f00du));
    XorShift32 rng(floorSeed);
    RoomGraph world{};
    world.seed = seed;
    world.floorIndex = clampedFloor;
    world.descent = Clamp(static_cast<float>(clampedFloor) / 5.0f, 0.0f, 1.0f);
    world.roomCount = 6;

    const Vec2 gridStep{21.0f, 14.8f};
    std::array<Vec2, kMaxRooms> grid{};
    grid[0] = Vec2{0.0f, 0.0f};

    for (int i = 1; i < world.roomCount; ++i) {
        const Vec2 previous = grid[i - 1];
        const int turn = rng.NextRange(0, 2) - 1;
        const float xStep = (i % 2 == 0 ? 1.0f : 0.75f) * gridStep.x;
        grid[i] = Vec2{previous.x + xStep, previous.y + static_cast<float>(turn) * gridStep.y};
    }

    for (int i = 0; i < world.roomCount; ++i) {
        Room& room = world.rooms[i];
        room.center = grid[i];
        room.halfSize = Vec2{
            7.1f + rng.NextFloat() * 2.8f,
            5.1f + rng.NextFloat() * 2.1f
        };
        room.locked = i != 0;
        room.cleared = i == 0;
        room.lifecycle = i == 0 ? RoomLifecycle::Completed : RoomLifecycle::Locked;
        room.objective = MakeObjective(i);
        if (i == world.roomCount - 1) {
            room.objective = RoomObjective{};
            room.objective.kind = RoomObjectiveKind::KillAll;
        }
        if (room.objective.kind == RoomObjectiveKind::ControlPoint) {
            room.objective.controlPoint = ControlPointForRoom(room, i);
        }

        world.sdfRooms[i].center = room.center;
        world.sdfRooms[i].halfSize = room.halfSize;
        world.sdfRooms[i].portalRadius = 1.75f;
        world.sdfRooms[i].materialSeed = rng.NextFloat();
    }

    for (int i = 0; i < world.roomCount - 1; ++i) {
        AddPortal(world, i, i + 1);
    }
    if (world.roomCount > 1) {
        world.rooms[1].locked = false;
        world.rooms[1].lifecycle = RoomLifecycle::Available;
    }

    for (int i = 1; i < world.roomCount; ++i) {
        const int spawnCount = PlannedSpawnCount(i, world.roomCount);
        for (int s = 0; s < spawnCount; ++s) {
            AddSpawn(world, rng, i, PlannedArchetype(i, s, clampedFloor), s, spawnCount);
        }
    }
    if (world.roomCount > 1) {
        const int finalRoom = world.roomCount - 1;
        AddSpawnAt(world, finalRoom, world.rooms[finalRoom].center, 4);
    }

    for (int p = 0; p < world.portalCount; ++p) {
        for (int r = 0; r < world.roomCount; ++r) {
            if (world.portals[p].a == r || world.portals[p].b == r) {
                Room& room = world.rooms[r];
                if (room.portalCount == 0) {
                    room.firstPortal = p;
                }
                ++room.portalCount;
            }
        }
    }

    return world;
}

uint32_t HashWorld(const RoomGraph& world) {
    uint32_t h = HashMix(0x96u, world.seed);
    h = HashMix(h, static_cast<uint32_t>(world.floorIndex));
    h = HashMix(h, FloatBitsHash(world.descent));
    h = HashMix(h, static_cast<uint32_t>(world.roomCount));
    h = HashMix(h, static_cast<uint32_t>(world.portalCount));
    h = HashMix(h, static_cast<uint32_t>(world.spawnCount));

    for (int i = 0; i < world.roomCount; ++i) {
        const Room& room = world.rooms[i];
        h = HashMix(h, FloatBitsHash(room.center.x));
        h = HashMix(h, FloatBitsHash(room.center.y));
        h = HashMix(h, FloatBitsHash(room.halfSize.x));
        h = HashMix(h, FloatBitsHash(room.halfSize.y));
        h = HashMix(h, static_cast<uint32_t>(room.lifecycle));
        h = HashMix(h, static_cast<uint32_t>(room.objective.kind));
        h = HashMix(h, FloatBitsHash(room.objective.targetSeconds));
        h = HashMix(h, FloatBitsHash(room.objective.controlPoint.x));
        h = HashMix(h, FloatBitsHash(room.objective.controlPoint.y));
        h = HashMix(h, FloatBitsHash(room.objective.controlRadius));
    }

    for (int i = 0; i < world.spawnCount; ++i) {
        const SpawnPoint& spawn = world.spawns[i];
        h = HashMix(h, FloatBitsHash(spawn.position.x));
        h = HashMix(h, FloatBitsHash(spawn.position.y));
        h = HashMix(h, static_cast<uint32_t>(spawn.roomIndex));
        h = HashMix(h, static_cast<uint32_t>(spawn.archetype));
    }

    return h;
}

int SpawnArchetypePressureCost(int archetype) {
    if (archetype >= 4) {
        return 7;
    }
    switch (archetype % 4) {
    case 0:
        return 2;
    case 1:
        return 3;
    case 2:
        return 2;
    case 3:
        return 3;
    }
    return 2;
}

int RoomSpawnPressure(const RoomGraph& world, int roomIndex) {
    int pressure = 0;
    for (int i = 0; i < world.spawnCount; ++i) {
        const SpawnPoint& spawn = world.spawns[i];
        if (spawn.roomIndex == roomIndex) {
            pressure += SpawnArchetypePressureCost(spawn.archetype);
        }
    }
    return pressure;
}

bool PointInsideRoom(const Room& room, Vec2 p) {
    const Vec2 d{std::abs(p.x - room.center.x), std::abs(p.y - room.center.y)};
    return d.x <= room.halfSize.x && d.y <= room.halfSize.y;
}

bool PointInsidePortalPath(const RoomGraph& world, const Portal& portal, Vec2 p) {
    if (portal.a < 0 || portal.a >= world.roomCount || portal.b < 0 || portal.b >= world.roomCount) {
        return false;
    }

    const Vec2 a = world.rooms[portal.a].center;
    const Vec2 b = world.rooms[portal.b].center;
    const Vec2 ab = b - a;
    const float abLenSq = LengthSq(ab);
    if (abLenSq <= 0.000001f) {
        return false;
    }

    const float t = Clamp(Dot(p - a, ab) / abLenSq, 0.0f, 1.0f);
    const Vec2 closest = a + ab * t;
    const float halfWidth = portal.radius * 0.65f;
    return LengthSq(p - closest) <= halfWidth * halfWidth;
}

bool IsTraversablePosition(const RoomGraph& world, int currentRoom, Vec2 p) {
    const int roomAtPosition = FindRoomAt(world, p);
    if (roomAtPosition >= 0) {
        if (roomAtPosition == currentRoom) {
            return true;
        }
        if (world.rooms[roomAtPosition].locked ||
            world.rooms[roomAtPosition].lifecycle == RoomLifecycle::Locked) {
            return false;
        }

        for (int i = 0; i < world.portalCount; ++i) {
            const Portal& portal = world.portals[i];
            if (!portal.open) {
                continue;
            }
            if ((portal.a == currentRoom && portal.b == roomAtPosition) ||
                (portal.b == currentRoom && portal.a == roomAtPosition)) {
                return true;
            }
        }
        return false;
    }

    for (int i = 0; i < world.portalCount; ++i) {
        const Portal& portal = world.portals[i];
        if (!portal.open || (portal.a != currentRoom && portal.b != currentRoom)) {
            continue;
        }
        if (PointInsidePortalPath(world, portal, p)) {
            return true;
        }
    }

    return false;
}

int FindRoomAt(const RoomGraph& world, Vec2 p) {
    for (int i = 0; i < world.roomCount; ++i) {
        if (PointInsideRoom(world.rooms[i], p)) {
            return i;
        }
    }
    return -1;
}

}
