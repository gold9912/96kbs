#pragma once

#include "game/combat_sim.h"
#include "game/world_gen.h"

#include <array>
#include <cstdint>

namespace rogue {

constexpr int kMaxGameEvents = 64;

enum class RunStatus : uint8_t {
    InProgress,
    Failed,
    FloorComplete
};

enum class GameEventType : uint8_t {
    RoomEntered,
    RoomActivated,
    EnemyActivated,
    EnemyKilled,
    PlayerDamaged,
    ObjectiveCompleted,
    RoomCompleted,
    PortalOpened,
    RunFailed,
    FloorCompleted
};

struct GameEvent {
    GameEventType type = GameEventType::RoomEntered;
    int roomIndex = -1;
    int entityIndex = -1;
    float value = 0.0f;
};

struct GameSessionTickResult {
    std::array<GameEvent, kMaxGameEvents> events{};
    int eventCount = 0;
    uint32_t eventHash = 0;
};

class GameSession {
public:
    void Start(uint32_t seed, int floorIndex = 0);
    const GameSessionTickResult& Tick(const InputState& input, float dt);

    bool TryEnterRoom(int roomIndex);
    int DamageRoomEnemies(int roomIndex, float damage);

    const RoomGraph& World() const { return world_; }
    const CombatSim& Combat() const { return combat_; }
    RunStatus Status() const { return status_; }
    uint32_t Seed() const { return seed_; }
    int FloorIndex() const { return floorIndex_; }
    int CurrentRoom() const { return currentRoom_; }
    uint32_t EventHash() const { return eventHash_; }
    const GameSessionTickResult& LastTick() const { return lastTick_; }
    CombatSnapshot Snapshot() const;

private:
    void ClearTickEvents();
    void Emit(GameEventType type, int roomIndex = -1, int entityIndex = -1, float value = 0.0f);
    bool EnterRoom(int roomIndex, bool placePlayerAtRoomCenter);
    void ActivateRoom(int roomIndex);
    void UpdateActiveObjective(float dt);
    void CompleteRoom(int roomIndex);
    void OpenPortalsFromRoom(int roomIndex);
    void CollectCombatEvents(
        const PlayerState& previousPlayer,
        const std::array<EnemyState, kMaxEnemies>& previousEnemies);
    bool IsEnterable(int roomIndex) const;

    uint32_t seed_ = kDefaultSeed;
    int floorIndex_ = 0;
    int currentRoom_ = 0;
    RunStatus status_ = RunStatus::InProgress;
    RoomGraph world_{};
    CombatSim combat_{};
    GameSessionTickResult lastTick_{};
    uint32_t eventHash_ = 0;
    int totalKills_ = 0;
};

}
