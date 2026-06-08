#pragma once

#include "game/combat_sim.h"
#include "game/world_gen.h"

#include <array>
#include <cstdint>

namespace rogue {

constexpr int kMaxGameEvents = 64;
constexpr int kRewardChoiceCount = 3;

enum class RunStatus : uint8_t {
    InProgress,
    Failed,
    FloorComplete
};

enum class RunPhase : uint8_t {
    Exploring,
    RewardChoice
};

enum class GameEventType : uint8_t {
    RoomEntered,
    RoomActivated,
    EnemyActivated,
    EnemyDamaged,
    EnemyKilled,
    PlayerDamaged,
    ObjectiveCompleted,
    RoomCompleted,
    RewardOffered,
    RewardSelected,
    PlayerActionUsed,
    PlayerAbilityUsed,
    StatusApplied,
    StatusRemoved,
    StatusRefreshed,
    ReactionTriggered,
    PortalOpened,
    RunFailed,
    FloorCompleted,
    FloorStarted
};

enum class RewardKind : uint8_t {
    WeaponSwap,
    ElementInfusion,
    PlayerUpgrade
};

struct RewardOption {
    RewardKind kind = RewardKind::PlayerUpgrade;
    int targetSlot = 0;
    WeaponId weapon = WeaponId::Pistol;
    Element element = Element::Fire;
    Element synergyElement = Element::None;
    PlayerUpgradeKind upgrade = PlayerUpgradeKind::Damage;
    float value = 0.0f;
    uint32_t iconSeed = 0;
    bool active = false;
};

constexpr uint32_t kRewardOverlayActiveBit = 0x80000000u;
constexpr uint32_t kRewardOverlayKindShift = 0u;
constexpr uint32_t kRewardOverlaySlotShift = 2u;
constexpr uint32_t kRewardOverlayWeaponShift = 4u;
constexpr uint32_t kRewardOverlayElementShift = 8u;
constexpr uint32_t kRewardOverlayUpgradeShift = 11u;
constexpr uint32_t kRewardOverlaySynergyElementShift = 14u;

inline uint32_t PackRewardOverlayOption(const RewardOption& option) {
    if (!option.active) {
        return 0u;
    }

    const uint32_t slot = option.targetSlot >= 0 ? static_cast<uint32_t>(option.targetSlot) : 0u;
    return kRewardOverlayActiveBit |
        ((static_cast<uint32_t>(option.kind) & 0x3u) << kRewardOverlayKindShift) |
        ((slot & 0x3u) << kRewardOverlaySlotShift) |
        ((static_cast<uint32_t>(option.weapon) & 0xfu) << kRewardOverlayWeaponShift) |
        ((static_cast<uint32_t>(option.element) & 0x7u) << kRewardOverlayElementShift) |
        ((static_cast<uint32_t>(option.upgrade) & 0x7u) << kRewardOverlayUpgradeShift) |
        ((static_cast<uint32_t>(option.synergyElement) & 0x7u) << kRewardOverlaySynergyElementShift);
}

inline bool RewardOverlayOptionActive(uint32_t packed) {
    return (packed & kRewardOverlayActiveBit) != 0u;
}

inline RewardKind RewardOverlayOptionKind(uint32_t packed) {
    return static_cast<RewardKind>((packed >> kRewardOverlayKindShift) & 0x3u);
}

inline int RewardOverlayOptionSlot(uint32_t packed) {
    return static_cast<int>((packed >> kRewardOverlaySlotShift) & 0x3u);
}

inline WeaponId RewardOverlayOptionWeapon(uint32_t packed) {
    return static_cast<WeaponId>((packed >> kRewardOverlayWeaponShift) & 0xfu);
}

inline Element RewardOverlayOptionElement(uint32_t packed) {
    return static_cast<Element>((packed >> kRewardOverlayElementShift) & 0x7u);
}

inline PlayerUpgradeKind RewardOverlayOptionUpgrade(uint32_t packed) {
    return static_cast<PlayerUpgradeKind>((packed >> kRewardOverlayUpgradeShift) & 0x7u);
}

inline Element RewardOverlayOptionSynergyElement(uint32_t packed) {
    return static_cast<Element>((packed >> kRewardOverlaySynergyElementShift) & 0x7u);
}

struct GameEvent {
    GameEventType type = GameEventType::RoomEntered;
    int roomIndex = -1;
    int entityIndex = -1;
    float value = 0.0f;
    WeaponId weapon = WeaponId::Pistol;
    Element element = Element::None;
    AttackShape actionShape = AttackShape::Cone;
    uint32_t payload = 0;
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
    bool AdvanceToNextFloor();

    const RoomGraph& World() const { return world_; }
    const CombatSim& Combat() const { return combat_; }
    RunStatus Status() const { return status_; }
    RunPhase Phase() const { return phase_; }
    uint32_t Seed() const { return seed_; }
    int FloorIndex() const { return floorIndex_; }
    int CurrentRoom() const { return currentRoom_; }
    uint32_t EventHash() const { return eventHash_; }
    const GameSessionTickResult& LastTick() const { return lastTick_; }
    const std::array<RewardOption, kRewardChoiceCount>& RewardOptions() const { return rewardOptions_; }
    int RewardOptionCount() const { return rewardOptionCount_; }
    CombatSnapshot Snapshot() const;

private:
    void ClearTickEvents();
    void Emit(
        GameEventType type,
        int roomIndex = -1,
        int entityIndex = -1,
        float value = 0.0f,
        WeaponId weapon = WeaponId::Pistol,
        Element element = Element::None,
        AttackShape actionShape = AttackShape::Cone,
        uint32_t payload = 0);
    bool EnterRoom(int roomIndex, bool placePlayerAtRoomCenter);
    void ActivateRoom(int roomIndex);
    void UpdateActiveObjective(float dt);
    void CompleteRoom(int roomIndex);
    void BeginRewardChoice(int roomIndex);
    bool ApplyRewardChoice(int choice);
    void ClearRewardChoice();
    void OpenPortalsFromRoom(int roomIndex);
    void CollectCombatEvents(
        const PlayerState& previousPlayer,
        const std::array<EnemyState, kMaxEnemies>& previousEnemies);
    bool IsEnterable(int roomIndex) const;

    uint32_t seed_ = kDefaultSeed;
    int floorIndex_ = 0;
    int currentRoom_ = 0;
    RunStatus status_ = RunStatus::InProgress;
    RunPhase phase_ = RunPhase::Exploring;
    RoomGraph world_{};
    CombatSim combat_{};
    GameSessionTickResult lastTick_{};
    uint32_t eventHash_ = 0;
    int totalKills_ = 0;
    int rewardRoom_ = -1;
    int rewardOptionCount_ = 0;
    std::array<RewardOption, kRewardChoiceCount> rewardOptions_{};
};

}
