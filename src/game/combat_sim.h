#pragma once

#include "game/math.h"
#include "game/world_gen.h"

#include <array>
#include <cstdint>

namespace rogue {

constexpr int kMaxEnemies = 24;
constexpr int kMaxProjectiles = 32;

struct InputState {
    float moveX = 0.0f;
    float moveY = 0.0f;
    float aimX = 1.0f;
    float aimY = 0.0f;
    bool melee = false;
    bool ranged = false;
    bool control = false;
    bool dash = false;
};

struct PlayerState {
    Vec2 position{};
    Vec2 facing{1.0f, 0.0f};
    float hp = 100.0f;
    float meleeCooldown = 0.0f;
    float rangedCooldown = 0.0f;
    float controlCooldown = 0.0f;
    float dashCooldown = 0.0f;
    int roomIndex = 0;
};

enum class EnemyKind : uint8_t {
    Brute,
    Caster
};

struct EnemyState {
    Vec2 position{};
    Vec2 velocity{};
    float hp = 0.0f;
    float attackCooldown = 0.0f;
    int roomIndex = 0;
    EnemyKind kind = EnemyKind::Brute;
    bool active = false;
};

struct ProjectileState {
    Vec2 position{};
    Vec2 velocity{};
    float damage = 0.0f;
    float radius = 0.0f;
    float ttl = 0.0f;
    int roomIndex = 0;
    bool active = false;
};

struct CombatSnapshot {
    PlayerState player{};
    int activeEnemies = 0;
    int activeProjectiles = 0;
    int totalKills = 0;
    int currentRoom = 0;
    RoomLifecycle currentRoomLifecycle = RoomLifecycle::Locked;
    bool floorComplete = false;
};

class CombatSim {
public:
    void Reset(const RoomGraph& world);
    void Tick(const InputState& input, float dt, RoomGraph& world);

    const PlayerState& Player() const { return player_; }
    const std::array<EnemyState, kMaxEnemies>& Enemies() const { return enemies_; }
    const std::array<ProjectileState, kMaxProjectiles>& Projectiles() const { return projectiles_; }
    CombatSnapshot Snapshot(const RoomGraph& world) const;
    void PlacePlayer(Vec2 position, int roomIndex);
    int ActivateEnemiesInRoom(int roomIndex);
    int DamageEnemiesInRoom(int roomIndex, float damage);
    int ActiveEnemiesInRoom(int roomIndex) const;
    int LivingEnemiesInRoom(int roomIndex) const;
    int ActiveProjectileCount() const;

private:
    void SpawnProjectile(Vec2 position, Vec2 dir, int roomIndex);
    void ApplyMelee(Vec2 dir);
    void ApplyControlSpell();
    void UpdateProjectiles(float dt);
    void UpdateEnemies(float dt);

    PlayerState player_{};
    std::array<EnemyState, kMaxEnemies> enemies_{};
    std::array<ProjectileState, kMaxProjectiles> projectiles_{};
};

}
