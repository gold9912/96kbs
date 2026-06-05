#include "game/world_gen.h"

#include "game/rng.h"

#include <cmath>

namespace rogue {

namespace {

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
    portal.radius = 1.55f;
    portal.open = a == 0;
}

void AddSpawn(RoomGraph& world, XorShift32& rng, int roomIndex, int archetype) {
    if (world.spawnCount >= kMaxSpawns) {
        return;
    }

    const Room& room = world.rooms[roomIndex];
    const float ox = (rng.NextFloat() * 2.0f - 1.0f) * (room.halfSize.x - 2.0f);
    const float oy = (rng.NextFloat() * 2.0f - 1.0f) * (room.halfSize.y - 2.0f);

    SpawnPoint& spawn = world.spawns[world.spawnCount++];
    spawn.position = room.center + Vec2{ox, oy};
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
        objective.kind = RoomObjectiveKind::CustomPlaceholder;
        break;
    }
    return objective;
}

}

RoomGraph GenerateWorld(uint32_t seed) {
    XorShift32 rng(seed);
    RoomGraph world{};
    world.seed = seed;
    world.roomCount = 6;

    const Vec2 gridStep{17.0f, 12.0f};
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
            5.8f + rng.NextFloat() * 2.3f,
            4.2f + rng.NextFloat() * 1.7f
        };
        room.locked = i != 0;
        room.cleared = i == 0;
        room.lifecycle = i == 0 ? RoomLifecycle::Completed : RoomLifecycle::Locked;
        room.objective = MakeObjective(i);

        world.sdfRooms[i].center = room.center;
        world.sdfRooms[i].halfSize = room.halfSize;
        world.sdfRooms[i].portalRadius = 1.55f;
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
        const int spawnCount = 1 + (i % 3);
        for (int s = 0; s < spawnCount; ++s) {
            AddSpawn(world, rng, i, (s + i) % 2);
        }
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

bool PointInsideRoom(const Room& room, Vec2 p) {
    const Vec2 d{std::abs(p.x - room.center.x), std::abs(p.y - room.center.y)};
    return d.x <= room.halfSize.x && d.y <= room.halfSize.y;
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
