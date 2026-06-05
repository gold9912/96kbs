#include "game/game_session.h"

#include <cmath>

namespace rogue {

namespace {

uint32_t HashMix(uint32_t h, uint32_t v) {
    h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}

uint32_t FloatEventHash(float value) {
    return static_cast<uint32_t>(static_cast<int>(std::round(value * 100.0f)));
}

}

void GameSession::Start(uint32_t seed, int floorIndex) {
    seed_ = seed;
    floorIndex_ = floorIndex;
    currentRoom_ = 0;
    status_ = RunStatus::InProgress;
    world_ = GenerateWorld(seed);
    combat_.Reset(world_);
    eventHash_ = HashMix(seed, static_cast<uint32_t>(floorIndex));
    totalKills_ = 0;
    ClearTickEvents();
}

const GameSessionTickResult& GameSession::Tick(const InputState& input, float dt) {
    ClearTickEvents();
    if (status_ != RunStatus::InProgress) {
        return lastTick_;
    }

    const PlayerState previousPlayer = combat_.Player();
    const auto previousEnemies = combat_.Enemies();

    combat_.Tick(input, dt, world_);
    CollectCombatEvents(previousPlayer, previousEnemies);

    if (combat_.Player().hp <= 0.0f) {
        status_ = RunStatus::Failed;
        Emit(GameEventType::RunFailed, currentRoom_);
        return lastTick_;
    }

    const int playerRoom = combat_.Player().roomIndex;
    if (playerRoom != currentRoom_ && IsEnterable(playerRoom)) {
        EnterRoom(playerRoom, false);
    }

    UpdateActiveObjective(dt);
    return lastTick_;
}

bool GameSession::TryEnterRoom(int roomIndex) {
    ClearTickEvents();
    if (status_ != RunStatus::InProgress || !IsEnterable(roomIndex)) {
        return false;
    }
    return EnterRoom(roomIndex, true);
}

int GameSession::DamageRoomEnemies(int roomIndex, float damage) {
    ClearTickEvents();
    if (status_ != RunStatus::InProgress || roomIndex < 0 || roomIndex >= world_.roomCount) {
        return 0;
    }

    const PlayerState previousPlayer = combat_.Player();
    const auto previousEnemies = combat_.Enemies();
    const int killed = combat_.DamageEnemiesInRoom(roomIndex, damage);
    CollectCombatEvents(previousPlayer, previousEnemies);
    if (combat_.Player().hp <= 0.0f) {
        status_ = RunStatus::Failed;
        Emit(GameEventType::RunFailed, currentRoom_);
        return killed;
    }
    UpdateActiveObjective(0.0f);
    return killed;
}

CombatSnapshot GameSession::Snapshot() const {
    CombatSnapshot snapshot = combat_.Snapshot(world_);
    snapshot.currentRoom = currentRoom_;
    if (currentRoom_ >= 0 && currentRoom_ < world_.roomCount) {
        snapshot.currentRoomLifecycle = world_.rooms[currentRoom_].lifecycle;
    }
    snapshot.totalKills = totalKills_;
    snapshot.floorComplete = status_ == RunStatus::FloorComplete;
    return snapshot;
}

void GameSession::ClearTickEvents() {
    lastTick_ = GameSessionTickResult{};
    lastTick_.eventHash = eventHash_;
}

void GameSession::Emit(GameEventType type, int roomIndex, int entityIndex, float value) {
    if (type == GameEventType::EnemyKilled) {
        ++totalKills_;
    }
    if (lastTick_.eventCount < kMaxGameEvents) {
        GameEvent& event = lastTick_.events[lastTick_.eventCount++];
        event.type = type;
        event.roomIndex = roomIndex;
        event.entityIndex = entityIndex;
        event.value = value;
    }

    eventHash_ = HashMix(eventHash_, static_cast<uint32_t>(type));
    eventHash_ = HashMix(eventHash_, static_cast<uint32_t>(roomIndex + 1024));
    eventHash_ = HashMix(eventHash_, static_cast<uint32_t>(entityIndex + 1024));
    eventHash_ = HashMix(eventHash_, FloatEventHash(value));
    lastTick_.eventHash = eventHash_;
}

bool GameSession::EnterRoom(int roomIndex, bool placePlayerAtRoomCenter) {
    if (!IsEnterable(roomIndex)) {
        return false;
    }

    if (placePlayerAtRoomCenter) {
        combat_.PlacePlayer(world_.rooms[roomIndex].center, roomIndex);
    }
    if (currentRoom_ != roomIndex) {
        currentRoom_ = roomIndex;
        Emit(GameEventType::RoomEntered, roomIndex);
    }

    if (world_.rooms[roomIndex].lifecycle == RoomLifecycle::Available) {
        ActivateRoom(roomIndex);
    }
    return true;
}

void GameSession::ActivateRoom(int roomIndex) {
    Room& room = world_.rooms[roomIndex];
    if (room.lifecycle != RoomLifecycle::Available) {
        return;
    }

    room.lifecycle = RoomLifecycle::Active;
    room.locked = false;
    room.cleared = false;
    room.objective.completed = false;
    room.objective.elapsedSeconds = 0.0f;
    Emit(GameEventType::RoomActivated, roomIndex);

    combat_.ActivateEnemiesInRoom(roomIndex);
    const auto& enemies = combat_.Enemies();
    for (int i = 0; i < kMaxEnemies; ++i) {
        const EnemyState& enemy = enemies[i];
        if (enemy.roomIndex == roomIndex && enemy.active && enemy.hp > 0.0f) {
            Emit(GameEventType::EnemyActivated, roomIndex, i);
        }
    }
}

void GameSession::UpdateActiveObjective(float dt) {
    if (currentRoom_ < 0 || currentRoom_ >= world_.roomCount) {
        return;
    }

    Room& room = world_.rooms[currentRoom_];
    if (room.lifecycle != RoomLifecycle::Active) {
        return;
    }

    bool completed = false;
    switch (room.objective.kind) {
    case RoomObjectiveKind::KillAll:
    case RoomObjectiveKind::CustomPlaceholder:
        completed = combat_.LivingEnemiesInRoom(currentRoom_) == 0;
        break;
    case RoomObjectiveKind::SurviveTimer:
        room.objective.elapsedSeconds += dt;
        completed = room.objective.elapsedSeconds >= room.objective.targetSeconds;
        break;
    }

    if (completed) {
        CompleteRoom(currentRoom_);
    }
}

void GameSession::CompleteRoom(int roomIndex) {
    Room& room = world_.rooms[roomIndex];
    if (room.lifecycle == RoomLifecycle::Completed) {
        return;
    }

    room.lifecycle = RoomLifecycle::Completed;
    room.locked = false;
    room.cleared = true;
    room.objective.completed = true;
    Emit(GameEventType::ObjectiveCompleted, roomIndex);
    Emit(GameEventType::RoomCompleted, roomIndex);
    OpenPortalsFromRoom(roomIndex);

    if (roomIndex == world_.roomCount - 1) {
        status_ = RunStatus::FloorComplete;
        Emit(GameEventType::FloorCompleted, roomIndex);
    }
}

void GameSession::OpenPortalsFromRoom(int roomIndex) {
    for (int i = 0; i < world_.portalCount; ++i) {
        Portal& portal = world_.portals[i];
        if (portal.a != roomIndex && portal.b != roomIndex) {
            continue;
        }

        if (!portal.open) {
            portal.open = true;
            Emit(GameEventType::PortalOpened, roomIndex, i);
        }

        const int other = portal.a == roomIndex ? portal.b : portal.a;
        if (other >= 0 && other < world_.roomCount && world_.rooms[other].lifecycle == RoomLifecycle::Locked) {
            world_.rooms[other].lifecycle = RoomLifecycle::Available;
            world_.rooms[other].locked = false;
        }
    }
}

void GameSession::CollectCombatEvents(
    const PlayerState& previousPlayer,
    const std::array<EnemyState, kMaxEnemies>& previousEnemies) {
    const PlayerState& player = combat_.Player();
    if (player.hp < previousPlayer.hp) {
        Emit(GameEventType::PlayerDamaged, currentRoom_, -1, previousPlayer.hp - player.hp);
    }

    const auto& enemies = combat_.Enemies();
    for (int i = 0; i < kMaxEnemies; ++i) {
        const EnemyState& before = previousEnemies[i];
        const EnemyState& after = enemies[i];
        if (before.hp > 0.0f && after.hp <= 0.0f) {
            Emit(GameEventType::EnemyKilled, after.roomIndex, i);
        }
        if (!before.active && after.active && after.hp > 0.0f) {
            Emit(GameEventType::EnemyActivated, after.roomIndex, i);
        }
    }
}

bool GameSession::IsEnterable(int roomIndex) const {
    if (roomIndex < 0 || roomIndex >= world_.roomCount) {
        return false;
    }
    return world_.rooms[roomIndex].lifecycle != RoomLifecycle::Locked;
}

}
