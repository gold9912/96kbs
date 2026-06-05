#pragma once

#include "game/math.h"

#include <array>
#include <cstdint>

namespace rogue {

constexpr uint32_t kDefaultSeed = 0x0000964bu;
constexpr int kMaxRooms = 8;
constexpr int kMaxPortals = 12;
constexpr int kMaxSpawns = 24;

enum class RoomLifecycle : uint8_t {
    Locked,
    Available,
    Active,
    Completed
};

enum class RoomObjectiveKind : uint8_t {
    KillAll,
    SurviveTimer,
    CustomPlaceholder
};

struct RoomObjective {
    RoomObjectiveKind kind = RoomObjectiveKind::KillAll;
    float targetSeconds = 0.0f;
    float elapsedSeconds = 0.0f;
    bool completed = false;
};

struct EngineConfig {
    uint32_t seed = kDefaultSeed;
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t renderQuality = 2;
    bool debugText = true;
    bool requireDxr = true;
};

struct Room {
    Vec2 center{};
    Vec2 halfSize{};
    int firstPortal = 0;
    int portalCount = 0;
    bool locked = true;
    bool cleared = false;
    RoomLifecycle lifecycle = RoomLifecycle::Locked;
    RoomObjective objective{};
};

struct Portal {
    int a = -1;
    int b = -1;
    Vec2 position{};
    float radius = 1.5f;
    bool open = false;
};

struct SpawnPoint {
    Vec2 position{};
    int roomIndex = 0;
    int archetype = 0;
};

struct SdfRoomParams {
    Vec2 center{};
    Vec2 halfSize{};
    float portalRadius = 1.5f;
    float materialSeed = 0.0f;
};

struct RoomGraph {
    uint32_t seed = 0;
    int roomCount = 0;
    int portalCount = 0;
    int spawnCount = 0;
    std::array<Room, kMaxRooms> rooms{};
    std::array<Portal, kMaxPortals> portals{};
    std::array<SpawnPoint, kMaxSpawns> spawns{};
    std::array<SdfRoomParams, kMaxRooms> sdfRooms{};
};

RoomGraph GenerateWorld(uint32_t seed);
uint32_t HashWorld(const RoomGraph& world);
bool PointInsideRoom(const Room& room, Vec2 p);
int FindRoomAt(const RoomGraph& world, Vec2 p);

}
