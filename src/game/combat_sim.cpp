#include "game/combat_sim.h"

#include <algorithm>
#include <cmath>

namespace rogue {

namespace {

constexpr float kPlayerSpeed = 6.0f;
constexpr float kDashSpeed = 18.0f;
constexpr float kEnemyAreaDamageScale = 0.32f;
constexpr float kEnemyProjectileDamageScale = 0.38f;
constexpr float kEnemySkirmisherDamageScale = 0.30f;
constexpr float kEnemyBulwarkDamageScale = 0.28f;
constexpr float kBossDamageScale = 0.30f;
constexpr float kChargedDischargeRadius = 6.2f;
constexpr float kChargedDischargeLaneRadius = 0.85f;
constexpr float kChargedDischargeDamageScale = 0.35f;
constexpr float kElementalMicroExplosionRadius = 2.75f;
constexpr float kWetIceChillIntensityBonus = 0.20f;
constexpr float kWetAirSpreadRadius = 3.4f;
constexpr float kStunSequenceSeconds = 1.6f;
constexpr float kStunPulseInterval = 1.0f;
constexpr float kStunPulseSeconds = 0.10f;
constexpr float kPi = 3.14159265359f;

struct EnemyPressureProfile {
    float speed = 2.0f;
    float preferredRange = 2.0f;
    float retreatRange = 0.0f;
    float attackRange = 2.0f;
    float cooldownScale = 1.0f;
    float damageScale = kEnemyAreaDamageScale;
    WeaponActionIndex actionIndex = WeaponActionIndex::Action1;
    bool projectile = false;
    bool harassStep = false;
};

float HpRatio(const EnemyState& enemy) {
    return enemy.maxHp > 0.0001f ? Clamp(enemy.hp / enemy.maxHp, 0.0f, 1.0f) : 0.0f;
}

WeaponActionSpec Action(
    AttackShape shape,
    float damage,
    float cooldown,
    float range,
    float radius,
    float speed,
    float duration,
    float coneDot,
    int projectileCount,
    bool piercing = false,
    bool rooted = false,
    bool invulnerableDash = false,
    float windup = 0.0f,
    float impactTime = 0.0f,
    float recovery = 0.0f,
    float vfxDuration = 0.0f) {
    return WeaponActionSpec{
        shape,
        damage,
        cooldown,
        range,
        radius,
        speed,
        duration,
        coneDot,
        projectileCount,
        piercing,
        rooted,
        invulnerableDash,
        windup,
        impactTime,
        recovery,
        vfxDuration
    };
}

const std::array<WeaponSpec, static_cast<std::size_t>(WeaponId::Count)> kWeaponSpecs{{
    WeaponSpec{WeaponId::Hammer, WeaponCategory::Melee, "Hammer", {"Smash", "Spin"}, 0x6810u, {
        Action(AttackShape::Circle, 54.0f, 0.72f, 1.3f, 2.15f, 0.0f, 0.0f, -1.0f, 0, false, false, false, 0.10f, 0.16f, 0.18f, 0.44f),
        Action(AttackShape::Circle, 36.0f, 1.08f, 0.0f, 3.35f, 0.0f, 0.35f, -1.0f, 0, false, false, false, 0.12f, 0.22f, 0.34f, 0.70f)
    }},
    WeaponSpec{WeaponId::Spear, WeaponCategory::Melee, "Spear", {"Thrust", "Dash"}, 0x5411u, {
        Action(AttackShape::Cone, 40.0f, 0.38f, 3.35f, 0.75f, 0.0f, 0.0f, 0.78f, 0, false, false, false, 0.04f, 0.0f, 0.08f, 0.24f),
        Action(AttackShape::Dash, 32.0f, 0.90f, 4.40f, 0.82f, 0.0f, 0.20f, 0.70f, 0, false, false, true, 0.03f, 0.0f, 0.10f, 0.36f)
    }},
    WeaponSpec{WeaponId::Katana, WeaponCategory::Melee, "Katana", {"Slash", "Wave"}, 0x4a12u, {
        Action(AttackShape::Cone, 34.0f, 0.30f, 2.75f, 1.15f, 0.0f, 0.0f, 0.35f, 0, false, false, false, 0.03f, 0.0f, 0.07f, 0.24f),
        Action(AttackShape::Wave, 26.0f, 1.05f, 7.50f, 0.48f, 10.5f, 0.85f, 0.20f, 1, true, false, false, 0.07f, 0.0f, 0.15f, 0.50f)
    }},
    WeaponSpec{WeaponId::Pistol, WeaponCategory::Ranged, "Pistol", {"Shot", "Charge"}, 0x7013u, {
        Action(AttackShape::Projectile, 26.0f, 0.24f, 10.0f, 0.30f, 14.5f, 0.95f, 0.0f, 1, false, false, false, 0.02f, 0.0f, 0.05f, 0.20f),
        Action(AttackShape::Projectile, 58.0f, 0.92f, 8.0f, 0.58f, 7.0f, 1.25f, 0.0f, 1, true, false, false, 0.10f, 0.0f, 0.16f, 0.42f)
    }},
    WeaponSpec{WeaponId::Rifle, WeaponCategory::Ranged, "Rifle", {"Pierce", "Burst"}, 0x7114u, {
        Action(AttackShape::Projectile, 44.0f, 0.52f, 15.0f, 0.24f, 20.0f, 0.80f, 0.0f, 1, true, false, false, 0.04f, 0.0f, 0.12f, 0.28f),
        Action(AttackShape::Burst, 20.0f, 1.10f, 9.0f, 0.26f, 12.0f, 0.85f, 0.0f, 10, false, false, false, 0.08f, 0.0f, 0.20f, 0.48f)
    }},
    WeaponSpec{WeaponId::Machinegun, WeaponCategory::Ranged, "Machinegun", {"Spray", "Overdrive"}, 0x7215u, {
        Action(AttackShape::Projectile, 12.0f, 0.09f, 8.5f, 0.22f, 13.5f, 0.75f, 0.0f, 1, false, false, false, 0.01f, 0.0f, 0.03f, 0.12f),
        Action(AttackShape::Burst, 11.0f, 1.35f, 7.5f, 0.24f, 12.5f, 0.75f, 0.72f, 7, false, true, false, 0.06f, 0.0f, 0.24f, 0.58f)
    }},
    WeaponSpec{WeaponId::Shotgun, WeaponCategory::Ranged, "Shotgun", {"Scatter", "Shock"}, 0x7316u, {
        Action(AttackShape::Burst, 16.0f, 0.68f, 4.5f, 0.34f, 10.0f, 0.48f, 0.65f, 5, false, false, false, 0.05f, 0.0f, 0.14f, 0.34f),
        Action(AttackShape::Cone, 22.0f, 1.05f, 4.0f, 1.55f, 0.0f, 0.0f, 0.45f, 0, false, false, false, 0.06f, 0.0f, 0.18f, 0.42f)
    }},
    WeaponSpec{WeaponId::Staff, WeaponCategory::Magic, "Staff", {"Orbit", "Orb"}, 0x8117u, {
        Action(AttackShape::Orbit, 18.0f, 0.72f, 3.6f, 1.05f, 0.0f, 0.0f, -1.0f, 0, false, false, false, 0.05f, 0.0f, 0.16f, 0.46f),
        Action(AttackShape::Projectile, 24.0f, 1.15f, 9.0f, 0.40f, 6.0f, 1.70f, 0.0f, 1, true, false, false, 0.08f, 0.0f, 0.20f, 0.52f)
    }},
    WeaponSpec{WeaponId::Scepter, WeaponCategory::Magic, "Scepter", {"Mark", "Bolt"}, 0x8218u, {
        Action(AttackShape::TargetArea, 42.0f, 0.95f, 6.0f, 1.75f, 0.0f, 0.0f, -1.0f, 0, false, false, false, 0.08f, 0.0f, 0.18f, 0.50f),
        Action(AttackShape::Projectile, 64.0f, 1.15f, 8.5f, 0.46f, 8.5f, 1.00f, 0.0f, 1, true, false, false, 0.10f, 0.0f, 0.22f, 0.50f)
    }},
    WeaponSpec{WeaponId::Gloves, WeaponCategory::Magic, "Gloves", {"Pulse", "Field"}, 0x8319u, {
        Action(AttackShape::Circle, 25.0f, 0.34f, 0.0f, 2.05f, 0.0f, 0.0f, -1.0f, 0, false, false, false, 0.02f, 0.0f, 0.08f, 0.24f),
        Action(AttackShape::Circle, 18.0f, 2.85f, 0.0f, 4.20f, 0.0f, 0.0f, -1.0f, 0, false, false, false, 0.08f, 0.0f, 0.28f, 0.70f)
    }}
}};

int WeaponIndex(WeaponId weapon) {
    const int index = static_cast<int>(weapon);
    return index >= 0 && index < static_cast<int>(WeaponId::Count) ? index : static_cast<int>(WeaponId::Pistol);
}

int ActionIndex(WeaponActionIndex action) {
    return action == WeaponActionIndex::Action2 ? 1 : 0;
}

float EnemyMaxHp(EnemyKind kind) {
    switch (kind) {
    case EnemyKind::Brute:
        return 92.0f;
    case EnemyKind::Caster:
        return 58.0f;
    case EnemyKind::Skirmisher:
        return 48.0f;
    case EnemyKind::Bulwark:
        return 116.0f;
    case EnemyKind::Boss:
        return 280.0f;
    }
    return 60.0f;
}

float EnemySpeed(EnemyKind kind) {
    switch (kind) {
    case EnemyKind::Brute:
        return 2.35f;
    case EnemyKind::Caster:
        return 1.75f;
    case EnemyKind::Skirmisher:
        return 3.35f;
    case EnemyKind::Bulwark:
        return 1.35f;
    case EnemyKind::Boss:
        return 1.90f;
    }
    return 2.0f;
}

float EnemyPreferredRange(EnemyKind kind) {
    switch (kind) {
    case EnemyKind::Brute:
        return 1.45f;
    case EnemyKind::Caster:
        return 5.8f;
    case EnemyKind::Skirmisher:
        return 2.25f;
    case EnemyKind::Bulwark:
        return 2.0f;
    case EnemyKind::Boss:
        return 4.2f;
    }
    return 2.0f;
}

WeaponSlot EnemyWeapon(EnemyKind kind) {
    switch (kind) {
    case EnemyKind::Brute:
        return WeaponSlot{WeaponId::Hammer, Element::Stone};
    case EnemyKind::Caster:
        return WeaponSlot{WeaponId::Staff, Element::Electricity};
    case EnemyKind::Skirmisher:
        return WeaponSlot{WeaponId::Katana, Element::Air};
    case EnemyKind::Bulwark:
        return WeaponSlot{WeaponId::Shotgun, Element::Ice};
    case EnemyKind::Boss:
        return WeaponSlot{WeaponId::Scepter, Element::Fire};
    }
    return WeaponSlot{WeaponId::Pistol, Element::Fire};
}

WeaponSlot BossPhaseWeapon(const EnemyState& enemy) {
    const float ratio = HpRatio(enemy);
    if (ratio > 0.66f) {
        return WeaponSlot{WeaponId::Scepter, Element::Fire};
    }
    if (ratio > 0.33f) {
        return WeaponSlot{WeaponId::Staff, Element::Electricity};
    }
    return WeaponSlot{WeaponId::Shotgun, Element::Ice};
}

WeaponSlot EnemyAttackWeapon(const EnemyState& enemy) {
    return enemy.kind == EnemyKind::Boss ? BossPhaseWeapon(enemy) : enemy.weapon;
}

EnemyPressureProfile EnemyProfile(const EnemyState& enemy) {
    if (enemy.kind == EnemyKind::Boss) {
        const float ratio = HpRatio(enemy);
        if (ratio > 0.66f) {
            return EnemyPressureProfile{1.65f, 5.2f, 2.8f, 7.0f, 0.92f, kBossDamageScale, WeaponActionIndex::Action1, false, false};
        }
        if (ratio > 0.33f) {
            return EnemyPressureProfile{1.95f, 6.0f, 3.0f, 9.2f, 0.82f, kBossDamageScale, WeaponActionIndex::Action2, true, false};
        }
        return EnemyPressureProfile{2.35f, 2.4f, 0.0f, 4.4f, 0.76f, kBossDamageScale, WeaponActionIndex::Action2, false, false};
    }

    switch (enemy.kind) {
    case EnemyKind::Brute:
        return EnemyPressureProfile{2.35f, 1.45f, 0.0f, 1.85f, 1.15f, kEnemyAreaDamageScale, WeaponActionIndex::Action1, false, false};
    case EnemyKind::Caster:
        return EnemyPressureProfile{1.75f, 6.2f, 3.15f, 8.8f, 1.05f, kEnemyProjectileDamageScale, WeaponActionIndex::Action2, true, false};
    case EnemyKind::Skirmisher:
        return EnemyPressureProfile{3.35f, 3.15f, 1.15f, 6.2f, 0.95f, kEnemySkirmisherDamageScale, WeaponActionIndex::Action2, true, true};
    case EnemyKind::Bulwark:
        return EnemyPressureProfile{1.35f, 2.0f, 0.0f, 4.1f, 1.25f, kEnemyBulwarkDamageScale, WeaponActionIndex::Action2, false, false};
    case EnemyKind::Boss:
        break;
    }
    return EnemyPressureProfile{};
}

float StatusDuration(StatusKind status) {
    switch (status) {
    case StatusKind::Wet:
        return 5.0f;
    case StatusKind::Burning:
        return 4.0f;
    case StatusKind::Charged:
        return 5.0f;
    case StatusKind::Chilled:
        return 4.5f;
    case StatusKind::None:
        return 0.0f;
    }
    return 0.0f;
}

float StatusIntensity(StatusKind status) {
    switch (status) {
    case StatusKind::Wet:
        return 1.0f;
    case StatusKind::Burning:
        return 0.045f;
    case StatusKind::Charged:
        return 1.0f;
    case StatusKind::Chilled:
        return 0.32f;
    case StatusKind::None:
        return 0.0f;
    }
    return 0.0f;
}

StatusInstance* FindStatus(ActorStatusSet& statuses, StatusKind status) {
    for (StatusInstance& instance : statuses.slots) {
        if (instance.active && instance.kind == status) {
            return &instance;
        }
    }
    return nullptr;
}

const StatusInstance* FindStatus(const ActorStatusSet& statuses, StatusKind status) {
    for (const StatusInstance& instance : statuses.slots) {
        if (instance.active && instance.kind == status) {
            return &instance;
        }
    }
    return nullptr;
}

StatusInstance* FindFreeStatusSlot(ActorStatusSet& statuses) {
    for (StatusInstance& instance : statuses.slots) {
        if (!instance.active) {
            return &instance;
        }
    }
    return &statuses.slots[0];
}

float StatusMoveMultiplier(const ActorStatusSet& statuses) {
    const StatusInstance* chilled = FindStatus(statuses, StatusKind::Chilled);
    if (!chilled) {
        return 1.0f;
    }
    return 1.0f - Clamp(chilled->intensity, 0.0f, 0.75f);
}

Vec2 Rotate(Vec2 v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return Vec2{v.x * c - v.y * s, v.x * s + v.y * c};
}

bool SnapshotHasStatus(const std::array<StatusInstance, kStatusSlotCount>& snapshot, StatusKind status) {
    for (const StatusInstance& instance : snapshot) {
        if (instance.active && instance.kind == status) {
            return true;
        }
    }
    return false;
}

void QueueForcedStatusElement(std::array<Element, kStatusSlotCount>& elements, int& count, Element element) {
    if (element == Element::None) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (elements[static_cast<std::size_t>(i)] == element) {
            return;
        }
    }
    if (count < kStatusSlotCount) {
        elements[static_cast<std::size_t>(count++)] = element;
    }
}

float DistanceSqToSegment(Vec2 p, Vec2 a, Vec2 b) {
    const Vec2 ab = b - a;
    const float abLenSq = LengthSq(ab);
    if (abLenSq <= 0.000001f) {
        return LengthSq(p - a);
    }
    const float t = Clamp(Dot(p - a, ab) / abLenSq, 0.0f, 1.0f);
    const Vec2 closest = a + ab * t;
    return LengthSq(p - closest);
}

Vec2 FarthestTraversablePoint(const RoomGraph& world, int roomIndex, Vec2 start, Vec2 dir, float distance) {
    constexpr int kSteps = 16;
    Vec2 best = start;
    for (int i = 1; i <= kSteps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSteps);
        const Vec2 candidate = start + dir * (distance * t);
        if (IsTraversablePosition(world, roomIndex, candidate)) {
            best = candidate;
        } else {
            break;
        }
    }
    return best;
}

}

const WeaponSpec& GetWeaponSpec(WeaponId weapon) {
    return kWeaponSpecs[static_cast<std::size_t>(WeaponIndex(weapon))];
}

CombatActionTiming GetWeaponActionTiming(const WeaponActionSpec& action) {
    return CombatActionTiming{
        action.windup,
        action.impactTime,
        action.recovery,
        action.vfxDuration
    };
}

CombatActionTiming GetWeaponActionTiming(WeaponId weapon, WeaponActionIndex action) {
    const WeaponSpec& spec = GetWeaponSpec(weapon);
    return GetWeaponActionTiming(spec.actions[static_cast<std::size_t>(ActionIndex(action))]);
}

uint32_t PackCombatActionTiming(CombatActionTiming timing) {
    auto quantize = [](float seconds) -> uint32_t {
        return static_cast<uint32_t>(Clamp(seconds, 0.0f, 1.0f) * 255.0f + 0.5f) & 0xffu;
    };
    return quantize(timing.windup) |
        (quantize(timing.impact) << 8) |
        (quantize(timing.recovery) << 16) |
        (quantize(timing.vfxDuration) << 24);
}

CombatActionTiming UnpackCombatActionTiming(uint32_t packed) {
    auto unpack = [](uint32_t bits) -> float {
        return static_cast<float>(bits & 0xffu) / 255.0f;
    };
    return CombatActionTiming{
        unpack(packed),
        unpack(packed >> 8),
        unpack(packed >> 16),
        unpack(packed >> 24)
    };
}

StatusKind PersistentStatusForElement(Element element) {
    switch (element) {
    case Element::Water:
        return StatusKind::Wet;
    case Element::Fire:
        return StatusKind::Burning;
    case Element::Electricity:
        return StatusKind::Charged;
    case Element::Ice:
        return StatusKind::Chilled;
    case Element::Stone:
    case Element::Air:
    case Element::None:
        return StatusKind::None;
    }
    return StatusKind::None;
}

bool ElementHasPersistentStatus(Element element) {
    return PersistentStatusForElement(element) != StatusKind::None;
}

float PlayerActionCooldownRatio(const PlayerState& player, WeaponActionIndex action) {
    if (player.activeWeaponSlot < 0 || player.activeWeaponSlot >= kPlayerWeaponSlots) {
        return 0.0f;
    }
    const int actionSlot = ActionIndex(action);
    const WeaponSlot& slot = player.weaponSlots[player.activeWeaponSlot];
    const WeaponSpec& weapon = GetWeaponSpec(slot.weapon);
    const float cooldownScale = player.cooldownMultiplier > 0.0f ? player.cooldownMultiplier : 1.0f;
    const float maxCooldown = weapon.actions[static_cast<std::size_t>(actionSlot)].cooldown * cooldownScale;
    if (maxCooldown <= 0.0001f) {
        return 0.0f;
    }
    return Clamp(slot.cooldowns[actionSlot] / maxCooldown, 0.0f, 1.0f);
}

bool PlayerActionReady(const PlayerState& player, WeaponActionIndex action) {
    if (player.activeWeaponSlot < 0 || player.activeWeaponSlot >= kPlayerWeaponSlots) {
        return false;
    }
    const int actionSlot = ActionIndex(action);
    return player.weaponSlots[player.activeWeaponSlot].cooldowns[actionSlot] <= 0.0001f;
}

EnemyAttackIntent EnemyAttackIntentFor(const EnemyState& enemy, const PlayerState& player) {
    EnemyAttackIntent intent{};
    if (!enemy.active || enemy.hp <= 0.0f || enemy.roomIndex != player.roomIndex || enemy.statuses.stun > 0.0f) {
        return intent;
    }

    if (enemy.actionTimer > 0.0f && enemy.actionDuration > 0.0f && enemy.actionImpactTime > 0.0f) {
        const float elapsed = Clamp(enemy.actionDuration - enemy.actionTimer, 0.0f, enemy.actionDuration);
        if (elapsed <= enemy.actionImpactTime + 0.001f) {
            const WeaponSpec& activeSpec = GetWeaponSpec(enemy.activeActionWeapon);
            const WeaponActionSpec& activeAction =
                activeSpec.actions[static_cast<std::size_t>(ActionIndex(enemy.activeActionIndex))];
            intent.weapon = enemy.activeActionWeapon;
            intent.element = enemy.activeActionElement;
            intent.actionIndex = enemy.activeActionIndex;
            intent.shape = enemy.activeActionShape;
            intent.direction = NormalizeOr(enemy.activeActionDirection, Vec2{1.0f, 0.0f});
            intent.range = activeAction.range;
            intent.radius = activeAction.radius;
            intent.cooldownRatio = 0.0f;
            intent.readiness = 1.0f;
            intent.inRange = true;
            intent.active = true;
            return intent;
        }
    }

    const EnemyPressureProfile profile = EnemyProfile(enemy);
    const WeaponSlot attackWeapon = EnemyAttackWeapon(enemy);
    const int actionSlot = ActionIndex(profile.actionIndex);
    const WeaponSpec& spec = GetWeaponSpec(attackWeapon.weapon);
    const WeaponActionSpec& action = spec.actions[static_cast<std::size_t>(actionSlot)];
    const float maxCooldown = action.cooldown * profile.cooldownScale;
    const float cooldownRatio = maxCooldown > 0.0001f
        ? Clamp(enemy.weapon.cooldowns[actionSlot] / maxCooldown, 0.0f, 1.0f)
        : 0.0f;
    const Vec2 toPlayer = player.position - enemy.position;
    const float distance = Length(toPlayer);

    intent.weapon = attackWeapon.weapon;
    intent.element = attackWeapon.element;
    intent.actionIndex = profile.actionIndex;
    intent.shape = action.shape;
    intent.direction = NormalizeOr(toPlayer, Vec2{1.0f, 0.0f});
    intent.range = action.range > 0.0001f ? action.range : profile.attackRange;
    intent.radius = action.radius;
    intent.cooldownRatio = cooldownRatio;
    intent.readiness = 1.0f - cooldownRatio;
    intent.inRange = distance <= profile.attackRange;
    intent.active = true;
    return intent;
}

void CombatSim::Reset(const RoomGraph& world) {
    player_ = PlayerState{};
    player_.position = world.rooms[0].center;
    player_.castOrigin = player_.position;
    player_.roomIndex = 0;
    player_.maxHp = 100.0f;
    player_.hp = player_.maxHp;
    player_.weaponSlots[0] = WeaponSlot{WeaponId::Katana, Element::Fire};
    player_.weaponSlots[1] = WeaponSlot{WeaponId::Pistol, Element::Electricity};
    player_.weaponSlots[2] = WeaponSlot{WeaponId::Gloves, Element::Ice};
    player_.activeWeaponSlot = 0;
    SyncPlayerActionFeedback();

    enemies_ = {};
    projectiles_ = {};
    pendingActions_ = {};
    nextActionSerial_ = 1;
    ClearEvents();

    for (int i = 0; i < world.spawnCount && i < kMaxEnemies; ++i) {
        const SpawnPoint& spawn = world.spawns[i];
        EnemyState& enemy = enemies_[i];
        enemy.position = spawn.position;
        enemy.roomIndex = spawn.roomIndex;
        if (spawn.archetype >= 4) {
            enemy.kind = EnemyKind::Boss;
        } else {
            switch (spawn.archetype % 4) {
        case 0:
            enemy.kind = EnemyKind::Brute;
            break;
        case 1:
            enemy.kind = EnemyKind::Caster;
            break;
        case 2:
            enemy.kind = EnemyKind::Skirmisher;
            break;
        default:
            enemy.kind = EnemyKind::Bulwark;
            break;
            }
        }
        const float floorScale = 1.0f + Clamp(static_cast<float>(world.floorIndex), 0.0f, 10.0f) * 0.10f;
        enemy.maxHp = EnemyMaxHp(enemy.kind) * floorScale;
        enemy.hp = enemy.maxHp;
        const float shieldScale = 1.0f + Clamp(static_cast<float>(world.floorIndex), 0.0f, 10.0f) * 0.08f;
        enemy.shield = (enemy.kind == EnemyKind::Boss ? 56.0f : (enemy.kind == EnemyKind::Bulwark ? 32.0f : 0.0f)) * shieldScale;
        enemy.weapon = EnemyWeapon(enemy.kind);
        enemy.active = false;
    }
}

void CombatSim::Tick(const InputState& input, float dt, RoomGraph& world) {
    ClearEvents();
    if (dt <= 0.0f) {
        SyncPlayerActionFeedback();
        return;
    }
    if (dt > 0.05f) {
        dt = 0.05f;
    }

    TickWeaponCooldowns(dt);
    TickStatuses(dt);
    TickActionFeedbackTimers(dt);
    TickPendingActions(dt, world);

    SelectWeaponSlot(input.selectWeaponSlot);

    player_.dashCooldown = Clamp(player_.dashCooldown - dt, 0.0f, 99.0f);
    player_.rootTimer = Clamp(player_.rootTimer - dt, 0.0f, 99.0f);

    Vec2 move = NormalizeOr(Vec2{input.moveX, input.moveY}, Vec2{0.0f, 0.0f});
    Vec2 aim = NormalizeOr(Vec2{input.aimX, input.aimY}, player_.facing);
    player_.facing = aim;

    float speed = kPlayerSpeed * player_.speedMultiplier * StatusMoveMultiplier(player_.statuses);
    if (player_.rootTimer > 0.0f || player_.statuses.stun > 0.0f) {
        speed = 0.0f;
    }
    if (input.dash && player_.dashCooldown <= 0.0f && player_.statuses.stun <= 0.0f) {
        speed = kDashSpeed * player_.speedMultiplier;
        player_.dashCooldown = 0.9f;
        player_.statuses.invulnerable = 0.16f;
    }
    const Vec2 candidatePosition = player_.position + move * (speed * dt);
    if (IsTraversablePosition(world, player_.roomIndex, candidatePosition)) {
        player_.position = candidatePosition;
    }

    const int newRoom = FindRoomAt(world, player_.position);
    if (newRoom >= 0 && !world.rooms[newRoom].locked) {
        player_.roomIndex = newRoom;
    }

    const bool useAction1 = input.action1 || input.melee;
    const bool useAction2 = input.action2 || input.ranged || input.control;
    if (useAction1) {
        CastWeaponAction(WeaponActionIndex::Action1, aim, world);
    }
    if (useAction2) {
        CastWeaponAction(WeaponActionIndex::Action2, aim, world);
    }

    UpdateProjectiles(dt);
    UpdateEnemies(dt, world);
    SyncPlayerActionFeedback();
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
    player_.castOrigin = position;
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
    ClearEvents();
    int killedBefore = 0;
    for (const EnemyState& enemy : enemies_) {
        if (enemy.roomIndex == roomIndex && enemy.hp <= 0.0f) {
            ++killedBefore;
        }
    }

    for (int i = 0; i < kMaxEnemies; ++i) {
        EnemyState& enemy = enemies_[i];
        if (enemy.roomIndex != roomIndex || enemy.hp <= 0.0f) {
            continue;
        }
        ApplyDamageToEnemy(i, damage, Element::Stone, WeaponId::Hammer, player_.position, 0.0f);
    }

    int killedAfter = 0;
    for (const EnemyState& enemy : enemies_) {
        if (enemy.roomIndex == roomIndex && enemy.hp <= 0.0f) {
            ++killedAfter;
        }
    }
    return std::max(0, killedAfter - killedBefore);
}

int CombatSim::DespawnEnemiesInRoom(int roomIndex) {
    int despawned = 0;
    for (EnemyState& enemy : enemies_) {
        if (enemy.roomIndex == roomIndex && enemy.hp > 0.0f) {
            enemy.hp = 0.0f;
            enemy.active = false;
            enemy.velocity = Vec2{};
            enemy.statuses = ActorStatusSet{};
            ++despawned;
        }
    }
    return despawned;
}

void CombatSim::SetPlayerWeaponSlot(int slot, WeaponId weapon, Element element) {
    if (slot < 0 || slot >= kPlayerWeaponSlots) {
        return;
    }
    player_.weaponSlots[slot] = WeaponSlot{weapon, element};
    player_.activeWeaponSlot = slot;
    SyncPlayerActionFeedback();
}

void CombatSim::RestorePlayerProgress(const PlayerState& progress, Vec2 position, int roomIndex) {
    const int activeSlot = progress.activeWeaponSlot >= 0 && progress.activeWeaponSlot < kPlayerWeaponSlots
        ? progress.activeWeaponSlot
        : 0;

    player_.position = position;
    player_.castOrigin = position;
    player_.roomIndex = roomIndex;
    player_.facing = NormalizeOr(progress.facing, Vec2{1.0f, 0.0f});
    player_.maxHp = Clamp(progress.maxHp, 1.0f, 300.0f);
    player_.hp = Clamp(std::max(progress.hp, player_.maxHp * 0.50f), 1.0f, player_.maxHp);
    player_.damageMultiplier = Clamp(progress.damageMultiplier, 0.25f, 4.0f);
    player_.cooldownMultiplier = Clamp(progress.cooldownMultiplier, 0.35f, 2.0f);
    player_.speedMultiplier = Clamp(progress.speedMultiplier, 0.35f, 2.0f);
    player_.areaMultiplier = Clamp(progress.areaMultiplier, 0.35f, 2.5f);
    player_.activeWeaponSlot = activeSlot;
    player_.katanaWaveCounter = progress.katanaWaveCounter;
    for (int i = 0; i < kPlayerWeaponSlots; ++i) {
        player_.weaponSlots[i] = progress.weaponSlots[i];
        player_.weaponSlots[i].cooldowns = {};
    }
    player_.statuses = ActorStatusSet{};
    player_.dashCooldown = 0.0f;
    player_.rootTimer = 0.0f;
    player_.meleeCooldown = 0.0f;
    player_.rangedCooldown = 0.0f;
    player_.controlCooldown = 0.0f;
    player_.actionTimer = 0.0f;
    player_.actionDuration = 0.0f;
    player_.actionImpactTime = 0.0f;
    player_.actionRecovery = 0.0f;
    SyncPlayerActionFeedback();
}

void CombatSim::ApplyPlayerUpgrade(PlayerUpgradeKind upgrade, float value) {
    switch (upgrade) {
    case PlayerUpgradeKind::Damage:
        player_.damageMultiplier = Clamp(player_.damageMultiplier + value, 0.25f, 4.0f);
        break;
    case PlayerUpgradeKind::Cooldown:
        player_.cooldownMultiplier = Clamp(player_.cooldownMultiplier * value, 0.35f, 2.0f);
        break;
    case PlayerUpgradeKind::Speed:
        player_.speedMultiplier = Clamp(player_.speedMultiplier + value, 0.35f, 2.0f);
        break;
    case PlayerUpgradeKind::Area:
        player_.areaMultiplier = Clamp(player_.areaMultiplier + value, 0.35f, 2.5f);
        break;
    case PlayerUpgradeKind::MaxHp:
        player_.maxHp = Clamp(player_.maxHp + value, 1.0f, 300.0f);
        player_.hp = Clamp(player_.hp + value, 0.0f, player_.maxHp);
        break;
    case PlayerUpgradeKind::Heal:
        player_.hp = Clamp(player_.hp + value, 0.0f, player_.maxHp);
        break;
    }
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

void CombatSim::ClearEvents() {
    events_ = {};
    eventCount_ = 0;
}

void CombatSim::Emit(CombatEvent event) {
    if (eventCount_ < kMaxCombatEvents) {
        events_[eventCount_++] = event;
    }
}

void CombatSim::TickWeaponCooldowns(float dt) {
    for (WeaponSlot& slot : player_.weaponSlots) {
        slot.cooldowns[0] = Clamp(slot.cooldowns[0] - dt, 0.0f, 99.0f);
        slot.cooldowns[1] = Clamp(slot.cooldowns[1] - dt, 0.0f, 99.0f);
    }
    for (EnemyState& enemy : enemies_) {
        enemy.weapon.cooldowns[0] = Clamp(enemy.weapon.cooldowns[0] - dt, 0.0f, 99.0f);
        enemy.weapon.cooldowns[1] = Clamp(enemy.weapon.cooldowns[1] - dt, 0.0f, 99.0f);
    }
    SyncPlayerActionFeedback();
}

void CombatSim::TickStatuses(float dt) {
    auto tickSet = [this, dt](ActorStatusSet& statuses, Faction faction, int roomIndex, int entityIndex) {
        statuses.statusLock = Clamp(statuses.statusLock - dt, 0.0f, 99.0f);
        statuses.stun = Clamp(statuses.stun - dt, 0.0f, 99.0f);
        statuses.invulnerable = Clamp(statuses.invulnerable - dt, 0.0f, 99.0f);
        if (statuses.stunSequence > 0.0f) {
            statuses.stunSequence = Clamp(statuses.stunSequence - dt, 0.0f, 99.0f);
            statuses.stunPulseTimer -= dt;
            if (statuses.stunSequence > 0.0f && statuses.stunPulseTimer <= 0.0f) {
                statuses.stun = std::max(statuses.stun, kStunPulseSeconds);
                statuses.stunPulseTimer += kStunPulseInterval;
                Emit(CombatEvent{
                    CombatEventType::StunApplied,
                    faction,
                    roomIndex,
                    entityIndex,
                    kStunPulseSeconds,
                    statuses.stunElement,
                    StatusKind::None,
                    statuses.stunReaction,
                    statuses.stunWeapon});
            }
        }
        for (StatusInstance& status : statuses.slots) {
            if (!status.active) {
                continue;
            }
            status.duration -= dt;
            if (status.duration <= 0.0f) {
                status = StatusInstance{};
            }
        }
    };

    auto applyBurning = [this, dt](Faction faction, int roomIndex, int entityIndex, float maxHp, float& hp, bool& active) {
        ActorStatusSet& statuses = faction == Faction::Player ? player_.statuses : enemies_[entityIndex].statuses;
        const StatusInstance* burning = FindStatus(statuses, StatusKind::Burning);
        if (!burning || hp <= 0.0f) {
            return;
        }
        const float damage = std::max(0.3f, maxHp * burning->intensity * dt);
        hp -= damage;
        Emit(CombatEvent{CombatEventType::ActorDamaged, faction, roomIndex, entityIndex, damage, Element::Fire, StatusKind::Burning, ReactionKind::None, WeaponId::Scepter});
        if (hp <= 0.0f) {
            hp = 0.0f;
            active = false;
            Emit(CombatEvent{CombatEventType::ActorKilled, faction, roomIndex, entityIndex, 0.0f, Element::Fire});
        }
    };

    bool playerActive = player_.hp > 0.0f;
    applyBurning(Faction::Player, player_.roomIndex, -1, player_.maxHp, player_.hp, playerActive);
    tickSet(player_.statuses, Faction::Player, player_.roomIndex, -1);

    for (int i = 0; i < kMaxEnemies; ++i) {
        EnemyState& enemy = enemies_[i];
        if (enemy.hp > 0.0f) {
            applyBurning(Faction::Enemy, enemy.roomIndex, i, enemy.maxHp, enemy.hp, enemy.active);
        }
        tickSet(enemy.statuses, Faction::Enemy, enemy.roomIndex, i);
    }
}

void CombatSim::TickActionFeedbackTimers(float dt) {
    player_.actionTimer = Clamp(player_.actionTimer - dt, 0.0f, 99.0f);
    if (player_.actionTimer <= 0.0f) {
        player_.actionDuration = 0.0f;
        player_.actionImpactTime = 0.0f;
        player_.actionRecovery = 0.0f;
    }

    for (EnemyState& enemy : enemies_) {
        enemy.actionTimer = Clamp(enemy.actionTimer - dt, 0.0f, 99.0f);
        if (enemy.actionTimer <= 0.0f) {
            enemy.actionDuration = 0.0f;
            enemy.actionImpactTime = 0.0f;
            enemy.actionRecovery = 0.0f;
        }
    }
}

void CombatSim::TickPendingActions(float dt, RoomGraph& world) {
    for (PendingCombatAction& action : pendingActions_) {
        if (!action.active) {
            continue;
        }
        action.impactTimer -= dt;
        if (action.impactTimer <= 0.0f) {
            const PendingCombatAction resolved = action;
            action = PendingCombatAction{};
            ResolvePendingAction(resolved, world);
        }
    }
}

bool CombatSim::EnemyHasPendingAction(int enemyIndex) const {
    for (const PendingCombatAction& action : pendingActions_) {
        if (action.active && action.faction == Faction::Enemy && action.entityIndex == enemyIndex) {
            return true;
        }
    }
    return false;
}

void CombatSim::QueueOrResolveAction(PendingCombatAction action, RoomGraph& world) {
    action.active = true;
    action.impactTimer = std::max(0.0f, action.action.impactTime);
    if (action.serial == 0u) {
        action.serial = nextActionSerial_++;
    }
    if (action.impactTimer <= 0.0001f) {
        ResolvePendingAction(action, world);
        return;
    }
    QueuePendingAction(action);
}

void CombatSim::QueuePendingAction(const PendingCombatAction& action) {
    for (PendingCombatAction& slot : pendingActions_) {
        if (!slot.active) {
            slot = action;
            slot.active = true;
            return;
        }
    }
}

void CombatSim::ResolvePendingAction(const PendingCombatAction& action, RoomGraph& world) {
    if (action.faction == Faction::Player) {
        switch (action.action.shape) {
        case AttackShape::Projectile:
        case AttackShape::Wave:
            SpawnProjectile(action.origin + action.direction * 0.85f, action.direction, action.roomIndex, action.action, action.element, action.weapon);
            if (action.weapon == WeaponId::Katana && action.actionIndex == WeaponActionIndex::Action2) {
                ++player_.katanaWaveCounter;
                if ((player_.katanaWaveCounter % 3u) == 0u) {
                    SpawnProjectile(action.origin + action.direction * 0.35f, action.direction, action.roomIndex, action.action, action.element, action.weapon);
                }
            }
            break;
        case AttackShape::Burst: {
            const int count = std::max(1, action.action.projectileCount);
            const float spread = action.action.coneDot > 0.0f ? 0.70f : kPi * 2.0f;
            for (int i = 0; i < count; ++i) {
                const float t = count == 1 ? 0.5f : static_cast<float>(i) / static_cast<float>(count - 1);
                const float angle = action.action.coneDot > 0.0f ? (t - 0.5f) * spread : t * kPi * 2.0f;
                SpawnProjectile(action.origin + action.direction * 0.75f, Rotate(action.direction, angle), action.roomIndex, action.action, action.element, action.weapon);
            }
            break;
        }
        case AttackShape::Dash:
            ApplyDashAttack(action.action, action.dashStart, action.dashEnd, action.element, action.weapon, action.roomIndex);
            if (LengthSq(action.dashEnd - action.dashStart) > 0.0001f && player_.roomIndex == action.roomIndex) {
                player_.position = action.dashEnd;
                const int newRoom = FindRoomAt(world, player_.position);
                if (newRoom >= 0 && !world.rooms[newRoom].locked) {
                    player_.roomIndex = newRoom;
                }
            }
            break;
        case AttackShape::Cone:
        case AttackShape::Circle:
        case AttackShape::Orbit:
            ApplyAreaAttack(action.action, action.origin, action.origin, action.direction, action.element, action.weapon, action.roomIndex);
            break;
        case AttackShape::TargetArea:
            ApplyAreaAttack(action.action, action.origin, action.targetCenter, action.direction, action.element, action.weapon, action.roomIndex);
            break;
        }
        return;
    }

    if (action.faction != Faction::Enemy || action.entityIndex < 0 || action.entityIndex >= kMaxEnemies) {
        return;
    }

    EnemyState& enemy = enemies_[action.entityIndex];
    if (!enemy.active || enemy.hp <= 0.0f) {
        return;
    }

    switch (action.action.shape) {
    case AttackShape::Projectile:
    case AttackShape::Wave:
        SpawnProjectile(
            action.origin + action.direction * 0.70f,
            action.direction,
            action.roomIndex,
            action.action,
            action.element,
            action.weapon,
            Faction::Enemy,
            action.damageScale);
        break;
    case AttackShape::Burst: {
        const int count = std::max(1, action.action.projectileCount);
        const float spread = action.action.coneDot > 0.0f ? 0.76f : kPi * 2.0f;
        for (int i = 0; i < count; ++i) {
            const float t = count == 1 ? 0.5f : static_cast<float>(i) / static_cast<float>(count - 1);
            const float angle = action.action.coneDot > 0.0f ? (t - 0.5f) * spread : t * kPi * 2.0f;
            SpawnProjectile(
                action.origin + action.direction * 0.70f,
                Rotate(action.direction, angle),
                action.roomIndex,
                action.action,
                action.element,
                action.weapon,
                Faction::Enemy,
                action.damageScale);
        }
        break;
    }
    case AttackShape::Dash:
    case AttackShape::Cone:
    case AttackShape::Circle:
    case AttackShape::Orbit:
    case AttackShape::TargetArea: {
        if (player_.hp <= 0.0f || player_.roomIndex != action.roomIndex) {
            break;
        }
        const Vec2 toPlayer = player_.position - action.origin;
        const float range = action.action.range > 0.0001f ? action.action.range : action.action.radius;
        const float radius = action.action.radius;
        bool hit = false;
        switch (action.action.shape) {
        case AttackShape::Circle:
            hit = LengthSq(toPlayer) <= radius * radius;
            break;
        case AttackShape::TargetArea:
            hit = LengthSq(player_.position - action.targetCenter) <= radius * radius;
            break;
        case AttackShape::Orbit: {
            const float dist = Length(toPlayer);
            const float inner = std::max(0.0f, range - radius);
            const float outer = range + radius;
            hit = dist >= inner && dist <= outer;
            break;
        }
        case AttackShape::Dash:
            hit = DistanceSqToSegment(player_.position, action.dashStart, action.dashEnd) <= radius * radius;
            break;
        case AttackShape::Cone: {
            const float distSq = LengthSq(toPlayer);
            if (distSq <= range * range) {
                const Vec2 playerDir = NormalizeOr(toPlayer, action.direction);
                hit = Dot(playerDir, action.direction) >= action.action.coneDot;
            }
            break;
        }
        default:
            break;
        }
        if (hit) {
            ApplyDamageToPlayer(action.action.damage * action.damageScale, action.element, action.weapon, action.origin);
        }
        break;
    }
    }

    if (action.harassStep) {
        const float speed = EnemyProfile(enemy).speed * StatusMoveMultiplier(enemy.statuses);
        enemy.position = enemy.position - action.direction * 0.42f;
        enemy.velocity = enemy.velocity - action.direction * (speed * 0.35f);
    }
}

void CombatSim::SelectWeaponSlot(int slot) {
    if (slot >= 0 && slot < kPlayerWeaponSlots) {
        player_.activeWeaponSlot = slot;
        SyncPlayerActionFeedback();
    }
}

void CombatSim::SyncPlayerActionFeedback() {
    if (player_.activeWeaponSlot < 0 || player_.activeWeaponSlot >= kPlayerWeaponSlots) {
        player_.meleeCooldown = 0.0f;
        player_.rangedCooldown = 0.0f;
        player_.controlCooldown = 0.0f;
        return;
    }

    const WeaponSlot& slot = player_.weaponSlots[player_.activeWeaponSlot];
    player_.meleeCooldown = slot.cooldowns[0];
    player_.rangedCooldown = slot.cooldowns[1];
    player_.controlCooldown = slot.cooldowns[1];
}

void CombatSim::CastWeaponAction(WeaponActionIndex actionIndex, Vec2 aim, RoomGraph& world) {
    if (player_.statuses.stun > 0.0f) {
        return;
    }
    if (player_.activeWeaponSlot < 0 || player_.activeWeaponSlot >= kPlayerWeaponSlots) {
        return;
    }

    WeaponSlot& slot = player_.weaponSlots[player_.activeWeaponSlot];
    const int actionSlot = ActionIndex(actionIndex);
    if (slot.cooldowns[actionSlot] > 0.0f) {
        return;
    }

    const WeaponSpec& weapon = GetWeaponSpec(slot.weapon);
    const WeaponActionSpec& action = weapon.actions[static_cast<std::size_t>(actionSlot)];
    const float cooldownScale = player_.cooldownMultiplier > 0.0f ? player_.cooldownMultiplier : 1.0f;
    const float areaScale = player_.areaMultiplier > 0.0f ? player_.areaMultiplier : 1.0f;
    const CombatActionTiming timing = GetWeaponActionTiming(action);
    const float actionDuration = std::max(timing.vfxDuration, timing.impact + timing.recovery);
    slot.cooldowns[actionSlot] = action.cooldown * cooldownScale;
    ++slot.actionCounter;
    player_.castOrigin = player_.position;
    player_.actionTimer = actionDuration;
    player_.actionDuration = actionDuration;
    player_.actionImpactTime = timing.impact;
    player_.actionRecovery = timing.recovery;
    player_.activeActionWeapon = slot.weapon;
    player_.activeActionElement = slot.element;
    player_.activeActionIndex = actionIndex;
    player_.activeActionShape = action.shape;
    player_.activeActionOrigin = player_.position;
    player_.activeActionTarget = player_.position;
    player_.activeActionDirection = aim;
    if (action.rooted) {
        player_.rootTimer = std::max(player_.rootTimer, 0.45f);
    }
    if (action.invulnerableDash) {
        player_.statuses.invulnerable = std::max(player_.statuses.invulnerable, action.duration);
    }

    SyncPlayerActionFeedback();
    Emit(CombatEvent{
        CombatEventType::WeaponActionUsed,
        Faction::Player,
        player_.roomIndex,
        -1,
        static_cast<float>(actionSlot),
        slot.element,
        StatusKind::None,
        ReactionKind::None,
        slot.weapon,
        action.shape,
        timing.windup,
        timing.impact,
        timing.recovery,
        timing.vfxDuration,
        PackCombatActionTiming(timing)});

    PendingCombatAction pending{};
    pending.faction = Faction::Player;
    pending.roomIndex = player_.roomIndex;
    pending.entityIndex = -1;
    pending.weapon = slot.weapon;
    pending.element = slot.element;
    pending.actionIndex = actionIndex;
    pending.action = action;
    pending.origin = player_.position;
    pending.targetCenter = player_.position;
    pending.direction = aim;
    pending.dashStart = player_.position;
    pending.dashEnd = player_.position;
    pending.damageScale = 1.0f;
    pending.impactTimer = timing.impact;
    pending.serial = nextActionSerial_++;

    if (action.shape == AttackShape::Dash) {
        pending.dashEnd = FarthestTraversablePoint(world, player_.roomIndex, pending.dashStart, aim, action.range);
        pending.targetCenter = pending.dashEnd;
    } else if (action.shape == AttackShape::TargetArea) {
        pending.targetCenter = FarthestTraversablePoint(world, player_.roomIndex, player_.position, aim, action.range * areaScale);
    }
    player_.activeActionOrigin = pending.origin;
    player_.activeActionTarget = pending.targetCenter;
    player_.activeActionDirection = pending.direction;

    QueueOrResolveAction(pending, world);
}

void CombatSim::SpawnProjectile(
    Vec2 position,
    Vec2 dir,
    int roomIndex,
    const WeaponActionSpec& action,
    Element element,
    WeaponId weapon,
    Faction ownerFaction,
    float damageScale) {
    for (ProjectileState& projectile : projectiles_) {
        if (!projectile.active) {
            const float areaScale = ownerFaction == Faction::Player && player_.areaMultiplier > 0.0f ? player_.areaMultiplier : 1.0f;
            projectile.active = true;
            projectile.position = position;
            projectile.velocity = dir * action.speed;
            projectile.damage = action.damage * (ownerFaction == Faction::Player ? player_.damageMultiplier : damageScale);
            projectile.radius = action.radius * areaScale;
            projectile.ttl = (action.duration > 0.0f ? action.duration : 1.0f) * areaScale;
            projectile.roomIndex = roomIndex;
            projectile.pierceRemaining = action.piercing ? 4 : 0;
            projectile.hitMask = 0;
            projectile.ownerFaction = ownerFaction;
            projectile.element = element;
            projectile.weapon = weapon;
            projectile.knockback = element == Element::Air ? (ownerFaction == Faction::Player ? 3.5f : 2.0f) : 0.0f;
            Emit(CombatEvent{CombatEventType::ProjectileSpawned, ownerFaction, roomIndex, -1, projectile.damage, element, StatusKind::None, ReactionKind::None, weapon});
            return;
        }
    }
}

void CombatSim::ApplyAreaAttack(
    const WeaponActionSpec& action,
    Vec2 origin,
    Vec2 targetCenter,
    Vec2 dir,
    Element element,
    WeaponId weapon,
    int roomIndex) {
    const float range = action.range * player_.areaMultiplier;
    const float radius = action.radius * player_.areaMultiplier;
    auto evaluateHit = [&](int enemyIndex, Vec2& damageOrigin) {
        EnemyState& enemy = enemies_[enemyIndex];
        if (!enemy.active || enemy.roomIndex != roomIndex || enemy.hp <= 0.0f) {
            return false;
        }
        const Vec2 toEnemy = enemy.position - origin;
        bool hit = false;
        damageOrigin = origin;
        switch (action.shape) {
        case AttackShape::Circle:
        case AttackShape::TargetArea:
            damageOrigin = targetCenter;
            hit = LengthSq(enemy.position - targetCenter) <= radius * radius;
            break;
        case AttackShape::Orbit: {
            const float dist = Length(enemy.position - origin);
            const float inner = std::max(0.0f, range - radius);
            const float outer = range + radius;
            hit = dist >= inner && dist <= outer;
            break;
        }
        case AttackShape::Dash:
        case AttackShape::Cone: {
            const float distSq = LengthSq(toEnemy);
            if (distSq <= range * range) {
                const Vec2 enemyDir = NormalizeOr(toEnemy, dir);
                hit = Dot(enemyDir, dir) >= action.coneDot;
            }
            break;
        }
        default:
            break;
        }
        return hit;
    };

    uint32_t hitMask = 0u;
    for (int i = 0; i < kMaxEnemies; ++i) {
        Vec2 damageOrigin{};
        if (evaluateHit(i, damageOrigin)) {
            hitMask |= 1u << static_cast<uint32_t>(i);
        }
    }

    for (int i = 0; i < kMaxEnemies; ++i) {
        Vec2 damageOrigin{};
        const bool hit = evaluateHit(i, damageOrigin);
        if (hit) {
            const float knockback = element == Element::Air || weapon == WeaponId::Shotgun ? 4.0f : 0.0f;
            ApplyDamageToEnemy(i, action.damage * player_.damageMultiplier, element, weapon, damageOrigin, knockback, hitMask);
        }
    }
}

void CombatSim::ApplyDashAttack(const WeaponActionSpec& action, Vec2 start, Vec2 end, Element element, WeaponId weapon, int roomIndex) {
    const float radius = action.radius * player_.areaMultiplier;
    const float radiusSq = radius * radius;
    uint32_t hitMask = 0u;
    for (int i = 0; i < kMaxEnemies; ++i) {
        const EnemyState& enemy = enemies_[i];
        if (!enemy.active || enemy.roomIndex != roomIndex || enemy.hp <= 0.0f) {
            continue;
        }
        if (DistanceSqToSegment(enemy.position, start, end) <= radiusSq) {
            hitMask |= 1u << static_cast<uint32_t>(i);
        }
    }

    for (int i = 0; i < kMaxEnemies; ++i) {
        EnemyState& enemy = enemies_[i];
        if (!enemy.active || enemy.roomIndex != roomIndex || enemy.hp <= 0.0f) {
            continue;
        }
        if (DistanceSqToSegment(enemy.position, start, end) <= radiusSq) {
            const float knockback = element == Element::Air ? 5.0f : 2.0f;
            ApplyDamageToEnemy(i, action.damage * player_.damageMultiplier, element, weapon, start, knockback, hitMask);
        }
    }
}

float CombatSim::ResolveDamageEvent(
    ActorStatusSet& statuses,
    Faction faction,
    int roomIndex,
    int entityIndex,
    Element element,
    WeaponId weapon,
    float damage,
    float& shield,
    float& knockback,
    std::array<Element, kStatusSlotCount>& forcedStatusElements,
    int& forcedStatusCount,
    bool& suppressBaseStatus,
    ReactionKind& areaReaction,
    float& areaReactionDamage,
    float& chilledIntensityBonus) {
    forcedStatusElements = {};
    forcedStatusCount = 0;
    suppressBaseStatus = false;
    areaReaction = ReactionKind::None;
    areaReactionDamage = 0.0f;
    chilledIntensityBonus = 0.0f;

    const auto statusSnapshot = statuses.slots;
    const bool hadWet = SnapshotHasStatus(statusSnapshot, StatusKind::Wet);
    const bool hadBurning = SnapshotHasStatus(statusSnapshot, StatusKind::Burning);
    const bool hadCharged = SnapshotHasStatus(statusSnapshot, StatusKind::Charged);
    const bool hadChilled = SnapshotHasStatus(statusSnapshot, StatusKind::Chilled);

    float damagePercent = 0.0f;
    bool lockCreated = false;

    auto applyStatusLock = [&](ReactionKind reaction) {
        statuses.statusLock = std::max(statuses.statusLock, kStunSequenceSeconds);
        statuses.stun = std::max(statuses.stun, kStunPulseSeconds);
        statuses.stunSequence = std::max(statuses.stunSequence, kStunSequenceSeconds);
        statuses.stunPulseTimer = kStunPulseInterval;
        statuses.stunReaction = reaction;
        statuses.stunElement = element;
        statuses.stunWeapon = weapon;
        suppressBaseStatus = true;
        if (!lockCreated) {
            Emit(CombatEvent{CombatEventType::StunApplied, faction, roomIndex, entityIndex, kStunPulseSeconds, element, StatusKind::None, reaction, weapon});
        }
        lockCreated = true;
    };

    if (element == Element::Air) {
        knockback = std::max(knockback, 3.0f);
    }

    if (hadWet && element == Element::Fire) {
        damagePercent -= 0.50f;
        RemoveStatus(statuses, faction, roomIndex, entityIndex, StatusKind::Wet);
        suppressBaseStatus = true;
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::WetFire, element, damage);
    }
    if (hadWet && element == Element::Electricity) {
        damagePercent += 0.20f;
        RemoveStatus(statuses, faction, roomIndex, entityIndex, StatusKind::Wet);
        applyStatusLock(ReactionKind::WetElectricity);
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::WetElectricity, element, damage);
    }
    if (hadWet && element == Element::Ice) {
        damagePercent += 0.20f;
        chilledIntensityBonus = std::max(chilledIntensityBonus, kWetIceChillIntensityBonus);
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::WetIce, element, damage);
    }
    if (hadWet && element == Element::Air) {
        RemoveStatus(statuses, faction, roomIndex, entityIndex, StatusKind::Wet);
        knockback += 3.0f;
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::WetAir, element, damage);
    }

    if (hadBurning && element == Element::Water) {
        damagePercent -= 0.10f;
        RemoveStatus(statuses, faction, roomIndex, entityIndex, StatusKind::Burning);
        suppressBaseStatus = true;
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::BurningWater, element, damage);
    }
    if (hadBurning && element == Element::Stone) {
        if (StatusInstance* burning = FindStatus(statuses, StatusKind::Burning)) {
            burning->duration += 1.25f;
        }
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::BurningStone, element, damage);
    }
    if (hadBurning && element == Element::Electricity) {
        const float explosionDamage = damage * 0.30f;
        areaReaction = ReactionKind::BurningElectricity;
        areaReactionDamage = std::max(areaReactionDamage, explosionDamage);
        RemoveStatus(statuses, faction, roomIndex, entityIndex, StatusKind::Burning);
        RemoveStatus(statuses, faction, roomIndex, entityIndex, StatusKind::Charged);
        suppressBaseStatus = true;
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::BurningElectricity, element, explosionDamage);
    }
    if (hadBurning && element == Element::Ice) {
        RemoveStatus(statuses, faction, roomIndex, entityIndex, StatusKind::Burning);
        QueueForcedStatusElement(forcedStatusElements, forcedStatusCount, Element::Water);
        suppressBaseStatus = true;
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::BurningIce, element, damage);
    }
    if (hadBurning && element == Element::Air) {
        if (StatusInstance* burning = FindStatus(statuses, StatusKind::Burning)) {
            burning->intensity *= 1.5f;
        }
        knockback += 3.0f;
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::BurningAir, element, damage);
    }

    if (hadCharged && element == Element::Water) {
        damagePercent += 0.20f;
        RemoveStatus(statuses, faction, roomIndex, entityIndex, StatusKind::Charged);
        applyStatusLock(ReactionKind::ChargedWater);
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::ChargedWater, element, damage);
    }
    if (hadCharged && element == Element::Fire) {
        const float explosionDamage = damage * 0.30f;
        areaReaction = ReactionKind::ChargedFire;
        areaReactionDamage = std::max(areaReactionDamage, explosionDamage);
        RemoveStatus(statuses, faction, roomIndex, entityIndex, StatusKind::Charged);
        RemoveStatus(statuses, faction, roomIndex, entityIndex, StatusKind::Burning);
        suppressBaseStatus = true;
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::ChargedFire, element, explosionDamage);
    }
    if (hadCharged && element == Element::Electricity) {
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::ChargedElectricity, element, damage);
    }
    if (hadCharged && element == Element::Ice) {
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::ChargedIce, element, damage);
    }
    if (hadCharged && element == Element::Air) {
        knockback += 3.0f;
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::ChargedAir, element, damage);
    }

    if (hadChilled && element == Element::Water) {
        damagePercent += 0.20f;
        chilledIntensityBonus = std::max(chilledIntensityBonus, kWetIceChillIntensityBonus);
        QueueForcedStatusElement(forcedStatusElements, forcedStatusCount, Element::Ice);
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::ChilledWater, element, damage);
    }
    if (hadChilled && element == Element::Fire) {
        RemoveStatus(statuses, faction, roomIndex, entityIndex, StatusKind::Chilled);
        QueueForcedStatusElement(forcedStatusElements, forcedStatusCount, Element::Water);
        suppressBaseStatus = true;
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::ChilledFire, element, damage);
    }
    if (hadChilled && element == Element::Stone) {
        if (shield > 0.0f) {
            const float stripped = std::min(shield, damage * 0.50f);
            shield -= stripped;
            Emit(CombatEvent{CombatEventType::ShieldHit, faction, roomIndex, entityIndex, stripped, element, StatusKind::None, ReactionKind::ChilledStone, weapon});
        }
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::ChilledStone, element, damage);
    }
    if (hadChilled && element == Element::Electricity) {
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::ChilledElectricity, element, damage);
    }
    if (hadChilled && element == Element::Ice) {
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::ChilledIce, element, damage);
    }
    if (hadChilled && element == Element::Air) {
        knockback += 3.0f;
        EmitReaction(faction, roomIndex, entityIndex, ReactionKind::ChilledAir, element, damage);
    }

    if (lockCreated) {
        forcedStatusCount = 0;
    }

    float resolvedDamage = damage * std::max(0.0f, 1.0f + damagePercent);
    if (shield > 0.0f && element != Element::Stone) {
        const float absorbed = std::min(shield, resolvedDamage);
        shield -= absorbed;
        resolvedDamage -= absorbed;
        Emit(CombatEvent{CombatEventType::ShieldHit, faction, roomIndex, entityIndex, absorbed, element, StatusKind::None, ReactionKind::None, weapon});
    }

    return std::max(0.0f, resolvedDamage);
}

void CombatSim::ApplyDamageToEnemy(
    int enemyIndex,
    float damage,
    Element element,
    WeaponId weapon,
    Vec2 origin,
    float knockback,
    uint32_t sameHitMask) {
    if (enemyIndex < 0 || enemyIndex >= kMaxEnemies) {
        return;
    }
    EnemyState& enemy = enemies_[enemyIndex];
    if (enemy.hp <= 0.0f) {
        return;
    }

    const bool spreadWet = element == Element::Air && HasStatus(enemy.statuses, StatusKind::Wet);
    std::array<Element, kStatusSlotCount> forcedStatusElements{};
    int forcedStatusCount = 0;
    bool suppressBaseStatus = false;
    ReactionKind areaReaction = ReactionKind::None;
    float areaReactionDamage = 0.0f;
    float chilledIntensityBonus = 0.0f;
    damage = ResolveDamageEvent(
        enemy.statuses,
        Faction::Enemy,
        enemy.roomIndex,
        enemyIndex,
        element,
        weapon,
        damage,
        enemy.shield,
        knockback,
        forcedStatusElements,
        forcedStatusCount,
        suppressBaseStatus,
        areaReaction,
        areaReactionDamage,
        chilledIntensityBonus);

    if (damage > 0.0f) {
        enemy.hp -= damage;
        Emit(CombatEvent{CombatEventType::ActorDamaged, Faction::Enemy, enemy.roomIndex, enemyIndex, damage, element, StatusKind::None, ReactionKind::None, weapon});
    }
    if (knockback > 0.0f) {
        const Vec2 dir = NormalizeOr(enemy.position - origin, Vec2{1.0f, 0.0f});
        enemy.velocity = enemy.velocity + dir * knockback;
        enemy.position = enemy.position + dir * (knockback * 0.05f);
        Emit(CombatEvent{CombatEventType::KnockbackApplied, Faction::Enemy, enemy.roomIndex, enemyIndex, knockback, element, StatusKind::None, ReactionKind::None, weapon});
    }
    if (spreadWet) {
        SpreadWetFromAirHit(enemyIndex, damage, sameHitMask | (1u << static_cast<uint32_t>(enemyIndex)));
    }
    if (areaReaction != ReactionKind::None && areaReactionDamage > 0.0f) {
        TriggerElementalMicroExplosion(enemyIndex, areaReactionDamage, element, areaReaction, weapon);
    }
    if (enemy.hp > 0.0f) {
        for (int i = 0; i < forcedStatusCount; ++i) {
            const Element forcedElement = forcedStatusElements[static_cast<std::size_t>(i)];
            const float intensityBonus = forcedElement == Element::Ice ? chilledIntensityBonus : 0.0f;
            ApplyBaseStatus(enemy.statuses, Faction::Enemy, enemy.roomIndex, enemyIndex, forcedElement, damage, intensityBonus);
            if (forcedElement == Element::Electricity) {
                ResolveChargedDischarge(enemyIndex, damage, weapon);
            }
        }
    }
    if (!suppressBaseStatus && enemy.hp > 0.0f) {
        const float intensityBonus = element == Element::Ice ? chilledIntensityBonus : 0.0f;
        ApplyBaseStatus(enemy.statuses, Faction::Enemy, enemy.roomIndex, enemyIndex, element, damage, intensityBonus);
        if (element == Element::Electricity) {
            ResolveChargedDischarge(enemyIndex, damage, weapon);
        }
    }
    if (enemy.hp <= 0.0f) {
        enemy.hp = 0.0f;
        enemy.active = false;
        Emit(CombatEvent{CombatEventType::ActorKilled, Faction::Enemy, enemy.roomIndex, enemyIndex, 0.0f, element, StatusKind::None, ReactionKind::None, weapon});
    }
}

void CombatSim::SpreadWetFromAirHit(int sourceEnemyIndex, float damage, uint32_t sameHitMask) {
    if (sourceEnemyIndex < 0 || sourceEnemyIndex >= kMaxEnemies) {
        return;
    }

    const EnemyState& source = enemies_[sourceEnemyIndex];
    const float radiusSq = kWetAirSpreadRadius * kWetAirSpreadRadius;
    for (int targetIndex = 0; targetIndex < kMaxEnemies; ++targetIndex) {
        const uint32_t targetBit = 1u << static_cast<uint32_t>(targetIndex);
        if ((sameHitMask & targetBit) != 0u) {
            continue;
        }

        EnemyState& target = enemies_[targetIndex];
        if (!target.active || target.hp <= 0.0f || target.roomIndex != source.roomIndex) {
            continue;
        }
        if (LengthSq(target.position - source.position) > radiusSq) {
            continue;
        }

        ApplyBaseStatus(target.statuses, Faction::Enemy, target.roomIndex, targetIndex, Element::Water, damage);
    }
}

void CombatSim::TriggerElementalMicroExplosion(
    int sourceEnemyIndex,
    float damage,
    Element element,
    ReactionKind reaction,
    WeaponId weapon) {
    if (sourceEnemyIndex < 0 || sourceEnemyIndex >= kMaxEnemies || damage <= 0.0f || reaction == ReactionKind::None) {
        return;
    }

    const EnemyState& source = enemies_[sourceEnemyIndex];
    TriggerElementalMicroExplosionAt(source.position, source.roomIndex, damage, element, reaction, weapon);
}

void CombatSim::TriggerElementalMicroExplosionAt(
    Vec2 center,
    int roomIndex,
    float damage,
    Element element,
    ReactionKind reaction,
    WeaponId weapon) {
    if (damage <= 0.0f || reaction == ReactionKind::None) {
        return;
    }

    const float radiusSq = kElementalMicroExplosionRadius * kElementalMicroExplosionRadius;

    for (int targetIndex = 0; targetIndex < kMaxEnemies; ++targetIndex) {
        EnemyState& target = enemies_[targetIndex];
        if (!target.active || target.hp <= 0.0f || target.roomIndex != roomIndex) {
            continue;
        }
        if (LengthSq(target.position - center) > radiusSq) {
            continue;
        }

        RemoveStatus(target.statuses, Faction::Enemy, target.roomIndex, targetIndex, StatusKind::Burning);
        RemoveStatus(target.statuses, Faction::Enemy, target.roomIndex, targetIndex, StatusKind::Charged);

        float resolvedDamage = damage;
        if (target.shield > 0.0f && element != Element::Stone) {
            const float absorbed = std::min(target.shield, resolvedDamage);
            target.shield -= absorbed;
            resolvedDamage -= absorbed;
            Emit(CombatEvent{CombatEventType::ShieldHit, Faction::Enemy, target.roomIndex, targetIndex, absorbed, element, StatusKind::None, reaction, weapon});
        }
        if (resolvedDamage <= 0.0f) {
            continue;
        }

        target.hp -= resolvedDamage;
        Emit(CombatEvent{CombatEventType::ActorDamaged, Faction::Enemy, target.roomIndex, targetIndex, resolvedDamage, element, StatusKind::None, reaction, weapon});
        if (target.hp <= 0.0f) {
            target.hp = 0.0f;
            target.active = false;
            Emit(CombatEvent{CombatEventType::ActorKilled, Faction::Enemy, target.roomIndex, targetIndex, 0.0f, element, StatusKind::None, reaction, weapon});
        }
    }
}

void CombatSim::ApplyDamageToPlayer(float damage, Element element, WeaponId weapon, Vec2 origin) {
    if (player_.hp <= 0.0f || player_.statuses.invulnerable > 0.0f) {
        return;
    }

    float shield = 0.0f;
    float knockback = 0.0f;
    std::array<Element, kStatusSlotCount> forcedStatusElements{};
    int forcedStatusCount = 0;
    bool suppressBaseStatus = false;
    ReactionKind areaReaction = ReactionKind::None;
    float areaReactionDamage = 0.0f;
    float chilledIntensityBonus = 0.0f;
    damage = ResolveDamageEvent(
        player_.statuses,
        Faction::Player,
        player_.roomIndex,
        -1,
        element,
        weapon,
        damage,
        shield,
        knockback,
        forcedStatusElements,
        forcedStatusCount,
        suppressBaseStatus,
        areaReaction,
        areaReactionDamage,
        chilledIntensityBonus);

    player_.hp -= damage;
    Emit(CombatEvent{CombatEventType::ActorDamaged, Faction::Player, player_.roomIndex, -1, damage, element, StatusKind::None, ReactionKind::None, weapon});
    if (areaReaction != ReactionKind::None && areaReactionDamage > 0.0f) {
        TriggerElementalMicroExplosionAt(player_.position, player_.roomIndex, areaReactionDamage, element, areaReaction, weapon);
    }
    if (player_.hp > 0.0f) {
        for (int i = 0; i < forcedStatusCount; ++i) {
            const Element forcedElement = forcedStatusElements[static_cast<std::size_t>(i)];
            const float intensityBonus = forcedElement == Element::Ice ? chilledIntensityBonus : 0.0f;
            ApplyBaseStatus(player_.statuses, Faction::Player, player_.roomIndex, -1, forcedElement, damage, intensityBonus);
        }
    }
    if (!suppressBaseStatus && player_.hp > 0.0f) {
        const float intensityBonus = element == Element::Ice ? chilledIntensityBonus : 0.0f;
        ApplyBaseStatus(player_.statuses, Faction::Player, player_.roomIndex, -1, element, damage, intensityBonus);
    }
    if (player_.hp <= 0.0f) {
        player_.hp = 0.0f;
        Emit(CombatEvent{CombatEventType::ActorKilled, Faction::Player, player_.roomIndex, -1, 0.0f, element, StatusKind::None, ReactionKind::None, weapon});
    }
    if (knockback > 0.0f) {
        const Vec2 dir = NormalizeOr(player_.position - origin, Vec2{1.0f, 0.0f});
        player_.position = player_.position + dir * (knockback * 0.07f);
        Emit(CombatEvent{CombatEventType::KnockbackApplied, Faction::Player, player_.roomIndex, -1, knockback, element, StatusKind::None, ReactionKind::None, weapon});
    }
}

void CombatSim::ApplyElementAndReactions(EnemyState& enemy, int enemyIndex, Element element, float damage, Vec2 origin) {
    (void)enemy;
    (void)enemyIndex;
    (void)element;
    (void)damage;
    (void)origin;
}

void CombatSim::ApplyElementAndReactionsToPlayer(Element element, float damage, Vec2 origin) {
    (void)damage;
    if (HasStatus(player_.statuses, StatusKind::Wet) && element == Element::Electricity) {
        player_.statuses.statusLock = std::max(player_.statuses.statusLock, 1.2f);
        player_.statuses.stun = std::max(player_.statuses.stun, 0.10f);
        RemoveStatus(player_.statuses, Faction::Player, player_.roomIndex, -1, StatusKind::Wet);
        EmitReaction(Faction::Player, player_.roomIndex, -1, ReactionKind::WetElectricity, element, 0.10f);
    }
    if (HasStatus(player_.statuses, StatusKind::Burning) && element == Element::Water) {
        RemoveStatus(player_.statuses, Faction::Player, player_.roomIndex, -1, StatusKind::Burning);
        EmitReaction(Faction::Player, player_.roomIndex, -1, ReactionKind::BurningWater, element, 0.0f);
    }
    if (element == Element::Air) {
        const Vec2 dir = NormalizeOr(player_.position - origin, Vec2{1.0f, 0.0f});
        player_.position = player_.position + dir * 0.20f;
    }
}

void CombatSim::ApplyBaseStatus(
    ActorStatusSet& statuses,
    Faction faction,
    int roomIndex,
    int entityIndex,
    Element element,
    float damage,
    float intensityBonus) {
    (void)damage;
    if (statuses.statusLock > 0.0f) {
        return;
    }
    const StatusKind status = PersistentStatusForElement(element);
    if (status == StatusKind::None) {
        return;
    }

    const float desiredIntensity = StatusIntensity(status) + (status == StatusKind::Chilled ? intensityBonus : 0.0f);
    if (StatusInstance* existing = FindStatus(statuses, status)) {
        existing->duration = std::max(existing->duration, StatusDuration(status));
        existing->intensity = std::max(existing->intensity, desiredIntensity);
        existing->instanceId = statuses.nextInstanceId++;
        existing->dischargeMask = 0u;
        Emit(CombatEvent{CombatEventType::StatusRefreshed, faction, roomIndex, entityIndex, existing->duration, element, status});
        return;
    }

    StatusInstance* slot = FindFreeStatusSlot(statuses);
    *slot = StatusInstance{
        status,
        element,
        StatusDuration(status),
        desiredIntensity,
        statuses.nextInstanceId++,
        true
    };
    Emit(CombatEvent{CombatEventType::StatusApplied, faction, roomIndex, entityIndex, slot->duration, element, status});
}

void CombatSim::RemoveStatus(ActorStatusSet& statuses, Faction faction, int roomIndex, int entityIndex, StatusKind status) {
    if (StatusInstance* existing = FindStatus(statuses, status)) {
        const Element element = existing->element;
        *existing = StatusInstance{};
        Emit(CombatEvent{CombatEventType::StatusRemoved, faction, roomIndex, entityIndex, 0.0f, element, status});
    }
}

bool CombatSim::HasStatus(const ActorStatusSet& statuses, StatusKind status) const {
    return FindStatus(statuses, status) != nullptr;
}

void CombatSim::EmitReaction(Faction faction, int roomIndex, int entityIndex, ReactionKind reaction, Element element, float value) {
    Emit(CombatEvent{CombatEventType::ReactionTriggered, faction, roomIndex, entityIndex, value, element, StatusKind::None, reaction});
}

void CombatSim::ResolveChargedDischarge(int sourceEnemyIndex, float sourceDamage, WeaponId weapon) {
    if (sourceEnemyIndex < 0 || sourceEnemyIndex >= kMaxEnemies) {
        return;
    }

    EnemyState& source = enemies_[sourceEnemyIndex];
    if (!source.active || source.hp <= 0.0f) {
        return;
    }

    StatusInstance* sourceCharged = FindStatus(source.statuses, StatusKind::Charged);
    if (!sourceCharged) {
        return;
    }

    const uint32_t sourceBit = 1u << static_cast<uint32_t>(sourceEnemyIndex);
    const float radiusSq = kChargedDischargeRadius * kChargedDischargeRadius;
    const float laneRadiusSq = kChargedDischargeLaneRadius * kChargedDischargeLaneRadius;
    const float dischargeDamage = std::max(0.0f, sourceDamage * kChargedDischargeDamageScale);
    if (dischargeDamage <= 0.0f) {
        return;
    }

    for (int targetIndex = 0; targetIndex < kMaxEnemies; ++targetIndex) {
        if (targetIndex == sourceEnemyIndex) {
            continue;
        }

        EnemyState& target = enemies_[targetIndex];
        if (!target.active || target.hp <= 0.0f || target.roomIndex != source.roomIndex) {
            continue;
        }

        StatusInstance* targetCharged = FindStatus(target.statuses, StatusKind::Charged);
        if (!targetCharged) {
            continue;
        }

        const uint32_t targetBit = 1u << static_cast<uint32_t>(targetIndex);
        if ((sourceCharged->dischargeMask & targetBit) != 0u) {
            continue;
        }
        if (LengthSq(target.position - source.position) > radiusSq) {
            continue;
        }

        sourceCharged->dischargeMask |= targetBit;
        targetCharged->dischargeMask |= sourceBit;
        EmitReaction(Faction::Enemy, source.roomIndex, sourceEnemyIndex, ReactionKind::ElectricDischarge, Element::Electricity, dischargeDamage);

        for (int victimIndex = 0; victimIndex < kMaxEnemies; ++victimIndex) {
            EnemyState& victim = enemies_[victimIndex];
            if (!victim.active || victim.hp <= 0.0f || victim.roomIndex != source.roomIndex) {
                continue;
            }
            if (DistanceSqToSegment(victim.position, source.position, target.position) > laneRadiusSq) {
                continue;
            }

            victim.hp -= dischargeDamage;
            Emit(CombatEvent{CombatEventType::ActorDamaged, Faction::Enemy, victim.roomIndex, victimIndex, dischargeDamage, Element::Electricity, StatusKind::None, ReactionKind::ElectricDischarge, weapon});
            if (victim.hp <= 0.0f) {
                victim.hp = 0.0f;
                victim.active = false;
                Emit(CombatEvent{CombatEventType::ActorKilled, Faction::Enemy, victim.roomIndex, victimIndex, 0.0f, Element::Electricity, StatusKind::None, ReactionKind::ElectricDischarge, weapon});
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
            Emit(CombatEvent{CombatEventType::ProjectileExpired, projectile.ownerFaction, projectile.roomIndex, -1, 0.0f, projectile.element, StatusKind::None, ReactionKind::None, projectile.weapon});
            continue;
        }

        if (projectile.ownerFaction == Faction::Player) {
            for (int i = 0; i < kMaxEnemies; ++i) {
                EnemyState& enemy = enemies_[i];
                if (!enemy.active || enemy.roomIndex != projectile.roomIndex || enemy.hp <= 0.0f) {
                    continue;
                }
                const uint32_t bit = 1u << static_cast<uint32_t>(i);
                if ((projectile.hitMask & bit) != 0u) {
                    continue;
                }
                const float hitRadius = projectile.radius + 0.55f;
                if (LengthSq(enemy.position - projectile.position) <= hitRadius * hitRadius) {
                    projectile.hitMask |= bit;
                    ApplyDamageToEnemy(i, projectile.damage, projectile.element, projectile.weapon, projectile.position, projectile.knockback, bit);
                    Emit(CombatEvent{CombatEventType::ProjectileHit, Faction::Enemy, enemy.roomIndex, i, projectile.damage, projectile.element, StatusKind::None, ReactionKind::None, projectile.weapon});
                    if (projectile.pierceRemaining <= 0) {
                        projectile.active = false;
                        break;
                    }
                    --projectile.pierceRemaining;
                }
            }
        } else if (projectile.ownerFaction == Faction::Enemy) {
            if (player_.hp <= 0.0f || player_.roomIndex != projectile.roomIndex) {
                continue;
            }
            const float hitRadius = projectile.radius + 0.45f;
            if (LengthSq(player_.position - projectile.position) <= hitRadius * hitRadius) {
                ApplyDamageToPlayer(projectile.damage, projectile.element, projectile.weapon, projectile.position);
                Emit(CombatEvent{CombatEventType::ProjectileHit, Faction::Player, projectile.roomIndex, -1, projectile.damage, projectile.element, StatusKind::None, ReactionKind::None, projectile.weapon});
                projectile.active = false;
            }
        }
    }
}

void CombatSim::UpdateEnemies(float dt, RoomGraph& world) {
    for (int i = 0; i < kMaxEnemies; ++i) {
        EnemyState& enemy = enemies_[i];
        if (!enemy.active || enemy.hp <= 0.0f || enemy.roomIndex != player_.roomIndex) {
            continue;
        }
        if (enemy.statuses.stun > 0.0f) {
            continue;
        }
        if (EnemyHasPendingAction(i)) {
            enemy.velocity = enemy.velocity * 0.70f;
            continue;
        }

        const Vec2 toPlayer = player_.position - enemy.position;
        const float distanceSq = LengthSq(toPlayer);
        const Vec2 dir = NormalizeOr(toPlayer, Vec2{0.0f, 0.0f});
        const EnemyPressureProfile profile = EnemyProfile(enemy);
        const WeaponSlot attackWeapon = EnemyAttackWeapon(enemy);
        const float speed = profile.speed * StatusMoveMultiplier(enemy.statuses);
        const float distance = std::sqrt(distanceSq);

        if (profile.retreatRange > 0.0f && distance < profile.retreatRange) {
            enemy.velocity = dir * (-speed * 0.78f);
            enemy.position = enemy.position + enemy.velocity * dt;
        } else if (distance > profile.preferredRange * 1.08f) {
            enemy.velocity = dir * speed;
            enemy.position = enemy.position + enemy.velocity * dt;
        } else {
            enemy.velocity = enemy.velocity * 0.78f;
        }

        const int actionSlot = ActionIndex(profile.actionIndex);
        if (distance <= profile.attackRange && enemy.weapon.cooldowns[actionSlot] <= 0.0f) {
            const WeaponSpec& spec = GetWeaponSpec(attackWeapon.weapon);
            const WeaponActionSpec& action = spec.actions[static_cast<std::size_t>(actionSlot)];
            const CombatActionTiming timing = GetWeaponActionTiming(action);
            const float actionDuration = std::max(timing.vfxDuration, timing.impact + timing.recovery);
            enemy.weapon.cooldowns[actionSlot] = action.cooldown * profile.cooldownScale;
            enemy.actionTimer = actionDuration;
            enemy.actionDuration = actionDuration;
            enemy.actionImpactTime = timing.impact;
            enemy.actionRecovery = timing.recovery;
            enemy.activeActionWeapon = attackWeapon.weapon;
            enemy.activeActionElement = attackWeapon.element;
            enemy.activeActionIndex = profile.actionIndex;
            enemy.activeActionShape = action.shape;
            enemy.activeActionOrigin = enemy.position;
            enemy.activeActionTarget = player_.position;
            enemy.activeActionDirection = dir;
            Emit(CombatEvent{
                CombatEventType::WeaponActionUsed,
                Faction::Enemy,
                enemy.roomIndex,
                i,
                static_cast<float>(actionSlot),
                attackWeapon.element,
                StatusKind::None,
                ReactionKind::None,
                attackWeapon.weapon,
                action.shape,
                timing.windup,
                timing.impact,
                timing.recovery,
                timing.vfxDuration,
                PackCombatActionTiming(timing)});

            PendingCombatAction pending{};
            pending.faction = Faction::Enemy;
            pending.roomIndex = enemy.roomIndex;
            pending.entityIndex = i;
            pending.weapon = attackWeapon.weapon;
            pending.element = attackWeapon.element;
            pending.actionIndex = profile.actionIndex;
            pending.action = action;
            pending.origin = enemy.position;
            pending.targetCenter = player_.position;
            pending.direction = dir;
            pending.dashStart = enemy.position;
            pending.dashEnd = enemy.position + dir * action.range;
            pending.damageScale = profile.damageScale;
            pending.impactTimer = timing.impact;
            pending.harassStep = profile.harassStep;
            pending.serial = nextActionSerial_++;
            QueueOrResolveAction(pending, world);
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
