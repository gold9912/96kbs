#include "game/combat_sim.h"

namespace rogue {

namespace {

constexpr float kPlayerSpeed = 6.0f;
constexpr float kDashSpeed = 18.0f;
constexpr float kMeleeRange = 2.45f;
constexpr float kMeleeDamage = 42.0f;
constexpr float kProjectileSpeed = 12.0f;
constexpr float kProjectileDamage = 28.0f;
constexpr float kControlRadius = 4.2f;
constexpr float kControlDamage = 18.0f;

float EnemyMaxHp(EnemyKind kind) {
    return kind == EnemyKind::Brute ? 82.0f : 54.0f;
}

float EnemySpeed(EnemyKind kind) {
    return kind == EnemyKind::Brute ? 2.35f : 1.85f;
}

float EnemyAttackRange(EnemyKind kind) {
    return kind == EnemyKind::Brute ? 1.35f : 5.0f;
}

float EnemyDamage(EnemyKind kind) {
    return kind == EnemyKind::Brute ? 13.0f : 8.0f;
}

}

void CombatSim::Reset(const RoomGraph& world) {
    player_ = PlayerState{};
    player_.position = world.rooms[0].center;
    player_.roomIndex = 0;

    enemies_ = {};
    projectiles_ = {};

    for (int i = 0; i < world.spawnCount && i < kMaxEnemies; ++i) {
        const SpawnPoint& spawn = world.spawns[i];
        EnemyState& enemy = enemies_[i];
        enemy.position = spawn.position;
        enemy.roomIndex = spawn.roomIndex;
        enemy.kind = spawn.archetype == 0 ? EnemyKind::Brute : EnemyKind::Caster;
        enemy.hp = EnemyMaxHp(enemy.kind);
        enemy.active = false;
    }
}

void CombatSim::Tick(const InputState& input, float dt, RoomGraph& world) {
    if (dt <= 0.0f) {
        return;
    }
    if (dt > 0.05f) {
        dt = 0.05f;
    }

    player_.meleeCooldown = Clamp(player_.meleeCooldown - dt, 0.0f, 99.0f);
    player_.rangedCooldown = Clamp(player_.rangedCooldown - dt, 0.0f, 99.0f);
    player_.controlCooldown = Clamp(player_.controlCooldown - dt, 0.0f, 99.0f);
    player_.dashCooldown = Clamp(player_.dashCooldown - dt, 0.0f, 99.0f);

    Vec2 move = NormalizeOr(Vec2{input.moveX, input.moveY}, Vec2{0.0f, 0.0f});
    Vec2 aim = NormalizeOr(Vec2{input.aimX, input.aimY}, player_.facing);
    player_.facing = aim;

    float speed = kPlayerSpeed;
    if (input.dash && player_.dashCooldown <= 0.0f) {
        speed = kDashSpeed;
        player_.dashCooldown = 0.9f;
    }
    player_.position = player_.position + move * (speed * dt);

    const int newRoom = FindRoomAt(world, player_.position);
    if (newRoom >= 0 && !world.rooms[newRoom].locked) {
        player_.roomIndex = newRoom;
    }

    if (input.melee && player_.meleeCooldown <= 0.0f) {
        ApplyMelee(aim);
        player_.meleeCooldown = 0.42f;
    }
    if (input.ranged && player_.rangedCooldown <= 0.0f) {
        SpawnProjectile(player_.position + aim * 1.0f, aim, player_.roomIndex);
        player_.rangedCooldown = 0.28f;
    }
    if (input.control && player_.controlCooldown <= 0.0f) {
        ApplyControlSpell();
        player_.controlCooldown = 3.0f;
    }

    UpdateProjectiles(dt);
    UpdateEnemies(dt);
}

CombatSnapshot CombatSim::Snapshot(const RoomGraph& world) const {
    CombatSnapshot snapshot{};
    snapshot.player = player_;
    snapshot.currentRoom = player_.roomIndex;
    snapshot.activeEnemies = ActiveEnemiesInRoom(player_.roomIndex);
    snapshot.activeProjectiles = ActiveProjectileCount();
    if (player_.roomIndex >= 0 && player_.roomIndex < world.roomCount) {
        snapshot.currentRoomLifecycle = world.rooms[player_.roomIndex].lifecycle;
    }
    snapshot.floorComplete = world.roomCount > 0 && world.rooms[world.roomCount - 1].lifecycle == RoomLifecycle::Completed;
    return snapshot;
}

void CombatSim::PlacePlayer(Vec2 position, int roomIndex) {
    player_.position = position;
    player_.roomIndex = roomIndex;
}

int CombatSim::ActivateEnemiesInRoom(int roomIndex) {
    int activated = 0;
    for (EnemyState& enemy : enemies_) {
        if (enemy.roomIndex == roomIndex && enemy.hp > 0.0f && !enemy.active) {
            enemy.active = true;
            ++activated;
        }
    }
    return activated;
}

int CombatSim::DamageEnemiesInRoom(int roomIndex, float damage) {
    int killed = 0;
    for (EnemyState& enemy : enemies_) {
        if (enemy.roomIndex != roomIndex || enemy.hp <= 0.0f) {
            continue;
        }
        enemy.hp -= damage;
        if (enemy.hp <= 0.0f) {
            enemy.active = false;
            ++killed;
        }
    }
    return killed;
}

int CombatSim::LivingEnemiesInRoom(int roomIndex) const {
    int count = 0;
    for (const EnemyState& enemy : enemies_) {
        if (enemy.roomIndex == roomIndex && enemy.hp > 0.0f) {
            ++count;
        }
    }
    return count;
}

int CombatSim::ActiveProjectileCount() const {
    int count = 0;
    for (const ProjectileState& projectile : projectiles_) {
        if (projectile.active) {
            ++count;
        }
    }
    return count;
}

void CombatSim::SpawnProjectile(Vec2 position, Vec2 dir, int roomIndex) {
    for (ProjectileState& projectile : projectiles_) {
        if (!projectile.active) {
            projectile.active = true;
            projectile.position = position;
            projectile.velocity = dir * kProjectileSpeed;
            projectile.damage = kProjectileDamage;
            projectile.radius = 0.36f;
            projectile.ttl = 1.6f;
            projectile.roomIndex = roomIndex;
            return;
        }
    }
}

void CombatSim::ApplyMelee(Vec2 dir) {
    for (EnemyState& enemy : enemies_) {
        if (!enemy.active || enemy.roomIndex != player_.roomIndex || enemy.hp <= 0.0f) {
            continue;
        }
        const Vec2 toEnemy = enemy.position - player_.position;
        if (LengthSq(toEnemy) > kMeleeRange * kMeleeRange) {
            continue;
        }
        const Vec2 enemyDir = NormalizeOr(toEnemy, dir);
        if (Dot(enemyDir, dir) > 0.15f) {
            enemy.hp -= kMeleeDamage;
            if (enemy.hp <= 0.0f) {
                enemy.active = false;
            }
        }
    }
}

void CombatSim::ApplyControlSpell() {
    for (EnemyState& enemy : enemies_) {
        if (!enemy.active || enemy.roomIndex != player_.roomIndex || enemy.hp <= 0.0f) {
            continue;
        }
        const Vec2 toEnemy = enemy.position - player_.position;
        if (LengthSq(toEnemy) <= kControlRadius * kControlRadius) {
            enemy.hp -= kControlDamage;
            enemy.velocity = enemy.velocity * 0.2f;
            if (enemy.hp <= 0.0f) {
                enemy.active = false;
            }
        }
    }
}

void CombatSim::UpdateProjectiles(float dt) {
    for (ProjectileState& projectile : projectiles_) {
        if (!projectile.active) {
            continue;
        }
        projectile.position = projectile.position + projectile.velocity * dt;
        projectile.ttl -= dt;
        if (projectile.ttl <= 0.0f) {
            projectile.active = false;
            continue;
        }

        for (EnemyState& enemy : enemies_) {
            if (!enemy.active || enemy.roomIndex != projectile.roomIndex || enemy.hp <= 0.0f) {
                continue;
            }
            const float hitRadius = projectile.radius + 0.55f;
            if (LengthSq(enemy.position - projectile.position) <= hitRadius * hitRadius) {
                enemy.hp -= projectile.damage;
                projectile.active = false;
                if (enemy.hp <= 0.0f) {
                    enemy.active = false;
                }
                break;
            }
        }
    }
}

void CombatSim::UpdateEnemies(float dt) {
    for (EnemyState& enemy : enemies_) {
        if (!enemy.active || enemy.hp <= 0.0f) {
            continue;
        }
        if (enemy.roomIndex != player_.roomIndex) {
            continue;
        }

        enemy.attackCooldown = Clamp(enemy.attackCooldown - dt, 0.0f, 99.0f);
        const Vec2 toPlayer = player_.position - enemy.position;
        const float distanceSq = LengthSq(toPlayer);
        const float attackRange = EnemyAttackRange(enemy.kind);

        if (distanceSq > attackRange * attackRange * 0.85f) {
            const Vec2 dir = NormalizeOr(toPlayer, Vec2{0.0f, 0.0f});
            enemy.velocity = dir * EnemySpeed(enemy.kind);
            enemy.position = enemy.position + enemy.velocity * dt;
        } else if (enemy.attackCooldown <= 0.0f) {
            player_.hp -= EnemyDamage(enemy.kind);
            enemy.attackCooldown = enemy.kind == EnemyKind::Brute ? 0.95f : 1.35f;
        }
    }
}

int CombatSim::ActiveEnemiesInRoom(int roomIndex) const {
    int count = 0;
    for (const EnemyState& enemy : enemies_) {
        if (enemy.active && enemy.roomIndex == roomIndex && enemy.hp > 0.0f) {
            ++count;
        }
    }
    return count;
}

}
