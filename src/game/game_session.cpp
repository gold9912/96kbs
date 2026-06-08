#include "game/game_session.h"

#include <algorithm>
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

PlayerUpgradeKind PickUpgrade(uint32_t value) {
    return static_cast<PlayerUpgradeKind>(value % 6u);
}

WeaponId NextWeapon(WeaponId current, uint32_t value) {
    const int count = static_cast<int>(WeaponId::Count);
    const int currentIndex = std::clamp(static_cast<int>(current), 0, count - 1);
    const int step = 1 + static_cast<int>(value % static_cast<uint32_t>(count - 1));
    return static_cast<WeaponId>((currentIndex + step) % count);
}

int ElementIndex(Element element) {
    const int index = static_cast<int>(element);
    return index >= 0 && index < 6 ? index : 0;
}

bool LoadoutHasWeaponExcept(const PlayerState& player, WeaponId weapon, int exceptSlot) {
    for (int i = 0; i < kPlayerWeaponSlots; ++i) {
        if (i != exceptSlot && player.weaponSlots[i].weapon == weapon) {
            return true;
        }
    }
    return false;
}

bool LoadoutHasElementExcept(const PlayerState& player, Element element, int exceptSlot) {
    for (int i = 0; i < kPlayerWeaponSlots; ++i) {
        if (i != exceptSlot && player.weaponSlots[i].element == element) {
            return true;
        }
    }
    return false;
}

WeaponId PickLoadoutDiversityWeapon(const PlayerState& player, int targetSlot, uint32_t value) {
    const int slot = std::clamp(targetSlot, 0, kPlayerWeaponSlots - 1);
    const WeaponId current = player.weaponSlots[slot].weapon;
    const WeaponCategory currentCategory = GetWeaponSpec(current).category;
    const int count = static_cast<int>(WeaponId::Count);
    int start = static_cast<int>(value % static_cast<uint32_t>(count));
    for (int offset = 0; offset < count; ++offset) {
        const WeaponId candidate = static_cast<WeaponId>((start + offset) % count);
        if (candidate != current &&
            GetWeaponSpec(candidate).category == currentCategory &&
            !LoadoutHasWeaponExcept(player, candidate, slot)) {
            return candidate;
        }
    }
    for (int offset = 0; offset < count; ++offset) {
        const WeaponId candidate = static_cast<WeaponId>((start + offset) % count);
        if (candidate != current && !LoadoutHasWeaponExcept(player, candidate, slot)) {
            return candidate;
        }
    }
    return NextWeapon(current, value);
}

Element NextElement(Element current, uint32_t value) {
    constexpr int kElementCount = 6;
    const int currentIndex = std::clamp(static_cast<int>(current), 0, kElementCount - 1);
    const int step = 1 + static_cast<int>(value % static_cast<uint32_t>(kElementCount - 1));
    return static_cast<Element>((currentIndex + step) % kElementCount);
}

int ElementReactionWeight(Element existing, Element incoming) {
    if (existing == Element::None || incoming == Element::None) {
        return 0;
    }
    if (existing == incoming) {
        return 1;
    }

    switch (existing) {
    case Element::Water:
        switch (incoming) {
        case Element::Fire:
            return 5;
        case Element::Stone:
            return 2;
        case Element::Electricity:
            return 9;
        case Element::Ice:
            return 8;
        case Element::Air:
            return 7;
        default:
            return 0;
        }
    case Element::Fire:
        switch (incoming) {
        case Element::Water:
            return 4;
        case Element::Stone:
            return 5;
        case Element::Electricity:
            return 8;
        case Element::Ice:
            return 6;
        case Element::Air:
            return 6;
        default:
            return 0;
        }
    case Element::Electricity:
        switch (incoming) {
        case Element::Water:
            return 9;
        case Element::Fire:
            return 8;
        case Element::Ice:
            return 3;
        case Element::Air:
            return 2;
        default:
            return 0;
        }
    case Element::Ice:
        switch (incoming) {
        case Element::Water:
            return 8;
        case Element::Fire:
            return 6;
        case Element::Stone:
            return 7;
        case Element::Electricity:
            return 3;
        case Element::Air:
            return 2;
        default:
            return 0;
        }
    case Element::Stone:
    case Element::Air:
    case Element::None:
        break;
    }
    return 0;
}

int LoadoutElementSynergyScore(const PlayerState& player, int targetSlot, Element candidate) {
    int score = 0;
    for (int i = 0; i < kPlayerWeaponSlots; ++i) {
        if (i == targetSlot) {
            continue;
        }
        const Element other = player.weaponSlots[i].element;
        score += ElementReactionWeight(other, candidate);
        score += ElementReactionWeight(candidate, other);
    }
    return score;
}

Element BestLoadoutSynergyElement(const PlayerState& player, int targetSlot, Element candidate) {
    Element best = Element::None;
    int bestScore = 0;
    for (int i = 0; i < kPlayerWeaponSlots; ++i) {
        if (i == targetSlot) {
            continue;
        }

        const Element other = player.weaponSlots[i].element;
        const int score = ElementReactionWeight(other, candidate) + ElementReactionWeight(candidate, other);
        if (score > bestScore) {
            bestScore = score;
            best = other;
        }
    }
    return bestScore > 0 ? best : Element::None;
}

int ElementBuildBias(Element element) {
    switch (element) {
    case Element::Water:
        return 30;
    case Element::Electricity:
        return 26;
    case Element::Ice:
        return 22;
    case Element::Fire:
        return 20;
    case Element::Air:
        return 14;
    case Element::Stone:
        return 12;
    case Element::None:
        return 0;
    }
    return 0;
}

Element PickLoadoutSynergyElement(const PlayerState& player, int targetSlot, uint32_t value, Element avoidElement = Element::None) {
    constexpr int kElementCount = 6;
    const int slot = std::clamp(targetSlot, 0, kPlayerWeaponSlots - 1);
    const Element current = player.weaponSlots[slot].element;
    Element best = Element::None;
    int bestScore = -1000000;

    for (int pass = 0; pass < 3; ++pass) {
        for (int offset = 0; offset < kElementCount; ++offset) {
            const Element candidate = static_cast<Element>(offset);
            if (candidate == current) {
                continue;
            }
            if (pass == 0 && avoidElement != Element::None && candidate == avoidElement) {
                continue;
            }
            if (pass <= 1 && LoadoutHasElementExcept(player, candidate, slot)) {
                continue;
            }

            const int synergy = LoadoutElementSynergyScore(player, slot, candidate);
            const int roll = static_cast<int>((value >> (ElementIndex(candidate) * 3)) & 0x7u);
            const int score = synergy * 100 + ElementBuildBias(candidate) + roll;
            if (score > bestScore) {
                bestScore = score;
                best = candidate;
            }
        }

        if (best != Element::None && (pass > 0 || best != avoidElement)) {
            return best;
        }
    }

    return NextElement(current, value);
}

PlayerUpgradeKind PickMeaningfulUpgrade(uint32_t value, const PlayerState& player) {
    PlayerUpgradeKind upgrade = PickUpgrade(value);
    if (upgrade == PlayerUpgradeKind::Heal && player.hp >= player.maxHp - 0.01f) {
        upgrade = PickUpgrade(value + 1u);
        if (upgrade == PlayerUpgradeKind::Heal) {
            upgrade = PlayerUpgradeKind::MaxHp;
        }
    }
    return upgrade;
}

float UpgradeValue(PlayerUpgradeKind upgrade) {
    switch (upgrade) {
    case PlayerUpgradeKind::Damage:
        return 0.18f;
    case PlayerUpgradeKind::Cooldown:
        return 0.88f;
    case PlayerUpgradeKind::Speed:
        return 0.12f;
    case PlayerUpgradeKind::Area:
        return 0.16f;
    case PlayerUpgradeKind::MaxHp:
        return 18.0f;
    case PlayerUpgradeKind::Heal:
        return 28.0f;
    }
    return 0.0f;
}

constexpr float kRewardSelectionRecovery = 34.0f;

bool PlayerInsideControlZone(const PlayerState& player, const RoomObjective& objective) {
    if (objective.controlRadius <= 0.0f) {
        return false;
    }
    const Vec2 delta = player.position - objective.controlPoint;
    return LengthSq(delta) <= objective.controlRadius * objective.controlRadius;
}

}

void GameSession::Start(uint32_t seed, int floorIndex) {
    seed_ = seed;
    floorIndex_ = floorIndex;
    currentRoom_ = 0;
    status_ = RunStatus::InProgress;
    phase_ = RunPhase::Exploring;
    world_ = GenerateWorld(seed, floorIndex_);
    combat_.Reset(world_);
    eventHash_ = HashMix(seed, static_cast<uint32_t>(floorIndex));
    totalKills_ = 0;
    ClearRewardChoice();
    ClearTickEvents();
}

const GameSessionTickResult& GameSession::Tick(const InputState& input, float dt) {
    ClearTickEvents();
    if (status_ == RunStatus::FloorComplete && input.advanceFloor) {
        AdvanceToNextFloor();
        return lastTick_;
    }

    if (status_ != RunStatus::InProgress) {
        return lastTick_;
    }

    if (phase_ == RunPhase::RewardChoice) {
        ApplyRewardChoice(input.rewardChoice);
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

    const int physicalRoom = FindRoomAt(world_, combat_.Player().position);
    const int playerRoom = physicalRoom >= 0 ? physicalRoom : combat_.Player().roomIndex;
    if (playerRoom != currentRoom_ && IsEnterable(playerRoom)) {
        EnterRoom(playerRoom, false);
    }

    UpdateActiveObjective(dt);
    return lastTick_;
}

bool GameSession::TryEnterRoom(int roomIndex) {
    ClearTickEvents();
    if (status_ != RunStatus::InProgress || phase_ != RunPhase::Exploring || !IsEnterable(roomIndex)) {
        return false;
    }
    return EnterRoom(roomIndex, true);
}

int GameSession::DamageRoomEnemies(int roomIndex, float damage) {
    ClearTickEvents();
    if (status_ != RunStatus::InProgress || phase_ != RunPhase::Exploring || roomIndex < 0 || roomIndex >= world_.roomCount) {
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

bool GameSession::AdvanceToNextFloor() {
    ClearTickEvents();
    if (status_ != RunStatus::FloorComplete) {
        return false;
    }

    const PlayerState progress = combat_.Player();
    ++floorIndex_;
    currentRoom_ = 0;
    status_ = RunStatus::InProgress;
    phase_ = RunPhase::Exploring;
    world_ = GenerateWorld(seed_, floorIndex_);
    combat_.Reset(world_);
    combat_.RestorePlayerProgress(progress, world_.rooms[0].center, 0);
    totalKills_ = 0;
    ClearRewardChoice();
    eventHash_ = HashMix(eventHash_, HashWorld(world_));
    Emit(GameEventType::FloorStarted, 0, -1, static_cast<float>(floorIndex_));
    return true;
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

void GameSession::Emit(
    GameEventType type,
    int roomIndex,
    int entityIndex,
    float value,
    WeaponId weapon,
    Element element,
    AttackShape actionShape,
    uint32_t payload) {
    if (type == GameEventType::EnemyKilled) {
        ++totalKills_;
    }
    if (lastTick_.eventCount < kMaxGameEvents) {
        GameEvent& event = lastTick_.events[lastTick_.eventCount++];
        event.type = type;
        event.roomIndex = roomIndex;
        event.entityIndex = entityIndex;
        event.value = value;
        event.weapon = weapon;
        event.element = element;
        event.actionShape = actionShape;
        event.payload = payload;
    }

    eventHash_ = HashMix(eventHash_, static_cast<uint32_t>(type));
    eventHash_ = HashMix(eventHash_, static_cast<uint32_t>(roomIndex + 1024));
    eventHash_ = HashMix(eventHash_, static_cast<uint32_t>(entityIndex + 1024));
    eventHash_ = HashMix(eventHash_, FloatEventHash(value));
    eventHash_ = HashMix(eventHash_, static_cast<uint32_t>(weapon));
    eventHash_ = HashMix(eventHash_, static_cast<uint32_t>(element));
    eventHash_ = HashMix(eventHash_, static_cast<uint32_t>(actionShape));
    eventHash_ = HashMix(eventHash_, payload);
    lastTick_.eventHash = eventHash_;
}

bool GameSession::EnterRoom(int roomIndex, bool placePlayerAtRoomCenter) {
    if (!IsEnterable(roomIndex)) {
        return false;
    }

    if (placePlayerAtRoomCenter) {
        combat_.PlacePlayer(world_.rooms[roomIndex].center, roomIndex);
    } else if (combat_.Player().roomIndex != roomIndex) {
        combat_.PlacePlayer(combat_.Player().position, roomIndex);
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
        completed = combat_.LivingEnemiesInRoom(currentRoom_) == 0;
        break;
    case RoomObjectiveKind::SurviveTimer:
        room.objective.elapsedSeconds += dt;
        completed = room.objective.elapsedSeconds >= room.objective.targetSeconds;
        break;
    case RoomObjectiveKind::ControlPoint:
        if (PlayerInsideControlZone(combat_.Player(), room.objective)) {
            room.objective.elapsedSeconds += dt;
        } else {
            room.objective.elapsedSeconds = Clamp(
                room.objective.elapsedSeconds - dt * kControlObjectiveDecayRate,
                0.0f,
                room.objective.targetSeconds);
        }
        completed = room.objective.targetSeconds <= 0.0001f ||
            room.objective.elapsedSeconds >= room.objective.targetSeconds;
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
    combat_.DespawnEnemiesInRoom(roomIndex);
    Emit(GameEventType::ObjectiveCompleted, roomIndex);
    Emit(GameEventType::RoomCompleted, roomIndex);

    if (roomIndex == world_.roomCount - 1) {
        status_ = RunStatus::FloorComplete;
        Emit(GameEventType::FloorCompleted, roomIndex);
        return;
    }

    if (roomIndex > 0) {
        BeginRewardChoice(roomIndex);
        return;
    }

    OpenPortalsFromRoom(roomIndex);
}

void GameSession::BeginRewardChoice(int roomIndex) {
    rewardRoom_ = roomIndex;
    rewardOptionCount_ = kRewardChoiceCount;
    phase_ = RunPhase::RewardChoice;
    rewardOptions_ = {};

    const uint32_t base = HashMix(HashMix(seed_, static_cast<uint32_t>(floorIndex_ + 4096)), static_cast<uint32_t>(roomIndex + 8192));
    const int firstSlot = (roomIndex + floorIndex_) % kPlayerWeaponSlots;
    const int secondSlot = (firstSlot + 1) % kPlayerWeaponSlots;
    const PlayerState& player = combat_.Player();

    RewardOption weaponSwap{};
    weaponSwap.kind = RewardKind::WeaponSwap;
    weaponSwap.targetSlot = firstSlot;
    weaponSwap.weapon = PickLoadoutDiversityWeapon(player, firstSlot, HashMix(base, 0x9u));
    weaponSwap.element = PickLoadoutSynergyElement(player, firstSlot, HashMix(base, 0x21u));
    weaponSwap.synergyElement = BestLoadoutSynergyElement(player, firstSlot, weaponSwap.element);
    weaponSwap.iconSeed = HashMix(base, 0xa11ceu);
    weaponSwap.active = true;
    rewardOptions_[0] = weaponSwap;

    RewardOption infusion{};
    infusion.kind = RewardKind::ElementInfusion;
    infusion.targetSlot = secondSlot;
    infusion.weapon = player.weaponSlots[secondSlot].weapon;
    infusion.element = PickLoadoutSynergyElement(player, secondSlot, HashMix(base, 0x42u), weaponSwap.element);
    infusion.synergyElement = BestLoadoutSynergyElement(player, secondSlot, infusion.element);
    infusion.iconSeed = HashMix(base, 0xe1e7u);
    infusion.active = true;
    rewardOptions_[1] = infusion;

    RewardOption upgrade{};
    upgrade.kind = RewardKind::PlayerUpgrade;
    upgrade.targetSlot = (firstSlot + 2) % kPlayerWeaponSlots;
    upgrade.element = Element::None;
    upgrade.upgrade = PickMeaningfulUpgrade(HashMix(base, 0x77u), player);
    upgrade.value = UpgradeValue(upgrade.upgrade);
    upgrade.iconSeed = HashMix(base, 0x500du);
    upgrade.active = true;
    rewardOptions_[2] = upgrade;

    Emit(GameEventType::RewardOffered, roomIndex, -1, static_cast<float>(rewardOptionCount_));
}

bool GameSession::ApplyRewardChoice(int choice) {
    if (phase_ != RunPhase::RewardChoice || choice < 0 || choice >= rewardOptionCount_) {
        return false;
    }

    const RewardOption option = rewardOptions_[choice];
    if (!option.active) {
        return false;
    }

    switch (option.kind) {
    case RewardKind::WeaponSwap:
        combat_.SetPlayerWeaponSlot(option.targetSlot, option.weapon, option.element);
        break;
    case RewardKind::ElementInfusion: {
        const int slot = std::clamp(option.targetSlot, 0, kPlayerWeaponSlots - 1);
        const WeaponId weapon = combat_.Player().weaponSlots[slot].weapon;
        combat_.SetPlayerWeaponSlot(slot, weapon, option.element);
        break;
    }
    case RewardKind::PlayerUpgrade:
        combat_.ApplyPlayerUpgrade(option.upgrade, option.value);
        break;
    }
    combat_.ApplyPlayerUpgrade(PlayerUpgradeKind::Heal, kRewardSelectionRecovery);

    const int roomIndex = rewardRoom_;
    Emit(
        GameEventType::RewardSelected,
        roomIndex,
        choice,
        option.value,
        option.weapon,
        option.element,
        AttackShape::Cone,
        PackRewardOverlayOption(option));
    ClearRewardChoice();
    OpenPortalsFromRoom(roomIndex);
    return true;
}

void GameSession::ClearRewardChoice() {
    rewardRoom_ = -1;
    rewardOptionCount_ = 0;
    rewardOptions_ = {};
    if (status_ == RunStatus::InProgress) {
        phase_ = RunPhase::Exploring;
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
    (void)previousPlayer;

    for (int i = 0; i < combat_.EventCount(); ++i) {
        const CombatEvent& event = combat_.Events()[i];
        switch (event.type) {
        case CombatEventType::ActorDamaged:
            if (event.faction == Faction::Player) {
                Emit(GameEventType::PlayerDamaged, currentRoom_, -1, event.value, event.weapon, event.element);
            } else if (event.faction == Faction::Enemy) {
                Emit(GameEventType::EnemyDamaged, event.roomIndex, event.entityIndex, event.value);
            }
            break;
        case CombatEventType::ActorKilled:
            if (event.faction == Faction::Enemy) {
                Emit(GameEventType::EnemyKilled, event.roomIndex, event.entityIndex);
            }
            break;
        case CombatEventType::WeaponActionUsed:
            if (event.faction == Faction::Player) {
                const GameEventType type = event.value > 0.5f
                    ? GameEventType::PlayerAbilityUsed
                    : GameEventType::PlayerActionUsed;
                Emit(
                    type,
                    currentRoom_,
                    static_cast<int>(event.weapon),
                    event.value,
                    event.weapon,
                    event.element,
                    event.actionShape,
                    event.payload);
            }
            break;
        case CombatEventType::StatusApplied:
            Emit(GameEventType::StatusApplied, event.roomIndex, event.entityIndex, static_cast<float>(event.status));
            break;
        case CombatEventType::StatusRemoved:
            Emit(GameEventType::StatusRemoved, event.roomIndex, event.entityIndex, static_cast<float>(event.status));
            break;
        case CombatEventType::StatusRefreshed:
            Emit(GameEventType::StatusRefreshed, event.roomIndex, event.entityIndex, static_cast<float>(event.status));
            break;
        case CombatEventType::ReactionTriggered:
            Emit(GameEventType::ReactionTriggered, event.roomIndex, event.entityIndex, static_cast<float>(event.reaction));
            break;
        default:
            break;
        }
    }

    const auto& enemies = combat_.Enemies();
    for (int i = 0; i < kMaxEnemies; ++i) {
        const EnemyState& before = previousEnemies[i];
        const EnemyState& after = enemies[i];
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
