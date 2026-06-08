#pragma once

#include "game/math.h"
#include "game/world_gen.h"

#include <array>
#include <cstdint>

namespace rogue {

constexpr int kMaxEnemies = 24;
constexpr int kMaxProjectiles = 32;
constexpr int kMaxPendingCombatActions = 32;
constexpr int kPlayerWeaponSlots = 3;
constexpr int kMaxCombatEvents = 96;
constexpr int kStatusSlotCount = 4;

struct InputState {
    float moveX = 0.0f;
    float moveY = 0.0f;
    float aimX = 1.0f;
    float aimY = 0.0f;
    bool action1 = false;
    bool action2 = false;
    bool melee = false;
    bool ranged = false;
    bool control = false;
    bool dash = false;
    bool advanceFloor = false;
    int selectWeaponSlot = -1;
    int rewardChoice = -1;
};

inline void ApplyMovementInputBindings(InputState& input, bool wDown, bool aDown, bool sDown, bool dDown) {
    input.moveX = (dDown ? 1.0f : 0.0f) - (aDown ? 1.0f : 0.0f);
    input.moveY = (wDown ? 1.0f : 0.0f) - (sDown ? 1.0f : 0.0f);
}

inline void ApplyScreenAimBinding(InputState& input, float cursorX, float cursorY, float viewportWidth, float viewportHeight) {
    const float cx = viewportWidth * 0.5f;
    const float cy = viewportHeight * 0.5f;
    input.aimX = cursorX - cx;
    input.aimY = cy - cursorY;
}

inline void ApplyInputActionBindings(InputState& input, bool qDown, bool eDown, bool lmbDown, bool rmbDown) {
    const bool primaryAttack = qDown || lmbDown;
    const bool ability = eDown || rmbDown;
    input.action1 = primaryAttack;
    input.action2 = ability;
    input.melee = primaryAttack;
    input.ranged = ability;
    input.control = ability;
}

inline void ApplyNumberInputBinding(InputState& input, int zeroBasedChoice) {
    if (zeroBasedChoice >= 0 && zeroBasedChoice < kPlayerWeaponSlots) {
        input.selectWeaponSlot = zeroBasedChoice;
    }
    if (zeroBasedChoice >= 0 && zeroBasedChoice < 3) {
        input.rewardChoice = zeroBasedChoice;
    }
}

enum class Faction : uint8_t {
    Player,
    Enemy,
    Boss,
    Neutral
};

enum class Element : uint8_t {
    Water,
    Fire,
    Stone,
    Electricity,
    Ice,
    Air,
    None
};

enum class StatusKind : uint8_t {
    Wet,
    Burning,
    Charged,
    Chilled,
    None
};

enum class ReactionKind : uint8_t {
    None,
    WetFire,
    BurningWater,
    WetElectricity,
    WetIce,
    WetAir,
    BurningStone,
    BurningElectricity,
    BurningIce,
    BurningAir,
    ChargedWater,
    ChargedFire,
    ChargedElectricity,
    ChargedIce,
    ChilledWater,
    ChilledFire,
    ChilledStone,
    ChilledElectricity,
    ChilledIce,
    ChargedAir,
    ChilledAir,
    ElectricDischarge
};

enum class WeaponCategory : uint8_t {
    Melee,
    Ranged,
    Magic
};

enum class WeaponId : uint8_t {
    Hammer,
    Spear,
    Katana,
    Pistol,
    Rifle,
    Machinegun,
    Shotgun,
    Staff,
    Scepter,
    Gloves,
    Count
};

enum class WeaponActionIndex : uint8_t {
    Action1,
    Action2
};

enum class AttackShape : uint8_t {
    Circle,
    Cone,
    Projectile,
    Burst,
    Dash,
    Wave,
    Orbit,
    TargetArea
};

enum class CombatEventType : uint8_t {
    WeaponActionUsed,
    ActorDamaged,
    ActorKilled,
    StatusApplied,
    StatusRemoved,
    StatusRefreshed,
    ReactionTriggered,
    ProjectileSpawned,
    ProjectileHit,
    ProjectileExpired,
    StunApplied,
    KnockbackApplied,
    ShieldHit
};

enum class PlayerUpgradeKind : uint8_t {
    Damage,
    Cooldown,
    Speed,
    Area,
    MaxHp,
    Heal
};

struct WeaponActionSpec {
    AttackShape shape = AttackShape::Cone;
    float damage = 0.0f;
    float cooldown = 0.3f;
    float range = 2.0f;
    float radius = 0.5f;
    float speed = 0.0f;
    float duration = 0.0f;
    float coneDot = 0.0f;
    int projectileCount = 0;
    bool piercing = false;
    bool rooted = false;
    bool invulnerableDash = false;
    float windup = 0.0f;
    float impactTime = 0.0f;
    float recovery = 0.0f;
    float vfxDuration = 0.0f;
};

struct CombatActionTiming {
    float windup = 0.0f;
    float impact = 0.0f;
    float recovery = 0.0f;
    float vfxDuration = 0.0f;
};

struct WeaponSpec {
    WeaponId id = WeaponId::Pistol;
    WeaponCategory category = WeaponCategory::Ranged;
    const char* name = "Pistol";
    std::array<const char*, 2> actionNames{};
    uint32_t visualTag = 0;
    std::array<WeaponActionSpec, 2> actions{};
};

struct WeaponSlot {
    WeaponId weapon = WeaponId::Pistol;
    Element element = Element::Fire;
    std::array<float, 2> cooldowns{};
    uint32_t actionCounter = 0;
};

struct StatusInstance {
    StatusKind kind = StatusKind::None;
    Element element = Element::None;
    float duration = 0.0f;
    float intensity = 0.0f;
    uint32_t instanceId = 0;
    bool active = false;
    uint32_t dischargeMask = 0;
};

struct ActorStatusSet {
    std::array<StatusInstance, kStatusSlotCount> slots{};
    float statusLock = 0.0f;
    float stun = 0.0f;
    float invulnerable = 0.0f;
    float stunSequence = 0.0f;
    float stunPulseTimer = 0.0f;
    ReactionKind stunReaction = ReactionKind::None;
    Element stunElement = Element::None;
    WeaponId stunWeapon = WeaponId::Pistol;
    uint32_t nextInstanceId = 1;
};

struct CombatEvent {
    CombatEventType type = CombatEventType::ActorDamaged;
    Faction faction = Faction::Enemy;
    int roomIndex = -1;
    int entityIndex = -1;
    float value = 0.0f;
    Element element = Element::None;
    StatusKind status = StatusKind::None;
    ReactionKind reaction = ReactionKind::None;
    WeaponId weapon = WeaponId::Pistol;
    AttackShape actionShape = AttackShape::Cone;
    float windup = 0.0f;
    float impactTime = 0.0f;
    float recovery = 0.0f;
    float vfxDuration = 0.0f;
    uint32_t payload = 0;
};

struct PlayerState {
    Vec2 position{};
    Vec2 facing{1.0f, 0.0f};
    Vec2 castOrigin{};
    float hp = 100.0f;
    float maxHp = 100.0f;
    float meleeCooldown = 0.0f;
    float rangedCooldown = 0.0f;
    float controlCooldown = 0.0f;
    float dashCooldown = 0.0f;
    float rootTimer = 0.0f;
    float damageMultiplier = 1.0f;
    float cooldownMultiplier = 1.0f;
    float speedMultiplier = 1.0f;
    float areaMultiplier = 1.0f;
    float actionTimer = 0.0f;
    float actionDuration = 0.0f;
    float actionImpactTime = 0.0f;
    float actionRecovery = 0.0f;
    int roomIndex = 0;
    int activeWeaponSlot = 0;
    uint32_t katanaWaveCounter = 0;
    WeaponId activeActionWeapon = WeaponId::Katana;
    Element activeActionElement = Element::Fire;
    WeaponActionIndex activeActionIndex = WeaponActionIndex::Action1;
    AttackShape activeActionShape = AttackShape::Cone;
    Vec2 activeActionOrigin{};
    Vec2 activeActionTarget{};
    Vec2 activeActionDirection{1.0f, 0.0f};
    ActorStatusSet statuses{};
    std::array<WeaponSlot, kPlayerWeaponSlots> weaponSlots{};
};

enum class EnemyKind : uint8_t {
    Brute,
    Caster,
    Skirmisher,
    Bulwark,
    Boss
};

struct EnemyState {
    Vec2 position{};
    Vec2 velocity{};
    float hp = 0.0f;
    float maxHp = 0.0f;
    float shield = 0.0f;
    float attackCooldown = 0.0f;
    float actionTimer = 0.0f;
    float actionDuration = 0.0f;
    float actionImpactTime = 0.0f;
    float actionRecovery = 0.0f;
    int roomIndex = 0;
    EnemyKind kind = EnemyKind::Brute;
    WeaponSlot weapon{};
    WeaponId activeActionWeapon = WeaponId::Hammer;
    Element activeActionElement = Element::Stone;
    WeaponActionIndex activeActionIndex = WeaponActionIndex::Action1;
    AttackShape activeActionShape = AttackShape::Circle;
    Vec2 activeActionOrigin{};
    Vec2 activeActionTarget{};
    Vec2 activeActionDirection{1.0f, 0.0f};
    ActorStatusSet statuses{};
    bool active = false;
};

struct EnemyAttackIntent {
    WeaponId weapon = WeaponId::Pistol;
    Element element = Element::Fire;
    WeaponActionIndex actionIndex = WeaponActionIndex::Action1;
    AttackShape shape = AttackShape::Cone;
    Vec2 direction{1.0f, 0.0f};
    float range = 0.0f;
    float radius = 0.0f;
    float cooldownRatio = 1.0f;
    float readiness = 0.0f;
    bool inRange = false;
    bool active = false;
};

struct ProjectileState {
    Vec2 position{};
    Vec2 velocity{};
    float damage = 0.0f;
    float radius = 0.0f;
    float ttl = 0.0f;
    float knockback = 0.0f;
    int roomIndex = 0;
    int pierceRemaining = 0;
    uint32_t hitMask = 0;
    Faction ownerFaction = Faction::Player;
    Element element = Element::Fire;
    WeaponId weapon = WeaponId::Pistol;
    bool active = false;
};

struct PendingCombatAction {
    bool active = false;
    Faction faction = Faction::Player;
    int roomIndex = -1;
    int entityIndex = -1;
    WeaponId weapon = WeaponId::Pistol;
    Element element = Element::None;
    WeaponActionIndex actionIndex = WeaponActionIndex::Action1;
    WeaponActionSpec action{};
    Vec2 origin{};
    Vec2 targetCenter{};
    Vec2 direction{1.0f, 0.0f};
    Vec2 dashStart{};
    Vec2 dashEnd{};
    float damageScale = 1.0f;
    float impactTimer = 0.0f;
    bool harassStep = false;
    uint32_t serial = 0;
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
    const std::array<CombatEvent, kMaxCombatEvents>& Events() const { return events_; }
    int EventCount() const { return eventCount_; }
    CombatSnapshot Snapshot(const RoomGraph& world) const;
    void PlacePlayer(Vec2 position, int roomIndex);
    int ActivateEnemiesInRoom(int roomIndex);
    int DamageEnemiesInRoom(int roomIndex, float damage);
    int DespawnEnemiesInRoom(int roomIndex);
    void SetPlayerWeaponSlot(int slot, WeaponId weapon, Element element);
    void RestorePlayerProgress(const PlayerState& progress, Vec2 position, int roomIndex);
    void ApplyPlayerUpgrade(PlayerUpgradeKind upgrade, float value);
    int ActiveEnemiesInRoom(int roomIndex) const;
    int LivingEnemiesInRoom(int roomIndex) const;
    int ActiveProjectileCount() const;

private:
    void ClearEvents();
    void Emit(CombatEvent event);
    void TickWeaponCooldowns(float dt);
    void TickStatuses(float dt);
    void TickActionFeedbackTimers(float dt);
    void TickPendingActions(float dt, RoomGraph& world);
    void SyncPlayerActionFeedback();
    void SelectWeaponSlot(int slot);
    void CastWeaponAction(WeaponActionIndex action, Vec2 aim, RoomGraph& world);
    void QueueOrResolveAction(PendingCombatAction action, RoomGraph& world);
    void QueuePendingAction(const PendingCombatAction& action);
    void ResolvePendingAction(const PendingCombatAction& action, RoomGraph& world);
    bool EnemyHasPendingAction(int enemyIndex) const;
    void SpawnProjectile(
        Vec2 position,
        Vec2 dir,
        int roomIndex,
        const WeaponActionSpec& action,
        Element element,
        WeaponId weapon,
        Faction ownerFaction = Faction::Player,
        float damageScale = 1.0f);
    void ApplyAreaAttack(
        const WeaponActionSpec& action,
        Vec2 origin,
        Vec2 targetCenter,
        Vec2 dir,
        Element element,
        WeaponId weapon,
        int roomIndex);
    void ApplyDashAttack(const WeaponActionSpec& action, Vec2 start, Vec2 end, Element element, WeaponId weapon, int roomIndex);
    void ApplyDamageToEnemy(
        int enemyIndex,
        float damage,
        Element element,
        WeaponId weapon,
        Vec2 origin,
        float knockback,
        uint32_t sameHitMask = 0u);
    void SpreadWetFromAirHit(int sourceEnemyIndex, float damage, uint32_t sameHitMask);
    void TriggerElementalMicroExplosion(
        int sourceEnemyIndex,
        float damage,
        Element element,
        ReactionKind reaction,
        WeaponId weapon);
    void TriggerElementalMicroExplosionAt(
        Vec2 center,
        int roomIndex,
        float damage,
        Element element,
        ReactionKind reaction,
        WeaponId weapon);
    void ApplyDamageToPlayer(float damage, Element element, WeaponId weapon, Vec2 origin);
    float ResolveDamageEvent(
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
        float& chilledIntensityBonus);
    void ApplyElementAndReactions(EnemyState& enemy, int enemyIndex, Element element, float damage, Vec2 origin);
    void ApplyElementAndReactionsToPlayer(Element element, float damage, Vec2 origin);
    void ApplyBaseStatus(
        ActorStatusSet& statuses,
        Faction faction,
        int roomIndex,
        int entityIndex,
        Element element,
        float damage,
        float intensityBonus = 0.0f);
    void RemoveStatus(ActorStatusSet& statuses, Faction faction, int roomIndex, int entityIndex, StatusKind status);
    bool HasStatus(const ActorStatusSet& statuses, StatusKind status) const;
    void EmitReaction(Faction faction, int roomIndex, int entityIndex, ReactionKind reaction, Element element, float value);
    void ResolveChargedDischarge(int sourceEnemyIndex, float sourceDamage, WeaponId weapon);
    void UpdateProjectiles(float dt);
    void UpdateEnemies(float dt, RoomGraph& world);

    PlayerState player_{};
    std::array<EnemyState, kMaxEnemies> enemies_{};
    std::array<ProjectileState, kMaxProjectiles> projectiles_{};
    std::array<PendingCombatAction, kMaxPendingCombatActions> pendingActions_{};
    std::array<CombatEvent, kMaxCombatEvents> events_{};
    int eventCount_ = 0;
    uint32_t nextActionSerial_ = 1;
};

const WeaponSpec& GetWeaponSpec(WeaponId weapon);
CombatActionTiming GetWeaponActionTiming(WeaponId weapon, WeaponActionIndex action);
CombatActionTiming GetWeaponActionTiming(const WeaponActionSpec& action);
uint32_t PackCombatActionTiming(CombatActionTiming timing);
CombatActionTiming UnpackCombatActionTiming(uint32_t packed);
float PlayerActionCooldownRatio(const PlayerState& player, WeaponActionIndex action);
bool PlayerActionReady(const PlayerState& player, WeaponActionIndex action);
EnemyAttackIntent EnemyAttackIntentFor(const EnemyState& enemy, const PlayerState& player);
StatusKind PersistentStatusForElement(Element element);
bool ElementHasPersistentStatus(Element element);

}
