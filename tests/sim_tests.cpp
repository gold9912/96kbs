#include "game/combat_sim.h"
#include "game/game_session.h"
#include "game/world_gen.h"
#include "render/entity_rt_proxy.h"
#include "render/render_scene.h"
#include "render/visual_style.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <span>
#include <vector>

namespace {

bool Expect(bool condition, const char* label) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", label);
        return false;
    }
    return true;
}

bool HasEvent(const rogue::GameSessionTickResult& tick, rogue::GameEventType type) {
    for (int i = 0; i < tick.eventCount; ++i) {
        if (tick.events[i].type == type) {
            return true;
        }
    }
    return false;
}

bool HasRewardSelectedPayload(const rogue::GameSessionTickResult& tick, int choice, const rogue::RewardOption& option) {
    for (int i = 0; i < tick.eventCount; ++i) {
        const rogue::GameEvent& event = tick.events[i];
        if (event.type == rogue::GameEventType::RewardSelected &&
            event.entityIndex == choice &&
            event.value == option.value &&
            event.weapon == option.weapon &&
            event.element == option.element &&
            event.payload == rogue::PackRewardOverlayOption(option)) {
            return true;
        }
    }
    return false;
}

bool HasCombatEvent(const rogue::CombatSim& sim, rogue::CombatEventType type) {
    for (int i = 0; i < sim.EventCount(); ++i) {
        if (sim.Events()[i].type == type) {
            return true;
        }
    }
    return false;
}

bool HasCombatEventWithFaction(const rogue::CombatSim& sim, rogue::CombatEventType type, rogue::Faction faction) {
    for (int i = 0; i < sim.EventCount(); ++i) {
        if (sim.Events()[i].type == type && sim.Events()[i].faction == faction) {
            return true;
        }
    }
    return false;
}

int CountCombatEventWithFaction(const rogue::CombatSim& sim, rogue::CombatEventType type, rogue::Faction faction) {
    int count = 0;
    for (int i = 0; i < sim.EventCount(); ++i) {
        if (sim.Events()[i].type == type && sim.Events()[i].faction == faction) {
            ++count;
        }
    }
    return count;
}

bool HasCombatEventValue(const rogue::CombatSim& sim, rogue::CombatEventType type, rogue::Faction faction, float value) {
    for (int i = 0; i < sim.EventCount(); ++i) {
        if (sim.Events()[i].type == type &&
            sim.Events()[i].faction == faction &&
            sim.Events()[i].value == value) {
            return true;
        }
    }
    return false;
}

bool HasCombatStatusEvent(const rogue::CombatSim& sim, rogue::Faction faction, rogue::StatusKind status) {
    for (int i = 0; i < sim.EventCount(); ++i) {
        const rogue::CombatEvent& event = sim.Events()[i];
        if (event.type == rogue::CombatEventType::StatusApplied &&
            event.faction == faction &&
            event.status == status) {
            return true;
        }
    }
    return false;
}

bool HasCombatActionEvent(
    const rogue::CombatSim& sim,
    rogue::Faction faction,
    rogue::WeaponId weapon,
    rogue::Element element,
    rogue::AttackShape shape,
    float actionValue) {
    for (int i = 0; i < sim.EventCount(); ++i) {
        const rogue::CombatEvent& event = sim.Events()[i];
        if (event.type == rogue::CombatEventType::WeaponActionUsed &&
            event.faction == faction &&
            event.weapon == weapon &&
            event.element == element &&
            event.actionShape == shape &&
            event.value == actionValue) {
            return true;
        }
    }
    return false;
}

bool HasCombatReaction(const rogue::CombatSim& sim, rogue::ReactionKind reaction) {
    for (int i = 0; i < sim.EventCount(); ++i) {
        if (sim.Events()[i].type == rogue::CombatEventType::ReactionTriggered &&
            sim.Events()[i].reaction == reaction) {
            return true;
        }
    }
    return false;
}

int CountCombatReaction(const rogue::CombatSim& sim, rogue::ReactionKind reaction) {
    int count = 0;
    for (int i = 0; i < sim.EventCount(); ++i) {
        if (sim.Events()[i].type == rogue::CombatEventType::ReactionTriggered &&
            sim.Events()[i].reaction == reaction) {
            ++count;
        }
    }
    return count;
}

int CountCombatDamageWithReaction(const rogue::CombatSim& sim, rogue::ReactionKind reaction) {
    int count = 0;
    for (int i = 0; i < sim.EventCount(); ++i) {
        if (sim.Events()[i].type == rogue::CombatEventType::ActorDamaged &&
            sim.Events()[i].reaction == reaction) {
            ++count;
        }
    }
    return count;
}

bool HasCombatDamageWithReactionForEntity(const rogue::CombatSim& sim, rogue::ReactionKind reaction, int entityIndex) {
    for (int i = 0; i < sim.EventCount(); ++i) {
        const rogue::CombatEvent& event = sim.Events()[i];
        if (event.type == rogue::CombatEventType::ActorDamaged &&
            event.reaction == reaction &&
            event.entityIndex == entityIndex) {
            return true;
        }
    }
    return false;
}

bool HasCombatStunReaction(const rogue::CombatSim& sim, rogue::ReactionKind reaction) {
    for (int i = 0; i < sim.EventCount(); ++i) {
        const rogue::CombatEvent& event = sim.Events()[i];
        if (event.type == rogue::CombatEventType::StunApplied &&
            event.reaction == reaction) {
            return true;
        }
    }
    return false;
}

bool HasGameEvent(const rogue::GameSessionTickResult& tick, rogue::GameEventType type, float value) {
    for (int i = 0; i < tick.eventCount; ++i) {
        if (tick.events[i].type == type && tick.events[i].value == value) {
            return true;
        }
    }
    return false;
}

bool HasGameActionEvent(
    const rogue::GameSessionTickResult& tick,
    rogue::GameEventType type,
    rogue::WeaponId weapon,
    rogue::Element element,
    rogue::AttackShape shape,
    float actionValue) {
    for (int i = 0; i < tick.eventCount; ++i) {
        const rogue::GameEvent& event = tick.events[i];
        if (event.type == type &&
            event.weapon == weapon &&
            event.element == element &&
            event.actionShape == shape &&
            event.value == actionValue) {
            return true;
        }
    }
    return false;
}

int FirstLivingEnemyInRoom(const rogue::CombatSim& sim, int roomIndex) {
    const auto& enemies = sim.Enemies();
    for (int i = 0; i < rogue::kMaxEnemies; ++i) {
        if (enemies[i].roomIndex == roomIndex && enemies[i].hp > 0.0f) {
            return i;
        }
    }
    return -1;
}

bool FindCastingPositionForTarget(
    const rogue::RoomGraph& world,
    int roomIndex,
    rogue::Vec2 target,
    float distance,
    rogue::Vec2& position,
    rogue::Vec2& aim) {
    const std::array<rogue::Vec2, 8> directions{
        rogue::Vec2{1.0f, 0.0f},
        rogue::Vec2{-1.0f, 0.0f},
        rogue::Vec2{0.0f, 1.0f},
        rogue::Vec2{0.0f, -1.0f},
        rogue::NormalizeOr(rogue::Vec2{1.0f, 1.0f}, rogue::Vec2{1.0f, 0.0f}),
        rogue::NormalizeOr(rogue::Vec2{-1.0f, 1.0f}, rogue::Vec2{-1.0f, 0.0f}),
        rogue::NormalizeOr(rogue::Vec2{1.0f, -1.0f}, rogue::Vec2{1.0f, 0.0f}),
        rogue::NormalizeOr(rogue::Vec2{-1.0f, -1.0f}, rogue::Vec2{-1.0f, 0.0f})
    };

    for (rogue::Vec2 dir : directions) {
        const rogue::Vec2 candidate = target - dir * distance;
        if (rogue::FindRoomAt(world, candidate) == roomIndex &&
            rogue::IsTraversablePosition(world, roomIndex, candidate)) {
            position = candidate;
            aim = dir;
            return true;
        }
    }
    return false;
}

bool EnemyHasStatus(const rogue::CombatSim& sim, int enemyIndex, rogue::StatusKind status) {
    if (enemyIndex < 0 || enemyIndex >= rogue::kMaxEnemies) {
        return false;
    }
    for (const rogue::StatusInstance& instance : sim.Enemies()[enemyIndex].statuses.slots) {
        if (instance.active && instance.kind == status) {
            return true;
        }
    }
    return false;
}

float EnemyStatusIntensity(const rogue::CombatSim& sim, int enemyIndex, rogue::StatusKind status) {
    if (enemyIndex < 0 || enemyIndex >= rogue::kMaxEnemies) {
        return 0.0f;
    }
    for (const rogue::StatusInstance& instance : sim.Enemies()[enemyIndex].statuses.slots) {
        if (instance.active && instance.kind == status) {
            return instance.intensity;
        }
    }
    return 0.0f;
}

bool PlayerHasStatus(const rogue::CombatSim& sim, rogue::StatusKind status) {
    for (const rogue::StatusInstance& instance : sim.Player().statuses.slots) {
        if (instance.active && instance.kind == status) {
            return true;
        }
    }
    return false;
}

float FirstEventValue(const rogue::GameSessionTickResult& tick, rogue::GameEventType type) {
    for (int i = 0; i < tick.eventCount; ++i) {
        if (tick.events[i].type == type) {
            return tick.events[i].value;
        }
    }
    return -1.0f;
}

bool HasProxyKind(std::span<const rogue::EntityRTProxy> proxies, rogue::EntityProxyKind kind) {
    for (const rogue::EntityRTProxy& proxy : proxies) {
        if (proxy.kind == kind) {
            return true;
        }
    }
    return false;
}

int CountProxyKind(std::span<const rogue::EntityRTProxy> proxies, rogue::EntityProxyKind kind) {
    int count = 0;
    for (const rogue::EntityRTProxy& proxy : proxies) {
        if (proxy.kind == kind) {
            ++count;
        }
    }
    return count;
}

int CountEnemyTellProxies(std::span<const rogue::EntityRTProxy> proxies) {
    return CountProxyKind(proxies, rogue::EntityProxyKind::EnemyTellCone) +
        CountProxyKind(proxies, rogue::EntityProxyKind::EnemyTellLine) +
        CountProxyKind(proxies, rogue::EntityProxyKind::EnemyTellRing);
}

const rogue::EntityRTProxy* FindProxyKind(std::span<const rogue::EntityRTProxy> proxies, rogue::EntityProxyKind kind) {
    for (const rogue::EntityRTProxy& proxy : proxies) {
        if (proxy.kind == kind) {
            return &proxy;
        }
    }
    return nullptr;
}

void ChooseReward(rogue::GameSession& session, int choice) {
    rogue::InputState input{};
    input.rewardChoice = choice;
    session.Tick(input, 0.0f);
}

void ReachFirstReward(rogue::GameSession& session, uint32_t seed = 0x96u) {
    session.Start(seed);
    session.TryEnterRoom(1);
    session.DamageRoomEnemies(1, 9999.0f);
}

bool PlayerUpgradeChanged(
    const rogue::PlayerState& before,
    const rogue::PlayerState& after,
    rogue::PlayerUpgradeKind upgrade) {
    switch (upgrade) {
    case rogue::PlayerUpgradeKind::Damage:
        return after.damageMultiplier > before.damageMultiplier;
    case rogue::PlayerUpgradeKind::Cooldown:
        return after.cooldownMultiplier < before.cooldownMultiplier;
    case rogue::PlayerUpgradeKind::Speed:
        return after.speedMultiplier > before.speedMultiplier;
    case rogue::PlayerUpgradeKind::Area:
        return after.areaMultiplier > before.areaMultiplier;
    case rogue::PlayerUpgradeKind::MaxHp:
        return after.maxHp > before.maxHp && after.hp > before.hp;
    case rogue::PlayerUpgradeKind::Heal:
        return after.hp > before.hp;
    }
    return false;
}

int WeaponIndex(rogue::WeaponId weapon) {
    const int index = static_cast<int>(weapon);
    return index >= 0 && index < static_cast<int>(rogue::WeaponId::Count) ? index : 0;
}

int ElementIndex(rogue::Element element) {
    const int index = static_cast<int>(element);
    return index >= 0 && index < 6 ? index : 0;
}

bool LoadoutHasWeaponExcept(const rogue::PlayerState& player, rogue::WeaponId weapon, int exceptSlot) {
    for (int i = 0; i < rogue::kPlayerWeaponSlots; ++i) {
        if (i != exceptSlot && player.weaponSlots[i].weapon == weapon) {
            return true;
        }
    }
    return false;
}

bool LoadoutHasElementExcept(const rogue::PlayerState& player, rogue::Element element, int exceptSlot) {
    for (int i = 0; i < rogue::kPlayerWeaponSlots; ++i) {
        if (i != exceptSlot && player.weaponSlots[i].element == element) {
            return true;
        }
    }
    return false;
}

int DistinctWeaponCount(const rogue::PlayerState& player) {
    std::array<bool, static_cast<std::size_t>(rogue::WeaponId::Count)> seen{};
    int count = 0;
    for (const rogue::WeaponSlot& slot : player.weaponSlots) {
        const int index = WeaponIndex(slot.weapon);
        if (!seen[static_cast<std::size_t>(index)]) {
            seen[static_cast<std::size_t>(index)] = true;
            ++count;
        }
    }
    return count;
}

int DistinctElementCount(const rogue::PlayerState& player) {
    std::array<bool, 6> seen{};
    int count = 0;
    for (const rogue::WeaponSlot& slot : player.weaponSlots) {
        const int index = ElementIndex(slot.element);
        if (!seen[static_cast<std::size_t>(index)]) {
            seen[static_cast<std::size_t>(index)] = true;
            ++count;
        }
    }
    return count;
}

int TestElementReactionWeight(rogue::Element existing, rogue::Element incoming) {
    if (existing == rogue::Element::None || incoming == rogue::Element::None) {
        return 0;
    }
    if (existing == incoming) {
        return 1;
    }

    switch (existing) {
    case rogue::Element::Water:
        switch (incoming) {
        case rogue::Element::Fire:
            return 5;
        case rogue::Element::Stone:
            return 2;
        case rogue::Element::Electricity:
            return 9;
        case rogue::Element::Ice:
            return 8;
        case rogue::Element::Air:
            return 7;
        default:
            return 0;
        }
    case rogue::Element::Fire:
        switch (incoming) {
        case rogue::Element::Water:
            return 4;
        case rogue::Element::Stone:
            return 5;
        case rogue::Element::Electricity:
            return 8;
        case rogue::Element::Ice:
            return 6;
        case rogue::Element::Air:
            return 6;
        default:
            return 0;
        }
    case rogue::Element::Electricity:
        switch (incoming) {
        case rogue::Element::Water:
            return 9;
        case rogue::Element::Fire:
            return 8;
        case rogue::Element::Ice:
            return 3;
        case rogue::Element::Air:
            return 2;
        default:
            return 0;
        }
    case rogue::Element::Ice:
        switch (incoming) {
        case rogue::Element::Water:
            return 8;
        case rogue::Element::Fire:
            return 6;
        case rogue::Element::Stone:
            return 7;
        case rogue::Element::Electricity:
            return 3;
        case rogue::Element::Air:
            return 2;
        default:
            return 0;
        }
    case rogue::Element::Stone:
    case rogue::Element::Air:
    case rogue::Element::None:
        break;
    }
    return 0;
}

int RewardElementSynergyScore(const rogue::PlayerState& player, int targetSlot, rogue::Element candidate) {
    int score = 0;
    for (int i = 0; i < rogue::kPlayerWeaponSlots; ++i) {
        if (i == targetSlot) {
            continue;
        }
        const rogue::Element other = player.weaponSlots[i].element;
        score += TestElementReactionWeight(other, candidate);
        score += TestElementReactionWeight(candidate, other);
    }
    return score;
}

bool TickToward(rogue::GameSession& session, rogue::Vec2 target, int maxFrames) {
    for (int i = 0; i < maxFrames && session.Status() == rogue::RunStatus::InProgress; ++i) {
        const rogue::Vec2 delta = target - session.Combat().Player().position;
        const float distanceSq = rogue::LengthSq(delta);
        rogue::InputState input{};
        const rogue::Vec2 dir = rogue::NormalizeOr(delta, rogue::Vec2{1.0f, 0.0f});
        input.aimX = dir.x;
        input.aimY = dir.y;
        if (distanceSq > 0.20f * 0.20f) {
            input.moveX = dir.x;
            input.moveY = dir.y;
        }
        session.Tick(input, 1.0f / 60.0f);
        if (distanceSq <= 0.20f * 0.20f) {
            return true;
        }
    }
    return false;
}

bool CompleteCurrentRoomObjective(rogue::GameSession& session) {
    const int roomIndex = session.CurrentRoom();
    if (roomIndex < 0 || roomIndex >= session.World().roomCount) {
        return false;
    }

    const rogue::RoomObjectiveKind kind = session.World().rooms[roomIndex].objective.kind;
    if (kind == rogue::RoomObjectiveKind::SurviveTimer) {
        session.DamageRoomEnemies(roomIndex, 9999.0f);
        for (int i = 0; i < 240 && session.Status() == rogue::RunStatus::InProgress; ++i) {
            session.Tick(rogue::InputState{}, 1.0f / 60.0f);
            if (session.World().rooms[roomIndex].lifecycle == rogue::RoomLifecycle::Completed) {
                return true;
            }
        }
        return false;
    }

    if (kind == rogue::RoomObjectiveKind::ControlPoint) {
        const rogue::RoomObjective objective = session.World().rooms[roomIndex].objective;
        session.DamageRoomEnemies(roomIndex, 9999.0f);
        if (!TickToward(session, objective.controlPoint, 240)) {
            return false;
        }
        for (int i = 0; i < 360 && session.Status() == rogue::RunStatus::InProgress; ++i) {
            const rogue::Vec2 delta = objective.controlPoint - session.Combat().Player().position;
            const rogue::Vec2 aim = rogue::NormalizeOr(delta, rogue::Vec2{1.0f, 0.0f});
            rogue::InputState hold{};
            hold.aimX = aim.x;
            hold.aimY = aim.y;
            session.Tick(hold, 1.0f / 60.0f);
            if (session.World().rooms[roomIndex].lifecycle == rogue::RoomLifecycle::Completed) {
                return true;
            }
        }
        return false;
    }

    session.DamageRoomEnemies(roomIndex, 9999.0f);
    return session.World().rooms[roomIndex].lifecycle == rogue::RoomLifecycle::Completed;
}

int NearestActiveEnemyInRoom(const rogue::CombatSim& sim, int roomIndex, rogue::Vec2 position) {
    int best = -1;
    float bestDistanceSq = 999999.0f;
    const auto& enemies = sim.Enemies();
    for (int i = 0; i < rogue::kMaxEnemies; ++i) {
        const rogue::EnemyState& enemy = enemies[i];
        if (!enemy.active || enemy.roomIndex != roomIndex || enemy.hp <= 0.0f) {
            continue;
        }
        const float distanceSq = rogue::LengthSq(enemy.position - position);
        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            best = i;
        }
    }
    return best;
}

void FillKitingCombatInput(
    const rogue::CombatSim& sim,
    int roomIndex,
    int activeSlot,
    int frame,
    rogue::InputState& input) {
    input = rogue::InputState{};
    input.selectWeaponSlot = activeSlot;

    const rogue::PlayerState& player = sim.Player();
    input.aimX = player.facing.x;
    input.aimY = player.facing.y;

    const int enemyIndex = NearestActiveEnemyInRoom(sim, roomIndex, player.position);
    if (enemyIndex < 0) {
        return;
    }

    const rogue::EnemyState& enemy = sim.Enemies()[enemyIndex];
    const rogue::Vec2 toEnemy = enemy.position - player.position;
    const float distanceSq = rogue::LengthSq(toEnemy);
    const rogue::Vec2 aim = rogue::NormalizeOr(toEnemy, player.facing);
    input.aimX = aim.x;
    input.aimY = aim.y;

    if (distanceSq < 4.2f * 4.2f) {
        input.moveX = -aim.x;
        input.moveY = -aim.y;
        input.dash = distanceSq < 2.4f * 2.4f;
    } else if (distanceSq > 7.2f * 7.2f) {
        input.moveX = aim.x;
        input.moveY = aim.y;
    } else {
        const float strafe = (frame / 45) % 2 == 0 ? 1.0f : -1.0f;
        input.moveX = -aim.y * strafe;
        input.moveY = aim.x * strafe;
    }

    const int slotIndex = activeSlot >= 0 && activeSlot < rogue::kPlayerWeaponSlots ? activeSlot : 0;
    const rogue::WeaponSlot& slot = player.weaponSlots[slotIndex];
    input.action1 = slot.cooldowns[0] <= 0.0001f;
    input.action2 = slot.cooldowns[1] <= 0.0001f;
}

bool HasGeometryNearControlPoint(const rogue::RenderScene& scene, const rogue::RoomObjective& objective) {
    const float radiusSq = objective.controlRadius * objective.controlRadius;
    for (const rogue::RtTriangle& tri : scene.generatedGeometry.triangles) {
        const std::array<rogue::Vec3, 3> vertices{tri.a.position, tri.b.position, tri.c.position};
        for (const rogue::Vec3& vertex : vertices) {
            const float dx = vertex.x - objective.controlPoint.x;
            const float dz = vertex.z - objective.controlPoint.y;
            if (dx * dx + dz * dz <= radiusSq) {
                return true;
            }
        }
    }
    return false;
}

int FindActiveEnemyKind(const rogue::CombatSim& sim, rogue::EnemyKind kind);

void FillControlObjectiveInput(
    const rogue::GameSession& session,
    int roomIndex,
    int activeSlot,
    int frame,
    rogue::InputState& input) {
    input = rogue::InputState{};
    input.selectWeaponSlot = activeSlot;

    const rogue::PlayerState& player = session.Combat().Player();
    const rogue::RoomObjective& objective = session.World().rooms[roomIndex].objective;
    const int enemyIndex = NearestActiveEnemyInRoom(session.Combat(), roomIndex, player.position);
    rogue::Vec2 aim = player.facing;
    if (enemyIndex >= 0) {
        aim = rogue::NormalizeOr(session.Combat().Enemies()[enemyIndex].position - player.position, player.facing);
    } else {
        aim = rogue::NormalizeOr(objective.controlPoint - player.position, player.facing);
    }
    input.aimX = aim.x;
    input.aimY = aim.y;

    const rogue::Vec2 toControl = objective.controlPoint - player.position;
    const float distanceToControlSq = rogue::LengthSq(toControl);
    const float settleRadius = objective.controlRadius * 0.48f;
    if (distanceToControlSq > settleRadius * settleRadius) {
        const rogue::Vec2 move = rogue::NormalizeOr(toControl, rogue::Vec2{0.0f, 0.0f});
        input.moveX = move.x;
        input.moveY = move.y;
    } else {
        const float pulse = (frame / 36) % 2 == 0 ? 0.16f : -0.16f;
        input.moveX = -aim.y * pulse;
        input.moveY = aim.x * pulse;
    }

    const int slotIndex = activeSlot >= 0 && activeSlot < rogue::kPlayerWeaponSlots ? activeSlot : 0;
    const rogue::WeaponSlot& slot = player.weaponSlots[slotIndex];
    input.action1 = slot.cooldowns[0] <= 0.0001f;
    input.action2 = slot.cooldowns[1] <= 0.0001f;
}

void ChoosePlayableReward(rogue::GameSession& session) {
    rogue::InputState choose{};
    const int choice = session.RewardOptionCount() > 2 ? 2 : 0;
    rogue::ApplyNumberInputBinding(choose, choice);
    session.Tick(choose, 0.0f);
}

struct PlayableCombatSmokeResult {
    bool completed = false;
    bool sawPrimary = false;
    bool sawAbility = false;
    bool sawEnemyDamage = false;
    bool sawLivePressureHud = false;
    bool sawPrimaryCooldownHud = false;
    bool sawAbilityCooldownHud = false;
    int frames = 0;
    float hp = 0.0f;
};

PlayableCombatSmokeResult PlayFirstCombatRoomWithRealInputs(rogue::GameSession& session, int maxFrames) {
    PlayableCombatSmokeResult result{};
    if (!session.TryEnterRoom(1)) {
        result.hp = session.Combat().Player().hp;
        return result;
    }

    for (int frame = 0; frame < maxFrames && session.Status() == rogue::RunStatus::InProgress; ++frame) {
        result.frames = frame + 1;
        if (session.Phase() == rogue::RunPhase::RewardChoice ||
            session.World().rooms[1].lifecycle == rogue::RoomLifecycle::Completed) {
            result.completed = true;
            break;
        }

        rogue::InputState input{};
        FillKitingCombatInput(session.Combat(), 1, 1, frame, input);

        session.Tick(input, 1.0f / 60.0f);
        result.sawPrimary = result.sawPrimary ||
            HasGameEvent(session.LastTick(), rogue::GameEventType::PlayerActionUsed, 0.0f);
        result.sawAbility = result.sawAbility ||
            HasGameEvent(session.LastTick(), rogue::GameEventType::PlayerAbilityUsed, 1.0f);
        result.sawEnemyDamage = result.sawEnemyDamage ||
            HasEvent(session.LastTick(), rogue::GameEventType::EnemyDamaged);
        if (!result.sawLivePressureHud || !result.sawPrimaryCooldownHud || !result.sawAbilityCooldownHud) {
            const rogue::RenderScene scene = rogue::BuildRenderScene(
                session.World(),
                session.Combat(),
                640,
                360,
                static_cast<float>(frame) / 60.0f,
                static_cast<uint32_t>(frame));
            result.sawLivePressureHud = result.sawLivePressureHud ||
                (scene.overlay.overlayActiveSlot == 2u && scene.overlay.overlayActiveEnemies > 0u);
            result.sawPrimaryCooldownHud = result.sawPrimaryCooldownHud ||
                (scene.overlay.overlayActiveSlot == 2u && scene.overlay.overlayQReadyPercent < 100u);
            result.sawAbilityCooldownHud = result.sawAbilityCooldownHud ||
                (scene.overlay.overlayActiveSlot == 2u && scene.overlay.overlayEReadyPercent < 100u);
        }
    }

    result.hp = session.Combat().Player().hp;
    result.completed = result.completed ||
        session.Phase() == rogue::RunPhase::RewardChoice ||
        session.World().rooms[1].lifecycle == rogue::RoomLifecycle::Completed;
    return result;
}

struct PlayableEarlyRunSmokeResult {
    PlayableCombatSmokeResult room1{};
    bool rewardSelected = false;
    bool portalOpened = false;
    bool rewardCleared = false;
    bool room2Entered = false;
    bool room2Completed = false;
    bool room2RewardOffered = false;
    bool sawRoom2Primary = false;
    bool sawRoom2Ability = false;
    bool sawRoom2EnemyDamage = false;
    bool sawRoom2SurvivalHud = false;
    bool sawRoom2SurvivalProgressHud = false;
    bool sawRoom2LivePressureHud = false;
    int room2Frames = 0;
    float hp = 0.0f;
};

PlayableEarlyRunSmokeResult PlayEarlyRunWithRealInputs(rogue::GameSession& session, int room1Frames, int room2Frames) {
    PlayableEarlyRunSmokeResult result{};
    result.room1 = PlayFirstCombatRoomWithRealInputs(session, room1Frames);
    if (!result.room1.completed || session.Phase() != rogue::RunPhase::RewardChoice) {
        result.hp = session.Combat().Player().hp;
        return result;
    }

    ChoosePlayableReward(session);
    result.rewardSelected = HasEvent(session.LastTick(), rogue::GameEventType::RewardSelected);
    result.portalOpened = HasEvent(session.LastTick(), rogue::GameEventType::PortalOpened);
    result.rewardCleared = session.Phase() == rogue::RunPhase::Exploring &&
        session.RewardOptionCount() == 0 &&
        session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Available;

    result.room2Entered = session.TryEnterRoom(2);
    if (!result.room2Entered) {
        result.hp = session.Combat().Player().hp;
        return result;
    }

    for (int frame = 0; frame < room2Frames && session.Status() == rogue::RunStatus::InProgress; ++frame) {
        result.room2Frames = frame + 1;
        if (session.Phase() == rogue::RunPhase::RewardChoice ||
            session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Completed) {
            result.room2Completed = true;
            break;
        }

        rogue::InputState input{};
        FillKitingCombatInput(session.Combat(), 2, 1, frame, input);
        session.Tick(input, 1.0f / 60.0f);

        result.sawRoom2Primary = result.sawRoom2Primary ||
            HasGameEvent(session.LastTick(), rogue::GameEventType::PlayerActionUsed, 0.0f);
        result.sawRoom2Ability = result.sawRoom2Ability ||
            HasGameEvent(session.LastTick(), rogue::GameEventType::PlayerAbilityUsed, 1.0f);
        result.sawRoom2EnemyDamage = result.sawRoom2EnemyDamage ||
            HasEvent(session.LastTick(), rogue::GameEventType::EnemyDamaged);

        if (!result.sawRoom2SurvivalHud ||
            !result.sawRoom2SurvivalProgressHud ||
            !result.sawRoom2LivePressureHud) {
            const rogue::RenderScene scene = rogue::BuildRenderScene(
                session.World(),
                session.Combat(),
                640,
                360,
                20.0f + static_cast<float>(frame) / 60.0f,
                static_cast<uint32_t>(1200 + frame));
            result.sawRoom2SurvivalHud = result.sawRoom2SurvivalHud ||
                scene.overlay.overlayObjectiveKind == static_cast<uint32_t>(rogue::RoomObjectiveKind::SurviveTimer);
            result.sawRoom2SurvivalProgressHud = result.sawRoom2SurvivalProgressHud ||
                (scene.overlay.overlayObjectiveKind == static_cast<uint32_t>(rogue::RoomObjectiveKind::SurviveTimer) &&
                 scene.overlay.overlayObjectiveProgressPercent > 0u &&
                 scene.overlay.overlayObjectiveProgressPercent < 100u);
            result.sawRoom2LivePressureHud = result.sawRoom2LivePressureHud ||
                (scene.overlay.overlayCurrentRoom == 3u && scene.overlay.overlayActiveEnemies > 0u);
        }
    }

    result.hp = session.Combat().Player().hp;
    result.room2Completed = result.room2Completed ||
        session.Phase() == rogue::RunPhase::RewardChoice ||
        session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Completed;
    result.room2RewardOffered = session.Phase() == rogue::RunPhase::RewardChoice &&
        session.RewardOptionCount() == rogue::kRewardChoiceCount;
    return result;
}

struct PlayableControlRunSmokeResult {
    PlayableEarlyRunSmokeResult early{};
    bool room2RewardSelected = false;
    bool room2PortalOpened = false;
    bool room3Unlocked = false;
    bool room3Entered = false;
    bool room3Completed = false;
    bool room3RewardOffered = false;
    bool sawRoom3Primary = false;
    bool sawRoom3Ability = false;
    bool sawRoom3EnemyDamage = false;
    bool sawRoom3ControlHud = false;
    bool sawRoom3ControlProgressHud = false;
    bool sawRoom3ControlMarkerGeometry = false;
    bool sawRoom3LivePressureHud = false;
    int room3Frames = 0;
    float hp = 0.0f;
};

PlayableControlRunSmokeResult PlayControlRunWithRealInputs(
    rogue::GameSession& session,
    int room1Frames,
    int room2Frames,
    int room3Frames) {
    PlayableControlRunSmokeResult result{};
    result.early = PlayEarlyRunWithRealInputs(session, room1Frames, room2Frames);
    if (!result.early.room2RewardOffered || session.Phase() != rogue::RunPhase::RewardChoice) {
        result.hp = session.Combat().Player().hp;
        return result;
    }

    ChoosePlayableReward(session);
    result.room2RewardSelected = HasEvent(session.LastTick(), rogue::GameEventType::RewardSelected);
    result.room2PortalOpened = HasEvent(session.LastTick(), rogue::GameEventType::PortalOpened);
    result.room3Unlocked = session.Phase() == rogue::RunPhase::Exploring &&
        session.RewardOptionCount() == 0 &&
        session.World().rooms[3].lifecycle == rogue::RoomLifecycle::Available;

    result.room3Entered = session.TryEnterRoom(3);
    if (!result.room3Entered) {
        result.hp = session.Combat().Player().hp;
        return result;
    }

    for (int frame = 0; frame < room3Frames && session.Status() == rogue::RunStatus::InProgress; ++frame) {
        result.room3Frames = frame + 1;
        if (session.Phase() == rogue::RunPhase::RewardChoice ||
            session.World().rooms[3].lifecycle == rogue::RoomLifecycle::Completed) {
            result.room3Completed = true;
            break;
        }

        rogue::InputState input{};
        FillControlObjectiveInput(session, 3, 1, frame, input);
        session.Tick(input, 1.0f / 60.0f);

        result.sawRoom3Primary = result.sawRoom3Primary ||
            HasGameEvent(session.LastTick(), rogue::GameEventType::PlayerActionUsed, 0.0f);
        result.sawRoom3Ability = result.sawRoom3Ability ||
            HasGameEvent(session.LastTick(), rogue::GameEventType::PlayerAbilityUsed, 1.0f);
        result.sawRoom3EnemyDamage = result.sawRoom3EnemyDamage ||
            HasEvent(session.LastTick(), rogue::GameEventType::EnemyDamaged);

        if (!result.sawRoom3ControlHud ||
            !result.sawRoom3ControlProgressHud ||
            !result.sawRoom3ControlMarkerGeometry ||
            !result.sawRoom3LivePressureHud) {
            const rogue::RenderScene scene = rogue::BuildRenderScene(
                session.World(),
                session.Combat(),
                640,
                360,
                40.0f + static_cast<float>(frame) / 60.0f,
                static_cast<uint32_t>(2400 + frame));
            const rogue::RoomObjective& objective = session.World().rooms[3].objective;
            result.sawRoom3ControlHud = result.sawRoom3ControlHud ||
                scene.overlay.overlayObjectiveKind == static_cast<uint32_t>(rogue::RoomObjectiveKind::ControlPoint);
            result.sawRoom3ControlProgressHud = result.sawRoom3ControlProgressHud ||
                (scene.overlay.overlayObjectiveKind == static_cast<uint32_t>(rogue::RoomObjectiveKind::ControlPoint) &&
                    scene.overlay.overlayObjectiveProgressPercent > 0u &&
                    scene.overlay.overlayObjectiveProgressPercent < 100u);
            result.sawRoom3ControlMarkerGeometry = result.sawRoom3ControlMarkerGeometry ||
                HasGeometryNearControlPoint(scene, objective);
            result.sawRoom3LivePressureHud = result.sawRoom3LivePressureHud ||
                (scene.overlay.overlayCurrentRoom == 4u && scene.overlay.overlayActiveEnemies > 0u);
        }
    }

    result.hp = session.Combat().Player().hp;
    result.room3Completed = result.room3Completed ||
        session.Phase() == rogue::RunPhase::RewardChoice ||
        session.World().rooms[3].lifecycle == rogue::RoomLifecycle::Completed;
    result.room3RewardOffered = session.Phase() == rogue::RunPhase::RewardChoice &&
        session.RewardOptionCount() == rogue::kRewardChoiceCount;
    return result;
}

struct PlayableKillRoomSmokeResult {
    bool entered = false;
    bool completed = false;
    bool rewardOffered = false;
    bool floorCompleted = false;
    bool sawPrimary = false;
    bool sawAbility = false;
    bool sawEnemyDamage = false;
    bool sawLivePressureHud = false;
    bool sawBossProxy = false;
    bool sawBossPhase2 = false;
    bool sawBossPhase3 = false;
    int frames = 0;
    float hp = 0.0f;
};

PlayableKillRoomSmokeResult PlayKillRoomWithRealInputs(
    rogue::GameSession& session,
    int roomIndex,
    int activeSlot,
    int maxFrames,
    bool trackBoss) {
    PlayableKillRoomSmokeResult result{};
    result.entered = session.TryEnterRoom(roomIndex);
    if (!result.entered) {
        result.hp = session.Combat().Player().hp;
        return result;
    }

    for (int frame = 0; frame < maxFrames && session.Status() == rogue::RunStatus::InProgress; ++frame) {
        result.frames = frame + 1;
        if (session.Phase() == rogue::RunPhase::RewardChoice ||
            session.World().rooms[roomIndex].lifecycle == rogue::RoomLifecycle::Completed) {
            result.completed = true;
            break;
        }

        rogue::InputState input{};
        FillKitingCombatInput(session.Combat(), roomIndex, activeSlot, frame, input);
        session.Tick(input, 1.0f / 60.0f);

        result.sawPrimary = result.sawPrimary ||
            HasGameEvent(session.LastTick(), rogue::GameEventType::PlayerActionUsed, 0.0f);
        result.sawAbility = result.sawAbility ||
            HasGameEvent(session.LastTick(), rogue::GameEventType::PlayerAbilityUsed, 1.0f);
        result.sawEnemyDamage = result.sawEnemyDamage ||
            HasEvent(session.LastTick(), rogue::GameEventType::EnemyDamaged);

        const int bossIndex = trackBoss ? FindActiveEnemyKind(session.Combat(), rogue::EnemyKind::Boss) : -1;
        if (bossIndex >= 0) {
            const rogue::EnemyState& boss = session.Combat().Enemies()[bossIndex];
            const float ratio = boss.maxHp > 0.0001f ? boss.hp / boss.maxHp : 1.0f;
            result.sawBossPhase2 = result.sawBossPhase2 || ratio <= 0.66f;
            result.sawBossPhase3 = result.sawBossPhase3 || ratio <= 0.33f;
        }

        if (!result.sawLivePressureHud || (trackBoss && !result.sawBossProxy)) {
            const rogue::RenderScene scene = rogue::BuildRenderScene(
                session.World(),
                session.Combat(),
                640,
                360,
                60.0f + static_cast<float>(frame) / 60.0f,
                static_cast<uint32_t>(3600 + roomIndex * 600 + frame));
            result.sawLivePressureHud = result.sawLivePressureHud ||
                (scene.overlay.overlayCurrentRoom == static_cast<uint32_t>(roomIndex + 1) &&
                 scene.overlay.overlayActiveEnemies > 0u);
            result.sawBossProxy = result.sawBossProxy || HasProxyKind(scene.proxies, rogue::EntityProxyKind::EnemyBoss);
        }
    }

    result.hp = session.Combat().Player().hp;
    result.completed = result.completed || session.World().rooms[roomIndex].lifecycle == rogue::RoomLifecycle::Completed;
    result.rewardOffered = session.Phase() == rogue::RunPhase::RewardChoice &&
        session.RewardOptionCount() == rogue::kRewardChoiceCount;
    result.floorCompleted = session.Status() == rogue::RunStatus::FloorComplete;
    return result;
}

struct PlayableFullRunSmokeResult {
    PlayableControlRunSmokeResult control{};
    bool room3RewardSelected = false;
    bool room3PortalOpened = false;
    bool room4Unlocked = false;
    PlayableKillRoomSmokeResult room4{};
    bool room4RewardSelected = false;
    bool room4PortalOpened = false;
    bool finalRoomUnlocked = false;
    PlayableKillRoomSmokeResult final{};
    bool floorCompletedEvent = false;
    float hp = 0.0f;
};

PlayableFullRunSmokeResult PlayFullRunWithRealInputs(rogue::GameSession& session) {
    PlayableFullRunSmokeResult result{};
    result.control = PlayControlRunWithRealInputs(session, 900, 210, 420);
    if (!result.control.room3RewardOffered || session.Phase() != rogue::RunPhase::RewardChoice) {
        result.hp = session.Combat().Player().hp;
        return result;
    }

    ChoosePlayableReward(session);
    result.room3RewardSelected = HasEvent(session.LastTick(), rogue::GameEventType::RewardSelected);
    result.room3PortalOpened = HasEvent(session.LastTick(), rogue::GameEventType::PortalOpened);
    result.room4Unlocked = session.Phase() == rogue::RunPhase::Exploring &&
        session.World().rooms[4].lifecycle == rogue::RoomLifecycle::Available;

    result.room4 = PlayKillRoomWithRealInputs(session, 4, 1, 1080, false);
    if (!result.room4.rewardOffered || session.Phase() != rogue::RunPhase::RewardChoice) {
        result.hp = session.Combat().Player().hp;
        return result;
    }

    ChoosePlayableReward(session);
    result.room4RewardSelected = HasEvent(session.LastTick(), rogue::GameEventType::RewardSelected);
    result.room4PortalOpened = HasEvent(session.LastTick(), rogue::GameEventType::PortalOpened);
    const int finalRoom = session.World().roomCount - 1;
    result.finalRoomUnlocked = session.Phase() == rogue::RunPhase::Exploring &&
        session.World().rooms[finalRoom].lifecycle == rogue::RoomLifecycle::Available;

    result.final = PlayKillRoomWithRealInputs(session, finalRoom, 1, 1500, true);
    result.floorCompletedEvent = HasEvent(session.LastTick(), rogue::GameEventType::FloorCompleted);
    result.hp = session.Combat().Player().hp;
    return result;
}

void CastGlovesElementAtEnemy(rogue::CombatSim& sim, rogue::RoomGraph& world, int enemyIndex, rogue::Element element) {
    const rogue::Vec2 enemyPos = sim.Enemies()[enemyIndex].position;
    sim.PlacePlayer(enemyPos + rogue::Vec2{-0.75f, 0.0f}, sim.Enemies()[enemyIndex].roomIndex);
    sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, element);

    rogue::InputState cast{};
    cast.action1 = true;
    cast.aimX = 1.0f;
    sim.Tick(cast, 1.0f / 60.0f, world);
}

void CastPistolElementAtEnemy(rogue::CombatSim& sim, rogue::RoomGraph& world, int enemyIndex, rogue::Element element) {
    const rogue::EnemyState& enemy = sim.Enemies()[enemyIndex];
    rogue::Vec2 position{};
    rogue::Vec2 aim{};
    if (!FindCastingPositionForTarget(world, enemy.roomIndex, enemy.position, 0.85f, position, aim)) {
        position = enemy.position + rogue::Vec2{-0.85f, 0.0f};
        aim = rogue::Vec2{1.0f, 0.0f};
    }

    sim.PlacePlayer(position, enemy.roomIndex);
    sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Pistol, element);

    rogue::InputState shot{};
    shot.action1 = true;
    shot.aimX = aim.x;
    shot.aimY = aim.y;
    sim.Tick(shot, 1.0f / 60.0f, world);
}

std::array<rogue::Vec2, 3> PlaceFirstThreeRoomSpawnsForDischarge(rogue::RoomGraph& world) {
    std::array<rogue::Vec2, 3> spawnPositions{};
    int found = 0;
    const rogue::Vec2 center = world.rooms[1].center;
    const std::array<rogue::Vec2, 3> positions{
        center + rogue::Vec2{-3.0f, 0.0f},
        center + rogue::Vec2{3.0f, 0.0f},
        center + rogue::Vec2{0.0f, 0.0f}
    };

    for (int i = 0; i < world.spawnCount && found < 3; ++i) {
        if (world.spawns[i].roomIndex != 1) {
            continue;
        }
        world.spawns[i].position = positions[static_cast<std::size_t>(found)];
        world.spawns[i].archetype = 0;
        spawnPositions[static_cast<std::size_t>(found)] = world.spawns[i].position;
        ++found;
    }
    return found == 3 ? spawnPositions : std::array<rogue::Vec2, 3>{};
}

std::array<rogue::Vec2, 3> PlaceFirstThreeRoomSpawnsForMicroExplosion(rogue::RoomGraph& world) {
    std::array<rogue::Vec2, 3> spawnPositions{};
    int found = 0;
    const rogue::Vec2 center = world.rooms[1].center;
    const std::array<rogue::Vec2, 3> positions{
        center,
        center + rogue::Vec2{0.0f, 1.55f},
        center + rogue::Vec2{3.55f, 0.0f}
    };

    for (int i = 0; i < world.spawnCount && found < 3; ++i) {
        if (world.spawns[i].roomIndex != 1) {
            continue;
        }
        world.spawns[i].position = positions[static_cast<std::size_t>(found)];
        world.spawns[i].archetype = 0;
        spawnPositions[static_cast<std::size_t>(found)] = world.spawns[i].position;
        ++found;
    }
    return found == 3 ? spawnPositions : std::array<rogue::Vec2, 3>{};
}

std::array<rogue::Vec2, 3> PlaceRoomSpawnsForPlayerMicroExplosion(rogue::RoomGraph& world) {
    std::array<rogue::Vec2, 3> spawnPositions{};
    int found = 0;
    const rogue::Vec2 center = world.rooms[1].center;
    const std::array<rogue::Vec2, 3> positions{
        center + rogue::Vec2{-1.70f, 0.0f},
        center + rogue::Vec2{4.60f, 0.0f},
        center + rogue::Vec2{0.0f, 1.40f}
    };
    const std::array<int, 3> archetypes{4, 1, 0};

    for (int i = 0; i < world.spawnCount && found < 3; ++i) {
        if (world.spawns[i].roomIndex != 1) {
            continue;
        }
        world.spawns[i].position = positions[static_cast<std::size_t>(found)];
        world.spawns[i].archetype = archetypes[static_cast<std::size_t>(found)];
        spawnPositions[static_cast<std::size_t>(found)] = world.spawns[i].position;
        ++found;
    }
    return found == 3 ? spawnPositions : std::array<rogue::Vec2, 3>{};
}

std::array<rogue::Vec2, 3> PlaceFirstThreeRoomSpawnsForWetAir(rogue::RoomGraph& world) {
    std::array<rogue::Vec2, 3> spawnPositions{};
    int found = 0;
    const rogue::Vec2 center = world.rooms[1].center;
    const std::array<rogue::Vec2, 3> positions{
        center,
        center + rogue::Vec2{1.35f, 0.0f},
        center + rogue::Vec2{3.05f, 0.0f}
    };

    for (int i = 0; i < world.spawnCount && found < 3; ++i) {
        if (world.spawns[i].roomIndex != 1) {
            continue;
        }
        world.spawns[i].position = positions[static_cast<std::size_t>(found)];
        world.spawns[i].archetype = 0;
        spawnPositions[static_cast<std::size_t>(found)] = world.spawns[i].position;
        ++found;
    }
    return found == 3 ? spawnPositions : std::array<rogue::Vec2, 3>{};
}

int FindActiveEnemyNear(const rogue::CombatSim& sim, int roomIndex, rogue::Vec2 position) {
    int best = -1;
    float bestDistanceSq = 0.45f * 0.45f;
    for (int i = 0; i < rogue::kMaxEnemies; ++i) {
        const rogue::EnemyState& enemy = sim.Enemies()[i];
        if (!enemy.active || enemy.roomIndex != roomIndex || enemy.hp <= 0.0f) {
            continue;
        }
        const float distanceSq = rogue::LengthSq(enemy.position - position);
        if (distanceSq <= bestDistanceSq) {
            bestDistanceSq = distanceSq;
            best = i;
        }
    }
    return best;
}

int FindActiveEnemyKind(const rogue::CombatSim& sim, rogue::EnemyKind kind) {
    for (int i = 0; i < rogue::kMaxEnemies; ++i) {
        const rogue::EnemyState& enemy = sim.Enemies()[i];
        if (enemy.active && enemy.kind == kind && enemy.hp > 0.0f) {
            return i;
        }
    }
    return -1;
}

int FindActiveShieldedEnemy(const rogue::CombatSim& sim) {
    for (int i = 0; i < rogue::kMaxEnemies; ++i) {
        const rogue::EnemyState& enemy = sim.Enemies()[i];
        if (enemy.active && enemy.hp > 0.0f && enemy.shield > 0.0f) {
            return i;
        }
    }
    return -1;
}

bool TickUntilPlayerStatus(rogue::CombatSim& sim, rogue::RoomGraph& world, rogue::StatusKind status, int maxFrames) {
    for (int i = 0; i < maxFrames; ++i) {
        sim.Tick(rogue::InputState{}, 1.0f / 60.0f, world);
        if (PlayerHasStatus(sim, status)) {
            return true;
        }
    }
    return false;
}

bool HasActiveProjectileWithOwner(const rogue::CombatSim& sim, rogue::Faction ownerFaction) {
    for (const rogue::ProjectileState& projectile : sim.Projectiles()) {
        if (projectile.active && projectile.ownerFaction == ownerFaction) {
            return true;
        }
    }
    return false;
}

bool PlacePlayerAtEnemyRange(
    rogue::CombatSim& sim,
    const rogue::RoomGraph& world,
    int enemyIndex,
    float distance) {
    if (enemyIndex < 0 || enemyIndex >= rogue::kMaxEnemies) {
        return false;
    }
    const rogue::EnemyState& enemy = sim.Enemies()[enemyIndex];
    rogue::Vec2 position{};
    rogue::Vec2 aim{};
    if (!FindCastingPositionForTarget(world, enemy.roomIndex, enemy.position, distance, position, aim)) {
        return false;
    }
    sim.PlacePlayer(position, enemy.roomIndex);
    return true;
}

void KeepOnlyRoomSpawn(rogue::RoomGraph& world, int roomIndex, rogue::Vec2 position, int archetype) {
    bool kept = false;
    for (int i = 0; i < world.spawnCount; ++i) {
        rogue::SpawnPoint& spawn = world.spawns[i];
        if (spawn.roomIndex != roomIndex) {
            continue;
        }
        if (!kept) {
            spawn.position = position;
            spawn.archetype = archetype;
            kept = true;
        } else {
            spawn.roomIndex = 0;
            spawn.position = world.rooms[0].center;
            spawn.archetype = 0;
        }
    }
}

void KeepOnlyFinalBossSpawn(rogue::RoomGraph& world) {
    const int finalRoom = world.roomCount - 1;
    for (int i = 0; i < world.spawnCount; ++i) {
        rogue::SpawnPoint& spawn = world.spawns[i];
        if (spawn.roomIndex == finalRoom && spawn.archetype != 4) {
            spawn.roomIndex = 0;
            spawn.position = world.rooms[0].center;
            spawn.archetype = 0;
        }
    }
}

const rogue::ProjectileState* FirstActiveProjectile(const rogue::CombatSim& sim, rogue::Faction ownerFaction) {
    for (const rogue::ProjectileState& projectile : sim.Projectiles()) {
        if (projectile.active && projectile.ownerFaction == ownerFaction) {
            return &projectile;
        }
    }
    return nullptr;
}

bool TestWorldDeterminism() {
    const rogue::RoomGraph a = rogue::GenerateWorld(0x12345678u);
    const rogue::RoomGraph b = rogue::GenerateWorld(0x12345678u);
    const rogue::RoomGraph c = rogue::GenerateWorld(0x87654321u);

    bool ok = true;
    ok &= Expect(rogue::HashWorld(a) == rogue::HashWorld(b), "same seed produces same world hash");
    ok &= Expect(rogue::HashWorld(a) != rogue::HashWorld(c), "different seeds produce different world hash");
    ok &= Expect(a.roomCount >= 4, "world has multiple rooms");
    ok &= Expect(a.portalCount == a.roomCount - 1, "starter graph is a connected chain");
    ok &= Expect(a.spawnCount > 0, "world has enemy spawns");
    ok &= Expect(a.rooms[0].lifecycle == rogue::RoomLifecycle::Completed, "start room is completed");
    ok &= Expect(a.rooms[1].lifecycle == rogue::RoomLifecycle::Available, "first combat room is available");
    ok &= Expect(a.rooms[2].lifecycle == rogue::RoomLifecycle::Locked, "later rooms start locked");
    ok &= Expect(a.rooms[1].objective.kind == rogue::RoomObjectiveKind::KillAll, "room 1 uses kill-all objective");
    ok &= Expect(a.rooms[2].objective.kind == rogue::RoomObjectiveKind::SurviveTimer, "room 2 uses survival objective");
    ok &= Expect(a.rooms[3].objective.kind == rogue::RoomObjectiveKind::ControlPoint, "room 3 uses control point objective");
    ok &= Expect(a.rooms[3].objective.targetSeconds == rogue::kControlObjectiveHoldSeconds, "control room has hold duration");
    ok &= Expect(a.rooms[3].objective.controlRadius == rogue::kControlObjectiveRadius, "control room has control radius");
    ok &= Expect(rogue::LengthSq(a.rooms[3].objective.controlPoint - a.rooms[3].center) > 0.5f, "control point is offset from room entry");
    ok &= Expect(a.rooms[a.roomCount - 1].objective.kind == rogue::RoomObjectiveKind::KillAll, "final room is a boss kill-all objective");
    return ok;
}

bool TestWorldEnemyArchetypeDiversityAndPressure() {
    const rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    std::array<bool, 5> seen{};
    std::array<int, rogue::kMaxRooms> spawnsByRoom{};
    std::array<int, rogue::kMaxRooms> pressureByRoom{};
    int bossSpawns = 0;

    for (int i = 0; i < world.spawnCount; ++i) {
        const rogue::SpawnPoint& spawn = world.spawns[i];
        if (spawn.archetype >= 0 && spawn.archetype < static_cast<int>(seen.size())) {
            seen[static_cast<std::size_t>(spawn.archetype)] = true;
        }
        if (spawn.archetype == 4 && spawn.roomIndex == world.roomCount - 1) {
            ++bossSpawns;
        }
        if (spawn.roomIndex >= 0 && spawn.roomIndex < rogue::kMaxRooms) {
            ++spawnsByRoom[static_cast<std::size_t>(spawn.roomIndex)];
            pressureByRoom[static_cast<std::size_t>(spawn.roomIndex)] += rogue::SpawnArchetypePressureCost(spawn.archetype);
        }
    }

    bool ok = true;
    ok &= Expect(seen[0] && seen[1] && seen[2] && seen[3], "world spawns all four enemy archetypes");
    ok &= Expect(seen[4] && bossSpawns == 1, "world spawns one final boss archetype");
    ok &= Expect(spawnsByRoom[1] >= 3, "first combat room starts with real pressure");
    ok &= Expect(spawnsByRoom[static_cast<std::size_t>(world.roomCount - 1)] > spawnsByRoom[1], "late room pressure ramps up");
    ok &= Expect(world.spawnCount <= rogue::kMaxSpawns, "enemy pressure stays within spawn budget");
    ok &= Expect(rogue::RoomSpawnPressure(world, 1) == 7, "room 1 uses the teaching pressure budget");
    ok &= Expect(rogue::RoomSpawnPressure(world, 2) == 8, "room 2 gently increases pressure");
    ok &= Expect(rogue::RoomSpawnPressure(world, 3) == 10, "room 3 increases mixed-role pressure");
    ok &= Expect(rogue::RoomSpawnPressure(world, 4) == 12, "room 4 reaches late-room pressure");
    ok &= Expect(rogue::RoomSpawnPressure(world, world.roomCount - 1) == 17, "final room combines boss and support pressure");
    for (int room = 1; room < world.roomCount; ++room) {
        ok &= Expect(pressureByRoom[static_cast<std::size_t>(room)] == rogue::RoomSpawnPressure(world, room), "room pressure helper matches spawn scan");
        ok &= Expect(pressureByRoom[static_cast<std::size_t>(room)] > 0, "combat room has positive pressure");
        for (int i = 0; i < world.spawnCount; ++i) {
            const rogue::SpawnPoint& a = world.spawns[i];
            if (a.roomIndex != room) {
                continue;
            }
            if (a.archetype < 4) {
                ok &= Expect(rogue::LengthSq(a.position - world.rooms[room].center) > 0.85f * 0.85f, "spawn pacing keeps enemies away from room center");
            }
            for (int j = i + 1; j < world.spawnCount; ++j) {
                const rogue::SpawnPoint& b = world.spawns[j];
                if (b.roomIndex == room) {
                    ok &= Expect(rogue::LengthSq(a.position - b.position) > 0.90f * 0.90f, "spawn pacing spreads enemies apart");
                }
            }
        }
    }
    return ok;
}

bool TestPortalTraversalPredicates() {
    const rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    bool ok = true;

    ok &= Expect(rogue::IsTraversablePosition(world, 0, world.rooms[0].center), "current room center is traversable");
    ok &= Expect(rogue::IsTraversablePosition(world, 0, world.portals[0].position), "open start portal path is traversable");
    ok &= Expect(rogue::IsTraversablePosition(world, 0, world.rooms[1].center), "connected available room is traversable through open portal");
    ok &= Expect(!rogue::IsTraversablePosition(world, 0, world.rooms[2].center), "locked room center is not traversable");
    ok &= Expect(!rogue::IsTraversablePosition(world, 1, world.portals[1].position), "closed next portal path is not traversable");
    return ok;
}

bool TestPortalTraversalByMovement() {
    rogue::GameSession session;
    session.Start(0x96u);

    const rogue::Vec2 start = session.World().rooms[0].center;
    const rogue::Vec2 target = session.World().rooms[1].center;
    const rogue::Vec2 dir = rogue::NormalizeOr(target - start, rogue::Vec2{1.0f, 0.0f});

    rogue::InputState move{};
    move.moveX = dir.x;
    move.moveY = dir.y;
    move.aimX = dir.x;
    move.aimY = dir.y;

    bool enteredRoom = false;
    for (int i = 0; i < 420 && session.Status() == rogue::RunStatus::InProgress; ++i) {
        session.Tick(move, 1.0f / 60.0f);
        if (session.CurrentRoom() == 1) {
            enteredRoom = true;
            break;
        }
    }

    bool ok = true;
    ok &= Expect(enteredRoom, "player can walk through the open portal into room 1");
    ok &= Expect(session.World().rooms[1].lifecycle == rogue::RoomLifecycle::Active, "walking into room 1 activates it");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RoomEntered), "movement entry emits room entered");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RoomActivated), "movement entry emits room activated");
    return ok;
}

bool TestClosedPortalBlocksMovement() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.PlacePlayer(world.rooms[1].center, 1);

    const rogue::Vec2 dir = rogue::NormalizeOr(world.rooms[2].center - world.rooms[1].center, rogue::Vec2{1.0f, 0.0f});
    rogue::InputState move{};
    move.moveX = dir.x;
    move.moveY = dir.y;
    move.aimX = dir.x;
    move.aimY = dir.y;

    for (int i = 0; i < 420; ++i) {
        sim.Tick(move, 1.0f / 60.0f, world);
    }

    bool ok = true;
    ok &= Expect(sim.Player().roomIndex == 1, "closed portal keeps player in current room");
    ok &= Expect(rogue::FindRoomAt(world, sim.Player().position) != 2, "closed portal prevents reaching locked room geometry");
    ok &= Expect(!rogue::IsTraversablePosition(world, 1, world.portals[1].position), "closed portal midpoint remains blocked");
    return ok;
}

bool TestRoomLifecycleAndActivation() {
    rogue::GameSession session;
    session.Start(0x96u);

    bool ok = true;
    ok &= Expect(session.Status() == rogue::RunStatus::InProgress, "run starts in progress");
    ok &= Expect(!session.TryEnterRoom(2), "locked room cannot be entered directly");
    ok &= Expect(session.TryEnterRoom(1), "available room can be entered");
    ok &= Expect(session.World().rooms[1].lifecycle == rogue::RoomLifecycle::Active, "entered room becomes active");
    ok &= Expect(session.Combat().ActiveEnemiesInRoom(1) > 0, "room entry activates enemies");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RoomEntered), "room entered event is emitted");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RoomActivated), "room activated event is emitted");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::EnemyActivated), "enemy activated event is emitted");
    return ok;
}

bool TestKillAllObjectiveCompletion() {
    rogue::GameSession session;
    session.Start(0x96u);
    bool ok = true;
    ok &= Expect(session.TryEnterRoom(1), "kill-all room can be entered");
    ok &= Expect(session.World().rooms[1].objective.kind == rogue::RoomObjectiveKind::KillAll, "room 1 is kill-all");
    ok &= Expect(session.Combat().LivingEnemiesInRoom(1) > 0, "kill-all room has living enemies");

    session.DamageRoomEnemies(1, 9999.0f);
    ok &= Expect(session.World().rooms[1].lifecycle == rogue::RoomLifecycle::Completed, "kill-all room completes after enemies die");
    ok &= Expect(session.Phase() == rogue::RunPhase::RewardChoice, "completed combat room offers reward before portal opens");
    ok &= Expect(session.RewardOptionCount() == rogue::kRewardChoiceCount, "reward offer has three options");
    ok &= Expect(session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Locked, "reward pending keeps next room locked");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::EnemyKilled), "enemy killed event is emitted");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::ObjectiveCompleted), "objective completed event is emitted");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RoomCompleted), "room completed event is emitted");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RewardOffered), "reward offered event is emitted");
    ok &= Expect(!HasEvent(session.LastTick(), rogue::GameEventType::PortalOpened), "portal waits for reward choice");

    ChooseReward(session, 0);
    ok &= Expect(session.Phase() == rogue::RunPhase::Exploring, "reward choice returns to exploration");
    ok &= Expect(session.RewardOptionCount() == 0, "reward options clear after selection");
    ok &= Expect(session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Available, "selected reward unlocks next room");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RewardSelected), "reward selected event is emitted");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::PortalOpened), "portal opened event is emitted after reward");
    return ok;
}

bool TestRewardChoiceContract() {
    rogue::GameSession a;
    rogue::GameSession b;
    ReachFirstReward(a);
    ReachFirstReward(b);
    const rogue::PlayerState rewardPlayer = a.Combat().Player();

    bool ok = true;
    ok &= Expect(a.Phase() == rogue::RunPhase::RewardChoice, "reward phase begins after clear");
    ok &= Expect(b.Phase() == rogue::RunPhase::RewardChoice, "matching seed also enters reward phase");
    ok &= Expect(rogue::PackRewardOverlayOption(rogue::RewardOption{}) == 0u, "inactive reward packs to empty overlay option");
    ok &= Expect(a.RewardOptions()[0].kind == rogue::RewardKind::WeaponSwap, "reward option 1 is a weapon swap");
    ok &= Expect(a.RewardOptions()[1].kind == rogue::RewardKind::ElementInfusion, "reward option 2 is an element infusion");
    ok &= Expect(a.RewardOptions()[2].kind == rogue::RewardKind::PlayerUpgrade, "reward option 3 is a player upgrade");

    for (int i = 0; i < rogue::kRewardChoiceCount; ++i) {
        const rogue::RewardOption& left = a.RewardOptions()[i];
        const rogue::RewardOption& right = b.RewardOptions()[i];
        const uint32_t packed = rogue::PackRewardOverlayOption(left);
        ok &= Expect(left.active, "reward option is active");
        ok &= Expect(left.kind == right.kind, "reward kind is deterministic");
        ok &= Expect(left.targetSlot == right.targetSlot, "reward target slot is deterministic");
        ok &= Expect(left.weapon == right.weapon, "reward weapon is deterministic");
        ok &= Expect(left.element == right.element, "reward element is deterministic");
        ok &= Expect(left.synergyElement == right.synergyElement, "reward synergy element is deterministic");
        ok &= Expect(left.upgrade == right.upgrade, "reward upgrade is deterministic");
        ok &= Expect(left.iconSeed == right.iconSeed, "reward icon seed is deterministic");
        ok &= Expect(rogue::RewardOverlayOptionActive(packed), "reward overlay option is active");
        ok &= Expect(rogue::RewardOverlayOptionKind(packed) == left.kind, "reward overlay stores kind");
        ok &= Expect(rogue::RewardOverlayOptionSlot(packed) == left.targetSlot, "reward overlay stores target slot");
        ok &= Expect(rogue::RewardOverlayOptionWeapon(packed) == left.weapon, "reward overlay stores weapon");
        ok &= Expect(rogue::RewardOverlayOptionElement(packed) == left.element, "reward overlay stores element");
        ok &= Expect(rogue::RewardOverlayOptionUpgrade(packed) == left.upgrade, "reward overlay stores upgrade");
        ok &= Expect(rogue::RewardOverlayOptionSynergyElement(packed) == left.synergyElement, "reward overlay stores synergy element");
    }

    const rogue::RewardOption& weaponSwap = a.RewardOptions()[0];
    const rogue::RewardOption& infusion = a.RewardOptions()[1];
    const rogue::RewardOption& upgrade = a.RewardOptions()[2];
    ok &= Expect(
        weaponSwap.weapon != rewardPlayer.weaponSlots[weaponSwap.targetSlot].weapon ||
            weaponSwap.element != rewardPlayer.weaponSlots[weaponSwap.targetSlot].element,
        "weapon swap reward changes the targeted slot loadout");
    ok &= Expect(infusion.weapon == rewardPlayer.weaponSlots[infusion.targetSlot].weapon, "infusion reward keeps the targeted weapon");
    ok &= Expect(infusion.element != rewardPlayer.weaponSlots[infusion.targetSlot].element, "infusion reward changes the targeted element");
    ok &= Expect(upgrade.value > 0.0f, "player upgrade reward has a non-zero value");
    ok &= Expect(
        upgrade.upgrade != rogue::PlayerUpgradeKind::Heal || rewardPlayer.hp < rewardPlayer.maxHp,
        "full-health reward roll avoids a no-op heal");

    rogue::InputState invalid{};
    invalid.rewardChoice = 9;
    a.Tick(invalid, 0.0f);
    ok &= Expect(a.Phase() == rogue::RunPhase::RewardChoice, "invalid reward choice keeps reward phase");
    ok &= Expect(a.World().rooms[2].lifecycle == rogue::RoomLifecycle::Locked, "invalid reward choice keeps portal closed");

    const rogue::RewardOption selected = a.RewardOptions()[0];
    ChooseReward(a, 0);
    ok &= Expect(a.Phase() == rogue::RunPhase::Exploring, "valid reward choice resumes exploration");
    ok &= Expect(a.World().rooms[2].lifecycle == rogue::RoomLifecycle::Available, "valid reward choice opens progression");
    ok &= Expect(a.Combat().Player().weaponSlots[selected.targetSlot].weapon == selected.weapon, "weapon reward applies to target slot");
    ok &= Expect(a.Combat().Player().weaponSlots[selected.targetSlot].element == selected.element, "weapon reward applies element");
    ok &= Expect(HasRewardSelectedPayload(a.LastTick(), 0, selected), "weapon reward selection event carries reward payload");

    rogue::GameSession infusionSession;
    ReachFirstReward(infusionSession);
    const rogue::PlayerState beforeInfusion = infusionSession.Combat().Player();
    const rogue::RewardOption infusionChoice = infusionSession.RewardOptions()[1];
    ChooseReward(infusionSession, 1);
    const rogue::PlayerState afterInfusion = infusionSession.Combat().Player();
    ok &= Expect(afterInfusion.weaponSlots[infusionChoice.targetSlot].weapon == beforeInfusion.weaponSlots[infusionChoice.targetSlot].weapon, "infusion selection keeps weapon in target slot");
    ok &= Expect(afterInfusion.weaponSlots[infusionChoice.targetSlot].element == infusionChoice.element, "infusion selection applies element to target slot");
    ok &= Expect(HasRewardSelectedPayload(infusionSession.LastTick(), 1, infusionChoice), "infusion reward selection event carries reward payload");

    rogue::GameSession upgradeSession;
    ReachFirstReward(upgradeSession);
    const rogue::PlayerState beforeUpgrade = upgradeSession.Combat().Player();
    const rogue::RewardOption upgradeChoice = upgradeSession.RewardOptions()[2];
    ChooseReward(upgradeSession, 2);
    const rogue::PlayerState afterUpgrade = upgradeSession.Combat().Player();
    ok &= Expect(PlayerUpgradeChanged(beforeUpgrade, afterUpgrade, upgradeChoice.upgrade), "upgrade selection changes player stats");
    ok &= Expect(HasRewardSelectedPayload(upgradeSession.LastTick(), 2, upgradeChoice), "upgrade reward selection event carries reward payload");
    return ok;
}

bool TestRewardSelectionRecoveryResetBeat() {
    rogue::GameSession session;
    session.Start(0x96u);
    bool ok = true;
    ok &= Expect(session.TryEnterRoom(1), "reward recovery test enters first combat room");

    bool sawPlayerDamage = false;
    for (int frame = 0;
         frame < 600 && session.Status() == rogue::RunStatus::InProgress && !sawPlayerDamage;
         ++frame) {
        session.Tick(rogue::InputState{}, 1.0f / 60.0f);
        sawPlayerDamage = HasEvent(session.LastTick(), rogue::GameEventType::PlayerDamaged);
    }

    const float damagedHp = session.Combat().Player().hp;
    const float maxHp = session.Combat().Player().maxHp;
    ok &= Expect(sawPlayerDamage, "reward recovery test receives real enemy damage");
    ok &= Expect(damagedHp < maxHp, "real enemy damage lowers HP before reward");
    ok &= Expect(damagedHp > 0.0f, "player survives before reward recovery");

    session.DamageRoomEnemies(1, 9999.0f);
    ok &= Expect(session.Phase() == rogue::RunPhase::RewardChoice, "damaged clear reaches reward choice");
    ok &= Expect(session.RewardOptionCount() == rogue::kRewardChoiceCount, "damaged clear offers three rewards");

    rogue::InputState invalid{};
    invalid.rewardChoice = rogue::kRewardChoiceCount;
    session.Tick(invalid, 0.0f);
    ok &= Expect(session.Phase() == rogue::RunPhase::RewardChoice, "invalid recovery choice keeps reward phase");
    ok &= Expect(session.Combat().Player().hp == damagedHp, "invalid recovery choice does not heal");
    ok &= Expect(!HasEvent(session.LastTick(), rogue::GameEventType::PortalOpened), "invalid recovery choice keeps portal closed");

    const rogue::RewardOption selected = session.RewardOptions()[0];
    const float beforeRewardHp = session.Combat().Player().hp;
    ChooseReward(session, 0);
    const float afterRewardHp = session.Combat().Player().hp;

    ok &= Expect(afterRewardHp > beforeRewardHp, "valid reward selection grants recovery");
    ok &= Expect(afterRewardHp <= maxHp, "reward recovery clamps to max HP");
    ok &= Expect(HasRewardSelectedPayload(session.LastTick(), 0, selected), "recovery reward selection carries payload");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::PortalOpened), "recovery reward opens portal");
    ok &= Expect(session.Phase() == rogue::RunPhase::Exploring, "recovery reward resumes exploration");
    ok &= Expect(session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Available, "recovery reward unlocks next room");
    return ok;
}

bool TestRewardChoiceLoadoutDiversity() {
    bool ok = true;
    for (uint32_t seed = 0x96u; seed < 0xb6u; ++seed) {
        rogue::GameSession offerSession;
        ReachFirstReward(offerSession, seed);
        ok &= Expect(offerSession.Phase() == rogue::RunPhase::RewardChoice, "diversity reward session reaches reward choice");
        const rogue::PlayerState offeredPlayer = offerSession.Combat().Player();
        const rogue::RewardOption& weaponSwap = offerSession.RewardOptions()[0];
        const rogue::RewardOption& infusion = offerSession.RewardOptions()[1];

        ok &= Expect(weaponSwap.kind == rogue::RewardKind::WeaponSwap, "diversity option 1 remains weapon swap");
        ok &= Expect(infusion.kind == rogue::RewardKind::ElementInfusion, "diversity option 2 remains infusion");
        ok &= Expect(
            weaponSwap.weapon != offeredPlayer.weaponSlots[weaponSwap.targetSlot].weapon,
            "weapon swap changes the targeted weapon");
        ok &= Expect(
            rogue::GetWeaponSpec(weaponSwap.weapon).category ==
                rogue::GetWeaponSpec(offeredPlayer.weaponSlots[weaponSwap.targetSlot].weapon).category,
            "weapon swap preserves the targeted slot role when a same-category option exists");
        ok &= Expect(
            !LoadoutHasWeaponExcept(offeredPlayer, weaponSwap.weapon, weaponSwap.targetSlot),
            "weapon swap offers a weapon outside the current loadout");
        ok &= Expect(
            !LoadoutHasElementExcept(offeredPlayer, weaponSwap.element, weaponSwap.targetSlot),
            "weapon swap pairs with an element outside the current loadout");
        ok &= Expect(
            RewardElementSynergyScore(offeredPlayer, weaponSwap.targetSlot, weaponSwap.element) > 0,
            "weapon swap element creates at least one current-loadout reaction pair");
        ok &= Expect(weaponSwap.synergyElement != rogue::Element::None, "weapon swap exposes the linked current element");
        ok &= Expect(
            TestElementReactionWeight(weaponSwap.synergyElement, weaponSwap.element) +
                    TestElementReactionWeight(weaponSwap.element, weaponSwap.synergyElement) >
                0,
            "weapon swap synergy hint names a real reaction pair");
        ok &= Expect(
            infusion.weapon == offeredPlayer.weaponSlots[infusion.targetSlot].weapon,
            "infusion names the weapon it modifies");
        ok &= Expect(
            infusion.element != offeredPlayer.weaponSlots[infusion.targetSlot].element,
            "infusion changes the targeted element");
        ok &= Expect(
            !LoadoutHasElementExcept(offeredPlayer, infusion.element, infusion.targetSlot),
            "infusion offers an element outside the current loadout");
        ok &= Expect(
            infusion.element != weaponSwap.element,
            "infusion offers a distinct synergy element from the weapon swap card");
        ok &= Expect(
            RewardElementSynergyScore(offeredPlayer, infusion.targetSlot, infusion.element) > 0,
            "infusion element creates at least one current-loadout reaction pair");
        ok &= Expect(infusion.synergyElement != rogue::Element::None, "infusion exposes the linked current element");
        ok &= Expect(
            TestElementReactionWeight(infusion.synergyElement, infusion.element) +
                    TestElementReactionWeight(infusion.element, infusion.synergyElement) >
                0,
            "infusion synergy hint names a real reaction pair");

        rogue::GameSession weaponSession;
        ReachFirstReward(weaponSession, seed);
        const int beforeWeaponCount = DistinctWeaponCount(weaponSession.Combat().Player());
        ChooseReward(weaponSession, 0);
        ok &= Expect(
            DistinctWeaponCount(weaponSession.Combat().Player()) >= beforeWeaponCount,
            "choosing weapon swap preserves or improves weapon variety");

        rogue::GameSession infusionSession;
        ReachFirstReward(infusionSession, seed);
        const int beforeElementCount = DistinctElementCount(infusionSession.Combat().Player());
        ChooseReward(infusionSession, 1);
        ok &= Expect(
            DistinctElementCount(infusionSession.Combat().Player()) >= beforeElementCount,
            "choosing infusion preserves or improves element variety");
    }
    return ok;
}

bool TestPlayerUpgradeGameplayScalars() {
    bool ok = true;

    {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        const rogue::Vec2 enemyPosition = world.rooms[1].center;
        KeepOnlyRoomSpawn(world, 1, enemyPosition, 0);

        rogue::CombatSim baseline;
        baseline.Reset(world);
        world.rooms[1].locked = false;
        world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
        baseline.ActivateEnemiesInRoom(1);
        const int baselineEnemy = FindActiveEnemyNear(baseline, 1, enemyPosition);
        ok &= Expect(baselineEnemy >= 0, "damage upgrade baseline finds a single brute target");
        if (baselineEnemy >= 0) {
            baseline.PlacePlayer(enemyPosition + rogue::Vec2{-0.75f, 0.0f}, 1);
            baseline.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, rogue::Element::Stone);
            const float beforeHp = baseline.Enemies()[baselineEnemy].hp;
            rogue::InputState cast{};
            cast.action1 = true;
            cast.aimX = 1.0f;
            baseline.Tick(cast, 1.0f / 60.0f, world);
            const float baselineDamage = beforeHp - baseline.Enemies()[baselineEnemy].hp;

            rogue::CombatSim upgraded;
            upgraded.Reset(world);
            upgraded.ActivateEnemiesInRoom(1);
            const int upgradedEnemy = FindActiveEnemyNear(upgraded, 1, enemyPosition);
            ok &= Expect(upgradedEnemy >= 0, "damage upgrade test finds upgraded brute target");
            if (upgradedEnemy >= 0) {
                upgraded.PlacePlayer(enemyPosition + rogue::Vec2{-0.75f, 0.0f}, 1);
                upgraded.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, rogue::Element::Stone);
                upgraded.ApplyPlayerUpgrade(rogue::PlayerUpgradeKind::Damage, 0.50f);
                const float upgradedBeforeHp = upgraded.Enemies()[upgradedEnemy].hp;
                upgraded.Tick(cast, 1.0f / 60.0f, world);
                const float upgradedDamage = upgradedBeforeHp - upgraded.Enemies()[upgradedEnemy].hp;
                ok &= Expect(baselineDamage > 0.0f, "baseline weapon damage is observable");
                ok &= Expect(upgradedDamage > baselineDamage * 1.35f, "damage upgrade increases actual action damage");
            }
        }
    }

    {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        rogue::CombatSim baseline;
        baseline.Reset(world);
        baseline.SetPlayerWeaponSlot(0, rogue::WeaponId::Katana, rogue::Element::Fire);
        rogue::InputState cast{};
        cast.action1 = true;
        cast.aimX = 1.0f;
        baseline.Tick(cast, 1.0f / 60.0f, world);

        rogue::CombatSim upgraded;
        upgraded.Reset(world);
        upgraded.SetPlayerWeaponSlot(0, rogue::WeaponId::Katana, rogue::Element::Fire);
        upgraded.ApplyPlayerUpgrade(rogue::PlayerUpgradeKind::Cooldown, 0.50f);
        upgraded.Tick(cast, 1.0f / 60.0f, world);

        ok &= Expect(baseline.Player().weaponSlots[0].cooldowns[0] > 0.0f, "baseline cast starts a Q cooldown");
        ok &= Expect(
            upgraded.Player().weaponSlots[0].cooldowns[0] < baseline.Player().weaponSlots[0].cooldowns[0] * 0.75f,
            "cooldown upgrade reduces actual action cooldown");
    }

    {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        rogue::CombatSim baseline;
        baseline.Reset(world);
        rogue::InputState move{};
        move.moveX = 1.0f;
        move.aimX = 1.0f;
        const rogue::Vec2 baselineStart = baseline.Player().position;
        baseline.Tick(move, 0.05f, world);
        const float baselineDistance = rogue::Length(baseline.Player().position - baselineStart);

        rogue::CombatSim upgraded;
        upgraded.Reset(world);
        upgraded.ApplyPlayerUpgrade(rogue::PlayerUpgradeKind::Speed, 0.50f);
        const rogue::Vec2 upgradedStart = upgraded.Player().position;
        upgraded.Tick(move, 0.05f, world);
        const float upgradedDistance = rogue::Length(upgraded.Player().position - upgradedStart);

        ok &= Expect(baselineDistance > 0.0f, "baseline movement is observable");
        ok &= Expect(upgradedDistance > baselineDistance * 1.35f, "speed upgrade increases actual movement distance");
    }

    {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        const rogue::Vec2 enemyPosition = world.rooms[1].center;
        KeepOnlyRoomSpawn(world, 1, enemyPosition, 0);
        world.rooms[1].locked = false;
        world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;

        rogue::CombatSim baseline;
        baseline.Reset(world);
        baseline.ActivateEnemiesInRoom(1);
        const int baselineEnemy = FindActiveEnemyNear(baseline, 1, enemyPosition);
        ok &= Expect(baselineEnemy >= 0, "area baseline finds a single brute target");
        if (baselineEnemy >= 0) {
            baseline.PlacePlayer(enemyPosition + rogue::Vec2{-2.35f, 0.0f}, 1);
            baseline.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, rogue::Element::Stone);
            const float beforeHp = baseline.Enemies()[baselineEnemy].hp;
            rogue::InputState cast{};
            cast.action1 = true;
            cast.aimX = 1.0f;
            baseline.Tick(cast, 1.0f / 60.0f, world);
            ok &= Expect(baseline.Enemies()[baselineEnemy].hp == beforeHp, "baseline area misses target just outside radius");

            rogue::CombatSim upgraded;
            upgraded.Reset(world);
            upgraded.ActivateEnemiesInRoom(1);
            const int upgradedEnemy = FindActiveEnemyNear(upgraded, 1, enemyPosition);
            ok &= Expect(upgradedEnemy >= 0, "area upgrade test finds upgraded brute target");
            if (upgradedEnemy >= 0) {
                upgraded.PlacePlayer(enemyPosition + rogue::Vec2{-2.35f, 0.0f}, 1);
                upgraded.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, rogue::Element::Stone);
                upgraded.ApplyPlayerUpgrade(rogue::PlayerUpgradeKind::Area, 0.25f);
                const float upgradedBeforeHp = upgraded.Enemies()[upgradedEnemy].hp;
                upgraded.Tick(cast, 1.0f / 60.0f, world);
                ok &= Expect(upgraded.Enemies()[upgradedEnemy].hp < upgradedBeforeHp, "area upgrade expands actual action footprint");
            }
        }
    }

    {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        rogue::CombatSim baseline;
        baseline.Reset(world);
        baseline.SetPlayerWeaponSlot(0, rogue::WeaponId::Pistol, rogue::Element::Stone);
        rogue::InputState shoot{};
        shoot.action1 = true;
        shoot.aimX = 1.0f;
        baseline.Tick(shoot, 1.0f / 60.0f, world);
        const rogue::ProjectileState* baselineProjectile = FirstActiveProjectile(baseline, rogue::Faction::Player);

        rogue::CombatSim upgraded;
        upgraded.Reset(world);
        upgraded.SetPlayerWeaponSlot(0, rogue::WeaponId::Pistol, rogue::Element::Stone);
        upgraded.ApplyPlayerUpgrade(rogue::PlayerUpgradeKind::Area, 0.50f);
        upgraded.Tick(shoot, 1.0f / 60.0f, world);
        const rogue::ProjectileState* upgradedProjectile = FirstActiveProjectile(upgraded, rogue::Faction::Player);

        ok &= Expect(baselineProjectile != nullptr, "baseline shot creates a player projectile");
        ok &= Expect(upgradedProjectile != nullptr, "area upgraded shot creates a player projectile");
        if (baselineProjectile && upgradedProjectile) {
            ok &= Expect(upgradedProjectile->radius > baselineProjectile->radius * 1.35f, "area upgrade increases projectile hit radius");
            ok &= Expect(upgradedProjectile->ttl > baselineProjectile->ttl * 1.35f, "area upgrade extends projectile reach");
        }
    }

    return ok;
}

bool TestInputActionBindings() {
    bool ok = true;

    rogue::InputState input{};
    rogue::ApplyInputActionBindings(input, true, false, false, false);
    ok &= Expect(input.action1 && input.melee, "Q maps to primary attack");
    ok &= Expect(!input.action2 && !input.ranged && !input.control, "Q does not trigger ability");

    input = rogue::InputState{};
    rogue::ApplyInputActionBindings(input, false, false, true, false);
    ok &= Expect(input.action1 && input.melee, "LMB maps to primary attack");
    ok &= Expect(!input.action2, "LMB does not trigger ability");

    input = rogue::InputState{};
    rogue::ApplyInputActionBindings(input, false, true, false, false);
    ok &= Expect(input.action2 && input.ranged && input.control, "E maps to ability action");
    ok &= Expect(!input.action1 && !input.melee, "E does not trigger primary attack");

    input = rogue::InputState{};
    rogue::ApplyInputActionBindings(input, false, false, false, true);
    ok &= Expect(input.action2 && input.ranged && input.control, "RMB mirrors ability action");

    input = rogue::InputState{};
    rogue::ApplyNumberInputBinding(input, 2);
    ok &= Expect(input.selectWeaponSlot == 2, "number key maps to weapon slot");
    ok &= Expect(input.rewardChoice == 2, "number key maps to reward choice");
    return ok;
}

bool TestInputNavigationBindingsAreNotInverted() {
    bool ok = true;

    rogue::InputState input{};
    rogue::ApplyMovementInputBindings(input, true, false, false, false);
    ok &= Expect(input.moveX == 0.0f && input.moveY > 0.0f, "W moves toward positive world/screen-up axis");

    input = rogue::InputState{};
    rogue::ApplyMovementInputBindings(input, false, false, true, false);
    ok &= Expect(input.moveX == 0.0f && input.moveY < 0.0f, "S moves toward negative world/screen-down axis");

    input = rogue::InputState{};
    rogue::ApplyMovementInputBindings(input, false, false, false, true);
    ok &= Expect(input.moveX > 0.0f && input.moveY == 0.0f, "D moves right");

    input = rogue::InputState{};
    rogue::ApplyScreenAimBinding(input, 1280.0f, 360.0f, 1280.0f, 720.0f);
    ok &= Expect(input.aimX > 0.0f && input.aimY == 0.0f, "cursor right of center aims right");

    input = rogue::InputState{};
    rogue::ApplyScreenAimBinding(input, 640.0f, 0.0f, 1280.0f, 720.0f);
    ok &= Expect(input.aimX == 0.0f && input.aimY > 0.0f, "cursor above center aims up");

    input = rogue::InputState{};
    rogue::ApplyScreenAimBinding(input, 640.0f, 720.0f, 1280.0f, 720.0f);
    ok &= Expect(input.aimX == 0.0f && input.aimY < 0.0f, "cursor below center aims down");
    return ok;
}

bool TestWeaponRosterContract() {
    bool ok = true;
    for (int i = 0; i < static_cast<int>(rogue::WeaponId::Count); ++i) {
        const rogue::WeaponId weapon = static_cast<rogue::WeaponId>(i);
        const rogue::WeaponSpec& spec = rogue::GetWeaponSpec(weapon);
        ok &= Expect(spec.id == weapon, "weapon spec id matches roster index");
        ok &= Expect(spec.name != nullptr && spec.name[0] != '\0', "weapon spec has readable name");
        ok &= Expect(spec.actionNames[0] != nullptr && spec.actionNames[0][0] != '\0', "weapon action1 has readable action name");
        ok &= Expect(spec.actionNames[1] != nullptr && spec.actionNames[1][0] != '\0', "weapon action2 has readable action name");
        ok &= Expect(spec.actions[0].damage > 0.0f, "weapon action1 has damage");
        ok &= Expect(spec.actions[1].damage > 0.0f, "weapon action2 has damage");
        ok &= Expect(spec.actions[0].cooldown > 0.0f, "weapon action1 has cooldown");
        ok &= Expect(spec.actions[1].cooldown > 0.0f, "weapon action2 has cooldown");
        ok &= Expect(spec.actions[1].cooldown >= spec.actions[0].cooldown, "weapon action2 has readable special cooldown");
    }
    return ok;
}

bool TestCombatActionTimingContract() {
    bool ok = true;

    const rogue::CombatActionTiming timing{0.10f, 0.16f, 0.18f, 0.44f};
    const rogue::CombatActionTiming unpacked = rogue::UnpackCombatActionTiming(rogue::PackCombatActionTiming(timing));
    ok &= Expect(std::fabs(unpacked.windup - timing.windup) < 0.005f, "combat action timing pack keeps windup");
    ok &= Expect(std::fabs(unpacked.impact - timing.impact) < 0.005f, "combat action timing pack keeps impact");
    ok &= Expect(std::fabs(unpacked.recovery - timing.recovery) < 0.005f, "combat action timing pack keeps recovery");
    ok &= Expect(std::fabs(unpacked.vfxDuration - timing.vfxDuration) < 0.005f, "combat action timing pack keeps vfx duration");

    for (int weaponIndex = 0; weaponIndex < static_cast<int>(rogue::WeaponId::Count); ++weaponIndex) {
        const rogue::WeaponId weapon = static_cast<rogue::WeaponId>(weaponIndex);
        for (int actionIndex = 0; actionIndex < 2; ++actionIndex) {
            const rogue::CombatActionTiming actionTiming =
                rogue::GetWeaponActionTiming(weapon, actionIndex == 0 ? rogue::WeaponActionIndex::Action1 : rogue::WeaponActionIndex::Action2);
            ok &= Expect(actionTiming.vfxDuration > 0.0f, "each weapon action exposes a procedural vfx duration");
            ok &= Expect(actionTiming.recovery >= 0.0f, "each weapon action exposes non-negative recovery");
        }
    }

    const rogue::CombatActionTiming hammerTiming =
        rogue::GetWeaponActionTiming(rogue::WeaponId::Hammer, rogue::WeaponActionIndex::Action1);
    ok &= Expect(hammerTiming.impact > 1.0f / 60.0f, "hammer Q has delayed impact for animation sync");

    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    const rogue::Vec2 enemyPosition = world.rooms[1].center;
    KeepOnlyRoomSpawn(world, 1, enemyPosition, 0);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);
    const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
    ok &= Expect(enemyIndex >= 0, "timing contract finds hammer target");
    if (enemyIndex < 0) {
        return ok;
    }

    sim.PlacePlayer(enemyPosition + rogue::Vec2{-0.85f, 0.0f}, 1);
    sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Hammer, rogue::Element::Stone);
    const float hpBefore = sim.Enemies()[enemyIndex].hp;

    rogue::InputState input{};
    input.action1 = true;
    input.aimX = 1.0f;
    sim.Tick(input, 1.0f / 60.0f, world);
    ok &= Expect(HasCombatActionEvent(
        sim,
        rogue::Faction::Player,
        rogue::WeaponId::Hammer,
        rogue::Element::Stone,
        rogue::AttackShape::Circle,
        0.0f), "timed hammer action emits gameplay action event");
    ok &= Expect(!HasCombatEventWithFaction(sim, rogue::CombatEventType::ActorDamaged, rogue::Faction::Enemy), "timed hammer does not damage before impact");
    ok &= Expect(std::fabs(sim.Enemies()[enemyIndex].hp - hpBefore) < 0.001f, "enemy hp stays stable before hammer impact");
    ok &= Expect(sim.Player().actionTimer > 0.0f, "player stores active action animation interval");

    const auto actionProxies = rogue::BuildEntityProxies(sim);
    ok &= Expect(HasProxyKind(actionProxies, rogue::EntityProxyKind::PlayerActionBurst), "active hammer action emits compact impact burst proxy");
    ok &= Expect(!HasProxyKind(actionProxies, rogue::EntityProxyKind::PlayerActionRing), "active hammer action avoids legacy ring proxy");

    bool sawImpact = false;
    for (int frame = 0; frame < 24 && !sawImpact; ++frame) {
        sim.Tick(rogue::InputState{}, 1.0f / 60.0f, world);
        sawImpact = HasCombatEventWithFaction(sim, rogue::CombatEventType::ActorDamaged, rogue::Faction::Enemy);
    }
    ok &= Expect(sawImpact, "timed hammer damages exactly on a later impact tick");
    ok &= Expect(sim.Enemies()[enemyIndex].hp < hpBefore, "enemy hp drops after delayed hammer impact");

    return ok;
}

bool TestWeaponActionVisualLanguageContract() {
    auto castPrimary = [](rogue::WeaponId weapon, rogue::Element element) {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u + static_cast<uint32_t>(weapon));
        rogue::CombatSim sim;
        sim.Reset(world);
        sim.SetPlayerWeaponSlot(0, weapon, element);
        rogue::InputState input{};
        input.action1 = true;
        input.aimX = 1.0f;
        sim.Tick(input, 1.0f / 60.0f, world);
        return rogue::BuildEntityProxies(sim);
    };

    const auto hammer = castPrimary(rogue::WeaponId::Hammer, rogue::Element::Stone);
    const auto spear = castPrimary(rogue::WeaponId::Spear, rogue::Element::Air);
    const auto katana = castPrimary(rogue::WeaponId::Katana, rogue::Element::Fire);
    const auto katanaIce = castPrimary(rogue::WeaponId::Katana, rogue::Element::Ice);
    const auto rifle = castPrimary(rogue::WeaponId::Rifle, rogue::Element::Electricity);
    const auto machinegun = castPrimary(rogue::WeaponId::Machinegun, rogue::Element::Fire);
    const auto shotgun = castPrimary(rogue::WeaponId::Shotgun, rogue::Element::Ice);
    const auto staff = castPrimary(rogue::WeaponId::Staff, rogue::Element::Water);
    const auto scepter = castPrimary(rogue::WeaponId::Scepter, rogue::Element::Fire);
    const auto gloves = castPrimary(rogue::WeaponId::Gloves, rogue::Element::Electricity);
    const auto pistolFire = castPrimary(rogue::WeaponId::Pistol, rogue::Element::Fire);
    const auto pistolElectric = castPrimary(rogue::WeaponId::Pistol, rogue::Element::Electricity);

    bool ok = true;
    ok &= Expect(!HasProxyKind(hammer, rogue::EntityProxyKind::PlayerActionRing), "hammer action visual avoids heavy impact rings");
    ok &= Expect(HasProxyKind(hammer, rogue::EntityProxyKind::PlayerActionBurst), "hammer action visual includes impact burst");
    ok &= Expect(HasProxyKind(hammer, rogue::EntityProxyKind::PlayerActionLine), "hammer action visual includes directed impact streak");
    ok &= Expect(HasProxyKind(spear, rogue::EntityProxyKind::PlayerActionLine), "spear action visual uses thrust line");
    ok &= Expect(HasProxyKind(spear, rogue::EntityProxyKind::PlayerActionCone), "spear action visual includes thrust tip");
    ok &= Expect(HasProxyKind(katana, rogue::EntityProxyKind::PlayerActionCone), "katana action visual uses slash cone");
    ok &= Expect(CountProxyKind(katana, rogue::EntityProxyKind::PlayerActionLine) >= 2, "katana action visual includes slash streaks");
    ok &= Expect(CountProxyKind(machinegun, rogue::EntityProxyKind::PlayerActionLine) >= 3, "machinegun action visual uses multi-line spray");
    ok &= Expect(HasProxyKind(shotgun, rogue::EntityProxyKind::PlayerActionBurst), "shotgun action visual uses scatter burst");
    ok &= Expect(HasProxyKind(shotgun, rogue::EntityProxyKind::PlayerActionCone), "shotgun action visual keeps readable cone");
    ok &= Expect(HasProxyKind(staff, rogue::EntityProxyKind::PlayerActionBurst), "staff action visual uses compact magic burst");
    ok &= Expect(HasProxyKind(staff, rogue::EntityProxyKind::PlayerActionLine), "staff action visual keeps a directed casting lane");
    ok &= Expect(!HasProxyKind(staff, rogue::EntityProxyKind::PlayerActionRing), "staff action visual avoids orbit rings");
    ok &= Expect(HasProxyKind(scepter, rogue::EntityProxyKind::PlayerActionBurst), "scepter action visual marks target with compact burst");
    ok &= Expect(HasProxyKind(scepter, rogue::EntityProxyKind::PlayerActionLine), "scepter action visual links caster and mark");
    ok &= Expect(HasProxyKind(gloves, rogue::EntityProxyKind::PlayerActionBurst), "gloves action visual uses local pulse burst");
    ok &= Expect(HasProxyKind(gloves, rogue::EntityProxyKind::PlayerActionLine), "gloves action visual uses short punch streak");
    ok &= Expect(!HasProxyKind(gloves, rogue::EntityProxyKind::PlayerActionRing), "gloves action visual avoids layered pulse rings");

    const uint32_t hammerHash = rogue::HashPackedRTGeometry(rogue::PackRTGeometry(rogue::GenerateRTGeometry(hammer)));
    const uint32_t katanaHash = rogue::HashPackedRTGeometry(rogue::PackRTGeometry(rogue::GenerateRTGeometry(katana)));
    const uint32_t katanaIceHash = rogue::HashPackedRTGeometry(rogue::PackRTGeometry(rogue::GenerateRTGeometry(katanaIce)));
    const uint32_t rifleHash = rogue::HashPackedRTGeometry(rogue::PackRTGeometry(rogue::GenerateRTGeometry(rifle)));
    ok &= Expect(hammerHash != katanaHash, "hammer and katana action geometry hashes differ");
    ok &= Expect(katanaHash != rifleHash, "katana and rifle action geometry hashes differ");
    ok &= Expect(katanaHash != katanaIceHash, "same weapon changes procedural particle geometry by element");

    const rogue::EntityRTProxy* fireProjectile = FindProxyKind(pistolFire, rogue::EntityProxyKind::Projectile);
    const rogue::EntityRTProxy* electricProjectile = FindProxyKind(pistolElectric, rogue::EntityProxyKind::Projectile);
    ok &= Expect(fireProjectile != nullptr, "fire pistol emits projectile visual proxy");
    ok &= Expect(electricProjectile != nullptr, "electric pistol emits projectile visual proxy");
    if (fireProjectile != nullptr && electricProjectile != nullptr) {
        ok &= Expect(fireProjectile->visualTag != 0u, "fire projectile carries elemental visual tag");
        ok &= Expect(electricProjectile->visualTag != 0u, "electric projectile carries elemental visual tag");
        ok &= Expect(fireProjectile->materialId != electricProjectile->materialId, "projectile proxy material follows element");
        const uint32_t fireProjectileHash = rogue::HashPackedRTGeometry(rogue::PackRTGeometry(rogue::GenerateRTGeometry(pistolFire)));
        const uint32_t electricProjectileHash = rogue::HashPackedRTGeometry(rogue::PackRTGeometry(rogue::GenerateRTGeometry(pistolElectric)));
        ok &= Expect(fireProjectileHash != electricProjectileHash, "projectile particle field changes by element");
    }
    return ok;
}

bool TestEnemyActionVisualLanguageContract() {
    auto runEnemy = [](
                        int archetype,
                        float distance,
                        rogue::EntityProxyKind primaryKind,
                        int minPrimary,
                        const char* activeMessage,
                        const char* visualMessage) {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        KeepOnlyRoomSpawn(world, 1, world.rooms[1].center, archetype);
        rogue::CombatSim sim;
        sim.Reset(world);
        world.rooms[1].locked = false;
        world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
        sim.ActivateEnemiesInRoom(1);

        bool roleOk = true;
        const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
        roleOk &= Expect(enemyIndex >= 0, activeMessage);
        if (enemyIndex < 0) {
            return roleOk;
        }
        roleOk &= Expect(PlacePlayerAtEnemyRange(sim, world, enemyIndex, distance), "enemy visual language places player in attack range");
        if (!roleOk) {
            return roleOk;
        }
        sim.Tick(rogue::InputState{}, 1.0f / 60.0f, world);
        const auto proxies = rogue::BuildEntityProxies(sim);
        roleOk &= Expect(sim.Enemies()[enemyIndex].actionTimer > 0.0f, activeMessage);
        roleOk &= Expect(CountProxyKind(proxies, primaryKind) >= minPrimary, visualMessage);
        roleOk &= Expect(CountEnemyTellProxies(proxies) >= minPrimary, "enemy active action emits procedural danger footprint");
        return roleOk;
    };

    bool ok = true;
    ok &= runEnemy(0, 1.20f, rogue::EntityProxyKind::EnemyTellRing, 2, "brute enters hammer windup", "brute windup uses heavy rings");
    ok &= runEnemy(1, 4.40f, rogue::EntityProxyKind::EnemyTellLine, 1, "caster enters staff cast", "caster cast uses line footprint");
    ok &= runEnemy(2, 3.20f, rogue::EntityProxyKind::EnemyTellLine, 2, "skirmisher enters wave cast", "skirmisher cast uses double line footprint");
    ok &= runEnemy(3, 2.40f, rogue::EntityProxyKind::EnemyTellCone, 1, "bulwark enters shotgun windup", "bulwark windup uses cone footprint");

    rogue::RoomGraph bossWorld = rogue::GenerateWorld(0x96u);
    KeepOnlyFinalBossSpawn(bossWorld);
    rogue::CombatSim bossSim;
    bossSim.Reset(bossWorld);
    const int finalRoom = bossWorld.roomCount - 1;
    bossWorld.rooms[finalRoom].locked = false;
    bossWorld.rooms[finalRoom].lifecycle = rogue::RoomLifecycle::Active;
    bossSim.ActivateEnemiesInRoom(finalRoom);
    const int bossIndex = FindActiveEnemyKind(bossSim, rogue::EnemyKind::Boss);
    ok &= Expect(bossIndex >= 0, "boss visual language finds boss");
    if (bossIndex >= 0) {
        ok &= Expect(PlacePlayerAtEnemyRange(bossSim, bossWorld, bossIndex, 5.20f), "boss visual language places player in phase one range");
        bossSim.Tick(rogue::InputState{}, 1.0f / 60.0f, bossWorld);
        const auto bossProxies = rogue::BuildEntityProxies(bossSim);
        ok &= Expect(bossSim.Enemies()[bossIndex].actionTimer > 0.0f, "boss enters active action interval");
        ok &= Expect(CountProxyKind(bossProxies, rogue::EntityProxyKind::EnemyTellRing) >= 2, "boss target mark uses layered rings");
    }
    return ok;
}

bool TestActiveWeaponSlotActions() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);
    sim.SetPlayerWeaponSlot(1, rogue::WeaponId::Rifle, rogue::Element::Air);

    rogue::InputState input{};
    input.selectWeaponSlot = 1;
    input.action1 = true;
    input.aimX = 1.0f;
    sim.Tick(input, 1.0f / 60.0f, world);

    bool ok = true;
    ok &= Expect(sim.Player().activeWeaponSlot == 1, "input selects active weapon slot");
    ok &= Expect(HasCombatEvent(sim, rogue::CombatEventType::WeaponActionUsed), "active weapon action emits usage event");
    ok &= Expect(HasCombatActionEvent(
        sim,
        rogue::Faction::Player,
        rogue::WeaponId::Rifle,
        rogue::Element::Air,
        rogue::AttackShape::Projectile,
        0.0f), "weapon action event carries weapon element and projectile shape");
    ok &= Expect(HasCombatEvent(sim, rogue::CombatEventType::ProjectileSpawned), "rifle action spawns projectile");
    ok &= Expect(HasCombatEventWithFaction(sim, rogue::CombatEventType::ProjectileSpawned, rogue::Faction::Player), "player projectile spawn keeps player faction");
    return ok;
}

bool TestFullWeaponRosterRuntimeActions() {
    bool ok = true;
    constexpr std::array<rogue::Element, 6> testElements{
        rogue::Element::Water,
        rogue::Element::Fire,
        rogue::Element::Stone,
        rogue::Element::Electricity,
        rogue::Element::Ice,
        rogue::Element::Air
    };

    for (int weaponIndex = 0; weaponIndex < static_cast<int>(rogue::WeaponId::Count); ++weaponIndex) {
        const rogue::WeaponId weapon = static_cast<rogue::WeaponId>(weaponIndex);
        const rogue::WeaponSpec& spec = rogue::GetWeaponSpec(weapon);
        for (int actionIndex = 0; actionIndex < 2; ++actionIndex) {
            rogue::RoomGraph world = rogue::GenerateWorld(0x96u + static_cast<uint32_t>(weaponIndex * 17 + actionIndex));
            rogue::CombatSim sim;
            sim.Reset(world);
            const rogue::Room& room = world.rooms[0];
            const rogue::Vec2 start = room.center - rogue::Vec2{room.halfSize.x * 0.45f, 0.0f};
            sim.PlacePlayer(start, 0);

            const rogue::Element element = testElements[static_cast<std::size_t>((weaponIndex + actionIndex) % static_cast<int>(testElements.size()))];
            sim.SetPlayerWeaponSlot(0, weapon, element);

            rogue::InputState input{};
            rogue::ApplyInputActionBindings(input, actionIndex == 0, actionIndex == 1, false, false);
            input.aimX = 1.0f;
            input.aimY = 0.0f;
            sim.Tick(input, 1.0f / 60.0f, world);

            const rogue::WeaponActionSpec& action = spec.actions[static_cast<std::size_t>(actionIndex)];
            const float actionValue = static_cast<float>(actionIndex);
            ok &= Expect(HasCombatActionEvent(
                sim,
                rogue::Faction::Player,
                weapon,
                element,
                action.shape,
                actionValue), "full weapon roster action emits matching gameplay event");
            ok &= Expect(sim.Player().weaponSlots[0].cooldowns[static_cast<std::size_t>(actionIndex)] > 0.0f, "full weapon roster action starts cooldown");

            if (action.shape == rogue::AttackShape::Projectile ||
                action.shape == rogue::AttackShape::Wave ||
                action.shape == rogue::AttackShape::Burst) {
                const int expectedProjectiles = action.shape == rogue::AttackShape::Burst
                    ? std::max(1, action.projectileCount)
                    : 1;
                ok &= Expect(
                    CountCombatEventWithFaction(sim, rogue::CombatEventType::ProjectileSpawned, rogue::Faction::Player) >= expectedProjectiles,
                    "full weapon roster projectile action emits spawned projectiles");
            }
            if (action.shape == rogue::AttackShape::Dash) {
                ok &= Expect(rogue::LengthSq(sim.Player().position - start) > 0.10f, "full weapon roster dash action moves player");
                ok &= Expect(sim.Player().statuses.invulnerable > 0.0f, "full weapon roster dash action grants invulnerability");
            }
            if (action.rooted) {
                ok &= Expect(sim.Player().rootTimer > 0.0f, "full weapon roster rooted action starts root timer");
            }
        }
    }
    return ok;
}

bool TestSpecialActionShapeGameplaySemantics() {
    bool ok = true;

    {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        KeepOnlyFinalBossSpawn(world);
        rogue::CombatSim sim;
        sim.Reset(world);
        sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Spear, rogue::Element::Air);

        const rogue::Vec2 start = sim.Player().position;
        const rogue::Room& room = world.rooms[sim.Player().roomIndex];
        rogue::Vec2 aim{1.0f, 0.0f};
        if ((room.center.x + room.halfSize.x - start.x) < (start.x - (room.center.x - room.halfSize.x))) {
            aim = rogue::Vec2{-1.0f, 0.0f};
        }

        rogue::InputState input{};
        input.action2 = true;
        input.aimX = aim.x;
        input.aimY = aim.y;
        sim.Tick(input, 1.0f / 60.0f, world);

        const float dashDistance = rogue::Dot(sim.Player().position - start, aim);
        ok &= Expect(dashDistance > 0.75f, "spear E dash moves the player along aim");
        ok &= Expect(sim.Player().statuses.invulnerable > 0.0f, "spear E dash applies short invulnerability window");
        ok &= Expect(HasCombatActionEvent(
            sim,
            rogue::Faction::Player,
            rogue::WeaponId::Spear,
            rogue::Element::Air,
            rogue::AttackShape::Dash,
            1.0f), "spear E emits dash shape event");
    }

    {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        rogue::CombatSim sim;
        sim.Reset(world);
        world.rooms[1].locked = false;
        world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
        sim.ActivateEnemiesInRoom(1);

        const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
        ok &= Expect(enemyIndex >= 0, "target area test finds a living enemy");
        if (enemyIndex >= 0) {
            const rogue::WeaponActionSpec& action = rogue::GetWeaponSpec(rogue::WeaponId::Scepter).actions[0];
            rogue::Vec2 playerPosition{};
            rogue::Vec2 aim{};
            ok &= Expect(FindCastingPositionForTarget(
                world,
                1,
                sim.Enemies()[enemyIndex].position,
                action.range,
                playerPosition,
                aim), "target area test finds a ranged casting position");
            if (ok) {
                const float beforeHp = sim.Enemies()[enemyIndex].hp;
                sim.PlacePlayer(playerPosition, 1);
                sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Scepter, rogue::Element::Fire);

                rogue::InputState input{};
                input.action1 = true;
                input.aimX = aim.x;
                input.aimY = aim.y;
                sim.Tick(input, 1.0f / 60.0f, world);

                ok &= Expect(sim.Enemies()[enemyIndex].hp < beforeHp, "scepter Q target area damages a distant aimed target");
                ok &= Expect(HasCombatActionEvent(
                    sim,
                    rogue::Faction::Player,
                    rogue::WeaponId::Scepter,
                    rogue::Element::Fire,
                    rogue::AttackShape::TargetArea,
                    0.0f), "scepter Q emits target-area shape event");
            }
        }
    }

    {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        rogue::CombatSim sim;
        sim.Reset(world);
        world.rooms[1].locked = false;
        world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
        sim.ActivateEnemiesInRoom(1);

        const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
        ok &= Expect(enemyIndex >= 0, "orbit test finds a living enemy");
        if (enemyIndex >= 0) {
            const rogue::WeaponActionSpec& action = rogue::GetWeaponSpec(rogue::WeaponId::Staff).actions[0];
            rogue::Vec2 playerPosition{};
            rogue::Vec2 aim{};
            ok &= Expect(FindCastingPositionForTarget(
                world,
                1,
                sim.Enemies()[enemyIndex].position,
                action.range,
                playerPosition,
                aim), "orbit test finds a ring casting position");
            if (ok) {
                const float beforeHp = sim.Enemies()[enemyIndex].hp;
                sim.PlacePlayer(playerPosition, 1);
                sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Staff, rogue::Element::Water);

                rogue::InputState input{};
                input.action1 = true;
                input.aimX = aim.x;
                input.aimY = aim.y;
                sim.Tick(input, 1.0f / 60.0f, world);

                ok &= Expect(sim.Enemies()[enemyIndex].hp < beforeHp, "staff Q orbit damages enemies on the orbit band");
                ok &= Expect(HasCombatActionEvent(
                    sim,
                    rogue::Faction::Player,
                    rogue::WeaponId::Staff,
                    rogue::Element::Water,
                    rogue::AttackShape::Orbit,
                    0.0f), "staff Q emits orbit shape event");
            }
        }
    }

    return ok;
}

bool TestRewardChoiceKeyboardBinding() {
    rogue::GameSession session;
    session.Start(0x96u);
    session.TryEnterRoom(1);
    session.DamageRoomEnemies(1, 9999.0f);

    rogue::InputState chooseSecond{};
    rogue::ApplyNumberInputBinding(chooseSecond, 1);
    session.Tick(chooseSecond, 0.0f);

    bool ok = true;
    ok &= Expect(session.Phase() == rogue::RunPhase::Exploring, "number key selects reward in reward phase");
    ok &= Expect(session.RewardOptionCount() == 0, "keyboard reward selection clears reward choices");
    ok &= Expect(session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Available, "keyboard reward selection opens progression");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RewardSelected), "keyboard reward selection emits reward selected");
    return ok;
}

bool TestPlayerAbilityFeedbackContract() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    KeepOnlyFinalBossSpawn(world);
    rogue::CombatSim sim;
    sim.Reset(world);
    sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, rogue::Element::Ice);

    const auto readyProxies = rogue::BuildEntityProxies(sim);
    const rogue::EntityRTProxy* lookIndicator = FindProxyKind(readyProxies, rogue::EntityProxyKind::PlayerBlade);

    rogue::InputState ability{};
    ability.action2 = true;
    ability.aimX = 1.0f;
    sim.Tick(ability, 1.0f / 60.0f, world);
    const auto cooldownProxies = rogue::BuildEntityProxies(sim);
    const rogue::RenderScene cooldownScene = rogue::BuildRenderScene(world, sim, 1280, 720, 2.0f, 8u);

    bool ok = true;
    ok &= Expect(lookIndicator != nullptr && lookIndicator->radius <= 0.45f, "player keeps only a compact facing weapon indicator");
    ok &= Expect(!HasProxyKind(readyProxies, rogue::EntityProxyKind::PlayerPrimaryGuide), "ready player hides persistent Q guide");
    ok &= Expect(!HasProxyKind(readyProxies, rogue::EntityProxyKind::PlayerAbilityLine), "ready player hides persistent E line guide");
    ok &= Expect(!HasProxyKind(readyProxies, rogue::EntityProxyKind::PlayerAbilityRing), "ready player hides persistent E ring guide");
    ok &= Expect(HasCombatEventValue(sim, rogue::CombatEventType::WeaponActionUsed, rogue::Faction::Player, 1.0f), "E emits action2 usage event");
    ok &= Expect(sim.Player().controlCooldown > 0.0f, "E starts ability cooldown feedback");
    ok &= Expect(sim.Player().rangedCooldown == sim.Player().controlCooldown, "legacy ranged/control cooldown aliases stay synced");
    ok &= Expect(rogue::PlayerActionCooldownRatio(sim.Player(), rogue::WeaponActionIndex::Action2) > 0.5f, "E cooldown ratio is visible");
    ok &= Expect(!HasProxyKind(cooldownProxies, rogue::EntityProxyKind::PlayerAbilityCooldown), "cooling ability uses overlay instead of world ring");
    ok &= Expect(!HasProxyKind(cooldownProxies, rogue::EntityProxyKind::PlayerAbilityRing), "cooling ability hides ready E guide ring");
    ok &= Expect(cooldownScene.overlay.overlayWeaponId == static_cast<uint32_t>(rogue::WeaponId::Gloves), "overlay tracks active weapon id");
    ok &= Expect(cooldownScene.overlay.overlayActiveSlot == 1u, "overlay tracks active weapon slot");
    ok &= Expect(cooldownScene.overlay.overlayQReadyPercent == 100u, "overlay tracks Q readiness");
    ok &= Expect(cooldownScene.overlay.overlayEReadyPercent < 100u, "overlay tracks E cooldown");
    return ok;
}

bool TestObjectiveOverlayContract() {
    rogue::GameSession session;
    session.Start(0x96u);
    session.TryEnterRoom(1);
    session.DamageRoomEnemies(1, 9999.0f);
    ChooseReward(session, 0);
    session.TryEnterRoom(2);

    for (int i = 0; i < 30 && session.Status() == rogue::RunStatus::InProgress; ++i) {
        session.Tick(rogue::InputState{}, 1.0f / 60.0f);
    }

    const rogue::RenderScene survivalScene = rogue::BuildRenderScene(
        session.World(),
        session.Combat(),
        1280,
        720,
        0.5f,
        12u);

    bool ok = true;
    ok &= Expect(survivalScene.overlay.overlayObjectiveKind == static_cast<uint32_t>(rogue::RoomObjectiveKind::SurviveTimer), "overlay exposes survival objective kind");
    ok &= Expect(survivalScene.overlay.overlayObjectiveProgressPercent > 0u, "overlay exposes live survival progress");
    ok &= Expect(survivalScene.overlay.overlayObjectiveProgressPercent < 100u, "overlay does not complete survival early");

    CompleteCurrentRoomObjective(session);
    const rogue::RenderScene rewardScene = rogue::BuildRenderScene(
        session.World(),
        session.Combat(),
        1280,
        720,
        1.4f,
        24u);
    ok &= Expect(rewardScene.overlay.overlayObjectiveProgressPercent == 100u, "overlay shows completed objective at reward pause");
    return ok;
}

bool TestGameSessionPlayerActionEvents() {
    rogue::GameSession primarySession;
    primarySession.Start(0x96u);
    rogue::InputState primary{};
    primary.action1 = true;
    primary.aimX = 1.0f;
    primarySession.Tick(primary, 1.0f / 60.0f);

    rogue::GameSession abilitySession;
    abilitySession.Start(0x96u);
    rogue::InputState ability{};
    ability.action2 = true;
    ability.aimX = 1.0f;
    abilitySession.Tick(ability, 1.0f / 60.0f);

    bool ok = true;
    ok &= Expect(HasGameEvent(primarySession.LastTick(), rogue::GameEventType::PlayerActionUsed, 0.0f), "Q action emits player action event");
    ok &= Expect(HasGameEvent(abilitySession.LastTick(), rogue::GameEventType::PlayerAbilityUsed, 1.0f), "E action emits player ability event");
    ok &= Expect(HasGameActionEvent(
        primarySession.LastTick(),
        rogue::GameEventType::PlayerActionUsed,
        rogue::WeaponId::Katana,
        rogue::Element::Fire,
        rogue::AttackShape::Cone,
        0.0f), "Q event keeps primary weapon action shape");
    ok &= Expect(HasGameActionEvent(
        abilitySession.LastTick(),
        rogue::GameEventType::PlayerAbilityUsed,
        rogue::WeaponId::Katana,
        rogue::Element::Fire,
        rogue::AttackShape::Wave,
        1.0f), "E event keeps ability weapon action shape");
    return ok;
}

bool TestEnemyActionEventFaction() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);

    const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
    bool ok = true;
    ok &= Expect(enemyIndex >= 0, "enemy faction test finds a living enemy");
    if (enemyIndex < 0) {
        return ok;
    }

    sim.PlacePlayer(sim.Enemies()[enemyIndex].position + rogue::Vec2{0.10f, 0.0f}, 1);
    sim.Tick(rogue::InputState{}, 1.0f / 60.0f, world);
    ok &= Expect(HasCombatEventWithFaction(sim, rogue::CombatEventType::WeaponActionUsed, rogue::Faction::Enemy), "enemy weapon usage keeps enemy faction");
    return ok;
}

bool TestWetFireReactionThroughWeaponPipeline() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);

    const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
    bool ok = true;
    ok &= Expect(enemyIndex >= 0, "reaction test finds a living enemy");
    if (enemyIndex < 0) {
        return ok;
    }

    const rogue::Vec2 enemyPos = sim.Enemies()[enemyIndex].position;
    sim.PlacePlayer(enemyPos + rogue::Vec2{-0.75f, 0.0f}, 1);
    sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, rogue::Element::Water);

    rogue::InputState cast{};
    cast.action1 = true;
    cast.aimX = 1.0f;
    sim.Tick(cast, 1.0f / 60.0f, world);
    ok &= Expect(HasCombatEvent(sim, rogue::CombatEventType::StatusApplied), "water weapon applies wet status");
    ok &= Expect(sim.Enemies()[enemyIndex].statuses.slots[0].active ||
        sim.Enemies()[enemyIndex].statuses.slots[1].active ||
        sim.Enemies()[enemyIndex].statuses.slots[2].active ||
        sim.Enemies()[enemyIndex].statuses.slots[3].active, "enemy stores an active status");

    sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, rogue::Element::Fire);
    cast = rogue::InputState{};
    cast.action1 = true;
    cast.aimX = 1.0f;
    sim.Tick(cast, 1.0f / 60.0f, world);
    ok &= Expect(HasCombatReaction(sim, rogue::ReactionKind::WetFire), "fire hit reacts with wet status");
    ok &= Expect(HasCombatEvent(sim, rogue::CombatEventType::StatusRemoved), "wet fire reaction removes wet status");
    return ok;
}

bool TestWaterIceKeepsMultiStatusSnapshotPipeline() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);

    const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
    bool ok = true;
    ok &= Expect(enemyIndex >= 0, "multi-status test finds a living enemy");
    if (enemyIndex < 0) {
        return ok;
    }

    CastGlovesElementAtEnemy(sim, world, enemyIndex, rogue::Element::Water);
    ok &= Expect(EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Wet), "water applies wet catalyst");

    CastGlovesElementAtEnemy(sim, world, enemyIndex, rogue::Element::Ice);
    ok &= Expect(HasCombatReaction(sim, rogue::ReactionKind::WetIce), "ice reacts with wet catalyst");
    ok &= Expect(EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Wet), "wet remains after wet+ice");
    ok &= Expect(EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Chilled), "ice adds chilled alongside wet");
    ok &= Expect(EnemyStatusIntensity(sim, enemyIndex, rogue::StatusKind::Chilled) >= 0.51f, "wet+ice boosts chilled slow intensity");
    return ok;
}

bool TestChilledWaterBoostsSlowAndKeepsWet() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);

    const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
    bool ok = true;
    ok &= Expect(enemyIndex >= 0, "chilled-water test finds a living enemy");
    if (enemyIndex < 0) {
        return ok;
    }

    CastGlovesElementAtEnemy(sim, world, enemyIndex, rogue::Element::Ice);
    ok &= Expect(EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Chilled), "ice applies chilled catalyst");
    ok &= Expect(EnemyStatusIntensity(sim, enemyIndex, rogue::StatusKind::Chilled) >= 0.31f, "base chilled starts at normal slow intensity");
    ok &= Expect(EnemyStatusIntensity(sim, enemyIndex, rogue::StatusKind::Chilled) < 0.51f, "base chilled is weaker than reaction boosted slow");

    CastGlovesElementAtEnemy(sim, world, enemyIndex, rogue::Element::Water);
    ok &= Expect(HasCombatReaction(sim, rogue::ReactionKind::ChilledWater), "water reacts with chilled catalyst");
    ok &= Expect(EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Wet), "chilled+water applies wet");
    ok &= Expect(EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Chilled), "chilled+water preserves chilled");
    ok &= Expect(EnemyStatusIntensity(sim, enemyIndex, rogue::StatusKind::Chilled) >= 0.51f, "chilled+water boosts chilled slow intensity");
    return ok;
}

bool TestChargedWaterCreatesStatusLockAndClearsCatalyst() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);

    const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
    bool ok = true;
    ok &= Expect(enemyIndex >= 0, "charged-water test finds a living enemy");
    if (enemyIndex < 0) {
        return ok;
    }

    CastGlovesElementAtEnemy(sim, world, enemyIndex, rogue::Element::Electricity);
    ok &= Expect(EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Charged), "electricity applies charged catalyst");

    CastGlovesElementAtEnemy(sim, world, enemyIndex, rogue::Element::Water);
    ok &= Expect(HasCombatReaction(sim, rogue::ReactionKind::ChargedWater), "water reacts with charged catalyst");
    ok &= Expect(HasCombatEvent(sim, rogue::CombatEventType::StunApplied), "charged+water creates stun sequence hook");
    ok &= Expect(sim.Enemies()[enemyIndex].statuses.statusLock > 0.0f, "charged+water creates status lock");
    ok &= Expect(sim.Enemies()[enemyIndex].statuses.stun > 0.0f, "charged+water applies immediate stun tick");
    ok &= Expect(sim.Enemies()[enemyIndex].statuses.stunSequence > 0.0f, "charged+water starts a repeated stun sequence");
    ok &= Expect(!EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Charged), "charged+water removes charged catalyst");
    ok &= Expect(!EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Wet), "charged+water suppresses incoming wet status");

    sim.PlacePlayer(sim.Enemies()[enemyIndex].position + rogue::Vec2{4.8f, 0.0f}, 1);
    bool sawFollowupStun = false;
    for (int frame = 0; frame < 75 && !sawFollowupStun; ++frame) {
        sim.Tick(rogue::InputState{}, 1.0f / 60.0f, world);
        sawFollowupStun = HasCombatStunReaction(sim, rogue::ReactionKind::ChargedWater);
    }
    ok &= Expect(sawFollowupStun, "charged+water emits a follow-up stun pulse during status lock");
    return ok;
}

bool TestBurningWaterClearsBothStatuses() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);

    const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
    bool ok = true;
    ok &= Expect(enemyIndex >= 0, "burning-water test finds a living enemy");
    if (enemyIndex < 0) {
        return ok;
    }

    CastGlovesElementAtEnemy(sim, world, enemyIndex, rogue::Element::Fire);
    ok &= Expect(EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Burning), "fire applies burning catalyst");

    CastGlovesElementAtEnemy(sim, world, enemyIndex, rogue::Element::Water);
    ok &= Expect(HasCombatReaction(sim, rogue::ReactionKind::BurningWater), "water reacts with burning catalyst");
    ok &= Expect(!EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Burning), "burning+water removes burning");
    ok &= Expect(!EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Wet), "burning+water suppresses incoming wet status");
    return ok;
}

bool TestBurningElectricityCreatesAreaMicroExplosion() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    const auto positions = PlaceFirstThreeRoomSpawnsForMicroExplosion(world);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);

    const int source = FindActiveEnemyNear(sim, 1, positions[0]);
    const int nearbyCharged = FindActiveEnemyNear(sim, 1, positions[1]);
    const int outsideBurning = FindActiveEnemyNear(sim, 1, positions[2]);

    bool ok = true;
    ok &= Expect(source >= 0, "micro explosion test finds source enemy");
    ok &= Expect(nearbyCharged >= 0, "micro explosion test finds nearby charged enemy");
    ok &= Expect(outsideBurning >= 0, "micro explosion test finds outside enemy");
    if (!ok) {
        return ok;
    }

    CastPistolElementAtEnemy(sim, world, source, rogue::Element::Fire);
    ok &= Expect(EnemyHasStatus(sim, source, rogue::StatusKind::Burning), "source starts with burning catalyst");

    CastPistolElementAtEnemy(sim, world, nearbyCharged, rogue::Element::Electricity);
    ok &= Expect(EnemyHasStatus(sim, nearbyCharged, rogue::StatusKind::Charged), "nearby target starts charged");

    CastPistolElementAtEnemy(sim, world, outsideBurning, rogue::Element::Fire);
    ok &= Expect(EnemyHasStatus(sim, outsideBurning, rogue::StatusKind::Burning), "outside target starts burning");

    const float sourceHpBefore = sim.Enemies()[source].hp;
    const float nearbyHpBefore = sim.Enemies()[nearbyCharged].hp;

    CastPistolElementAtEnemy(sim, world, source, rogue::Element::Electricity);

    ok &= Expect(HasCombatReaction(sim, rogue::ReactionKind::BurningElectricity), "burning+electricity emits micro explosion reaction");
    ok &= Expect(CountCombatDamageWithReaction(sim, rogue::ReactionKind::BurningElectricity) >= 2, "micro explosion damages source and nearby target");
    ok &= Expect(HasCombatDamageWithReactionForEntity(sim, rogue::ReactionKind::BurningElectricity, source), "source receives micro explosion damage event");
    ok &= Expect(HasCombatDamageWithReactionForEntity(sim, rogue::ReactionKind::BurningElectricity, nearbyCharged), "nearby target receives micro explosion damage event");
    ok &= Expect(!HasCombatDamageWithReactionForEntity(sim, rogue::ReactionKind::BurningElectricity, outsideBurning), "outside target receives no micro explosion damage event");
    ok &= Expect(sim.Enemies()[source].hp < sourceHpBefore, "source takes direct and micro explosion damage");
    ok &= Expect(sim.Enemies()[nearbyCharged].hp < nearbyHpBefore, "nearby target takes area reaction damage");
    ok &= Expect(!EnemyHasStatus(sim, source, rogue::StatusKind::Burning), "micro explosion removes burning from source");
    ok &= Expect(!EnemyHasStatus(sim, source, rogue::StatusKind::Charged), "micro explosion suppresses incoming charged on source");
    ok &= Expect(!EnemyHasStatus(sim, nearbyCharged, rogue::StatusKind::Charged), "micro explosion removes charged from nearby target");
    ok &= Expect(EnemyHasStatus(sim, outsideBurning, rogue::StatusKind::Burning), "outside target keeps burning outside radius");
    return ok;
}

bool TestPlayerMicroExplosionUsesSharedAreaPipeline() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    const auto positions = PlaceRoomSpawnsForPlayerMicroExplosion(world);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);

    const int boss = FindActiveEnemyNear(sim, 1, positions[0]);
    const int caster = FindActiveEnemyNear(sim, 1, positions[1]);
    const int nearbyVictim = FindActiveEnemyNear(sim, 1, positions[2]);

    bool ok = true;
    ok &= Expect(boss >= 0, "player micro explosion test finds fire boss");
    ok &= Expect(caster >= 0, "player micro explosion test finds electric caster");
    ok &= Expect(nearbyVictim >= 0, "player micro explosion test finds nearby enemy victim");
    if (!ok) {
        return ok;
    }

    sim.PlacePlayer(world.rooms[1].center, 1);
    sim.Tick(rogue::InputState{}, 1.0f / 60.0f, world);
    ok &= Expect(PlayerHasStatus(sim, rogue::StatusKind::Burning), "boss fire attack applies burning to player");
    ok &= Expect(HasCombatEventWithFaction(sim, rogue::CombatEventType::ProjectileSpawned, rogue::Faction::Enemy), "caster creates electric projectile toward burning player");

    bool sawReaction = false;
    bool sawNearbyDamage = false;
    for (int frame = 0; frame < 150 && !sawReaction; ++frame) {
        sim.Tick(rogue::InputState{}, 1.0f / 60.0f, world);
        sawReaction = HasCombatReaction(sim, rogue::ReactionKind::BurningElectricity);
        sawNearbyDamage = HasCombatDamageWithReactionForEntity(sim, rogue::ReactionKind::BurningElectricity, nearbyVictim);
    }

    ok &= Expect(sawReaction, "electric hit on burning player emits burning+electricity reaction");
    ok &= Expect(sawNearbyDamage, "player-centered micro explosion damages nearby enemy");
    ok &= Expect(!PlayerHasStatus(sim, rogue::StatusKind::Burning), "player burning catalyst is cleared by micro explosion");
    ok &= Expect(!PlayerHasStatus(sim, rogue::StatusKind::Charged), "incoming charged status is suppressed by micro explosion");
    return ok;
}

bool TestWetAirTransfersWetToNearbyNonHitTargets() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    const auto positions = PlaceFirstThreeRoomSpawnsForWetAir(world);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);

    const int source = FindActiveEnemyNear(sim, 1, positions[0]);
    const int sameAirHitTarget = FindActiveEnemyNear(sim, 1, positions[1]);
    const int spreadTarget = FindActiveEnemyNear(sim, 1, positions[2]);

    bool ok = true;
    ok &= Expect(source >= 0, "wet-air test finds source enemy");
    ok &= Expect(sameAirHitTarget >= 0, "wet-air test finds same-hit exclusion enemy");
    ok &= Expect(spreadTarget >= 0, "wet-air test finds nearby spread target");
    if (!ok) {
        return ok;
    }

    sim.PlacePlayer(positions[0] + rogue::Vec2{-0.85f, 0.0f}, 1);
    sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Pistol, rogue::Element::Water);
    rogue::InputState shot{};
    shot.action1 = true;
    shot.aimX = 1.0f;
    sim.Tick(shot, 1.0f / 60.0f, world);

    ok &= Expect(EnemyHasStatus(sim, source, rogue::StatusKind::Wet), "single water shot wets only the source");
    ok &= Expect(!EnemyHasStatus(sim, sameAirHitTarget, rogue::StatusKind::Wet), "same-hit target starts dry");
    ok &= Expect(!EnemyHasStatus(sim, spreadTarget, rogue::StatusKind::Wet), "spread target starts dry");

    sim.PlacePlayer(positions[0], 1);
    sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, rogue::Element::Air);
    rogue::InputState pulse{};
    pulse.action1 = true;
    pulse.aimX = 1.0f;
    sim.Tick(pulse, 1.0f / 60.0f, world);

    ok &= Expect(HasCombatReaction(sim, rogue::ReactionKind::WetAir), "air hit reacts with wet catalyst");
    ok &= Expect(HasCombatEvent(sim, rogue::CombatEventType::KnockbackApplied), "wet+air applies air knockback");
    ok &= Expect(!EnemyHasStatus(sim, source, rogue::StatusKind::Wet), "wet+air removes wet from source");
    ok &= Expect(!EnemyHasStatus(sim, sameAirHitTarget, rogue::StatusKind::Wet), "same air hit target is excluded from wet transfer");
    ok &= Expect(EnemyHasStatus(sim, spreadTarget, rogue::StatusKind::Wet), "nearby non-hit target receives transferred wet");
    return ok;
}

bool TestAirReactsWithChargedAndChilledWithoutClearingCatalysts() {
    bool ok = true;
    auto runCase = [](rogue::Element catalystElement, rogue::StatusKind catalystStatus, rogue::ReactionKind reaction, const char* findMessage, const char* catalystMessage, const char* reactionMessage, const char* keepMessage) {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        rogue::CombatSim sim;
        sim.Reset(world);
        world.rooms[1].locked = false;
        world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
        sim.ActivateEnemiesInRoom(1);

        bool caseOk = true;
        const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
        caseOk &= Expect(enemyIndex >= 0, findMessage);
        if (enemyIndex < 0) {
            return caseOk;
        }

        CastGlovesElementAtEnemy(sim, world, enemyIndex, catalystElement);
        caseOk &= Expect(EnemyHasStatus(sim, enemyIndex, catalystStatus), catalystMessage);

        CastGlovesElementAtEnemy(sim, world, enemyIndex, rogue::Element::Air);
        caseOk &= Expect(HasCombatReaction(sim, reaction), reactionMessage);
        caseOk &= Expect(HasCombatEvent(sim, rogue::CombatEventType::KnockbackApplied), "air catalyst reaction emits knockback event");
        caseOk &= Expect(EnemyHasStatus(sim, enemyIndex, catalystStatus), keepMessage);
        return caseOk;
    };

    ok &= runCase(
        rogue::Element::Electricity,
        rogue::StatusKind::Charged,
        rogue::ReactionKind::ChargedAir,
        "charged-air test finds a living enemy",
        "electricity applies charged catalyst before air",
        "air reacts with charged catalyst",
        "charged+air keeps charged catalyst");
    ok &= runCase(
        rogue::Element::Ice,
        rogue::StatusKind::Chilled,
        rogue::ReactionKind::ChilledAir,
        "chilled-air test finds a living enemy",
        "ice applies chilled catalyst before air",
        "air reacts with chilled catalyst",
        "chilled+air keeps chilled catalyst");
    return ok;
}

bool TestStoneShieldBypassAndChilledStoneStrip() {
    auto setupShieldCase = [](rogue::RoomGraph& world, rogue::CombatSim& sim) {
        world = rogue::GenerateWorld(0x96u);
        sim.Reset(world);
        for (int room = 1; room < world.roomCount; ++room) {
            world.rooms[room].locked = false;
            world.rooms[room].lifecycle = rogue::RoomLifecycle::Active;
            sim.ActivateEnemiesInRoom(room);
        }
        return FindActiveShieldedEnemy(sim);
    };
    auto placeForGlovesPulse = [](rogue::CombatSim& sim, int enemyIndex) {
        const rogue::EnemyState& enemy = sim.Enemies()[enemyIndex];
        sim.PlacePlayer(enemy.position + rogue::Vec2{-0.75f, 0.0f}, enemy.roomIndex);
    };
    auto pulse = [](rogue::CombatSim& sim, rogue::RoomGraph& world) {
        rogue::InputState input{};
        input.action1 = true;
        input.aimX = 1.0f;
        sim.Tick(input, 1.0f / 60.0f, world);
    };

    bool ok = true;
    {
        rogue::RoomGraph world{};
        rogue::CombatSim sim;
        const int enemyIndex = setupShieldCase(world, sim);
        ok &= Expect(enemyIndex >= 0, "shield bypass test finds a shielded enemy");
        if (enemyIndex >= 0) {
            const float hpBefore = sim.Enemies()[enemyIndex].hp;
            const float shieldBefore = sim.Enemies()[enemyIndex].shield;
            placeForGlovesPulse(sim, enemyIndex);
            sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, rogue::Element::Fire);
            pulse(sim, world);

            ok &= Expect(HasCombatEvent(sim, rogue::CombatEventType::ShieldHit), "non-stone hit is absorbed by shield");
            ok &= Expect(sim.Enemies()[enemyIndex].shield < shieldBefore, "non-stone hit reduces shield");
            ok &= Expect(sim.Enemies()[enemyIndex].hp >= hpBefore - 0.001f, "non-stone shield hit leaves HP protected");
        }
    }
    {
        rogue::RoomGraph world{};
        rogue::CombatSim sim;
        const int enemyIndex = setupShieldCase(world, sim);
        ok &= Expect(enemyIndex >= 0, "stone bypass test finds a shielded enemy");
        if (enemyIndex >= 0) {
            const float hpBefore = sim.Enemies()[enemyIndex].hp;
            const float shieldBefore = sim.Enemies()[enemyIndex].shield;
            placeForGlovesPulse(sim, enemyIndex);
            sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, rogue::Element::Stone);
            pulse(sim, world);

            ok &= Expect(!HasCombatEvent(sim, rogue::CombatEventType::ShieldHit), "stone hit does not emit ordinary shield absorption");
            ok &= Expect(sim.Enemies()[enemyIndex].shield >= shieldBefore - 0.001f, "stone hit bypasses shield without reducing it");
            ok &= Expect(sim.Enemies()[enemyIndex].hp < hpBefore, "stone hit damages HP through shield");
        }
    }
    {
        rogue::RoomGraph world{};
        rogue::CombatSim sim;
        const int enemyIndex = setupShieldCase(world, sim);
        ok &= Expect(enemyIndex >= 0, "chilled stone test finds a shielded enemy");
        if (enemyIndex >= 0) {
            placeForGlovesPulse(sim, enemyIndex);
            sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, rogue::Element::Ice);
            pulse(sim, world);
            ok &= Expect(EnemyHasStatus(sim, enemyIndex, rogue::StatusKind::Chilled), "ice hit applies chilled before stone strip");

            const float hpBeforeStone = sim.Enemies()[enemyIndex].hp;
            const float shieldBeforeStone = sim.Enemies()[enemyIndex].shield;
            sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Gloves, rogue::Element::Stone);
            pulse(sim, world);

            ok &= Expect(HasCombatReaction(sim, rogue::ReactionKind::ChilledStone), "chilled+stone emits shield strip reaction");
            ok &= Expect(HasCombatEvent(sim, rogue::CombatEventType::ShieldHit), "chilled+stone emits shield strip event");
            ok &= Expect(sim.Enemies()[enemyIndex].shield < shieldBeforeStone, "chilled+stone strips shield");
            ok &= Expect(sim.Enemies()[enemyIndex].hp < hpBeforeStone, "chilled+stone still damages HP through shield");
        }
    }
    return ok;
}

bool TestEnemyWeaponsApplyPlayerStatusesThroughSamePipeline() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);

    for (int room = 1; room < world.roomCount; ++room) {
        world.rooms[room].locked = false;
        world.rooms[room].lifecycle = rogue::RoomLifecycle::Active;
        sim.ActivateEnemiesInRoom(room);
    }

    const int casterIndex = FindActiveEnemyKind(sim, rogue::EnemyKind::Caster);

    bool ok = true;
    ok &= Expect(casterIndex >= 0, "enemy pipeline test finds an electric caster");
    if (casterIndex < 0) {
        return ok;
    }

    ok &= Expect(PlacePlayerAtEnemyRange(sim, world, casterIndex, 4.4f), "enemy pipeline test places player in caster projectile range");
    if (!ok) {
        return ok;
    }
    sim.Tick(rogue::InputState{}, 1.0f / 60.0f, world);

    ok &= Expect(HasCombatActionEvent(sim, rogue::Faction::Enemy, rogue::WeaponId::Staff, rogue::Element::Electricity, rogue::AttackShape::Projectile, 1.0f), "enemy caster uses staff projectile action");
    ok &= Expect(HasCombatEventWithFaction(sim, rogue::CombatEventType::ProjectileSpawned, rogue::Faction::Enemy), "enemy projectile spawn keeps enemy faction");
    ok &= Expect(TickUntilPlayerStatus(sim, world, rogue::StatusKind::Charged, 120), "enemy electric projectile applies charged to player through shared pipeline");
    return ok;
}

bool TestPlayerStatusOverlayContract() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);

    for (int room = 1; room < world.roomCount; ++room) {
        world.rooms[room].locked = false;
        world.rooms[room].lifecycle = rogue::RoomLifecycle::Active;
        sim.ActivateEnemiesInRoom(room);
    }

    const int casterIndex = FindActiveEnemyKind(sim, rogue::EnemyKind::Caster);

    bool ok = true;
    ok &= Expect(casterIndex >= 0, "status overlay test finds an electric caster");
    if (casterIndex < 0) {
        return ok;
    }

    ok &= Expect(PlacePlayerAtEnemyRange(sim, world, casterIndex, 4.4f), "status overlay test places player in caster projectile range");
    if (!ok) {
        return ok;
    }
    ok &= Expect(TickUntilPlayerStatus(sim, world, rogue::StatusKind::Charged, 120), "shared enemy hit leaves player charged");
    const rogue::RenderScene scene = rogue::BuildRenderScene(world, sim, 1280, 720, 2.5f, 14u);

    ok &= Expect((scene.overlay.overlayPlayerStatusMask & rogue::kOverlayStatusChargedBit) != 0u, "render overlay exposes player charged status");
    ok &= Expect((scene.overlay.overlayPlayerStatusMask & rogue::kOverlayStatusWetBit) == 0u, "render overlay does not invent unrelated statuses");
    return ok;
}

bool TestEnemyPressureRolesUseDistinctActions() {
    bool ok = true;
    auto runRole = [](
                       rogue::EnemyKind kind,
                       float distance,
                       rogue::WeaponId weapon,
                       rogue::Element element,
                       rogue::AttackShape shape,
                       bool expectProjectile,
                       rogue::StatusKind expectedStatus,
                       const char* findMessage,
                       const char* placeMessage,
                       const char* actionMessage,
                       const char* feedbackMessage) {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        rogue::CombatSim sim;
        sim.Reset(world);
        for (int room = 1; room < world.roomCount; ++room) {
            world.rooms[room].locked = false;
            world.rooms[room].lifecycle = rogue::RoomLifecycle::Active;
            sim.ActivateEnemiesInRoom(room);
        }

        bool roleOk = true;
        const int enemyIndex = FindActiveEnemyKind(sim, kind);
        roleOk &= Expect(enemyIndex >= 0, findMessage);
        if (enemyIndex < 0) {
            return roleOk;
        }
        roleOk &= Expect(PlacePlayerAtEnemyRange(sim, world, enemyIndex, distance), placeMessage);
        if (!roleOk) {
            return roleOk;
        }

        sim.Tick(rogue::InputState{}, 1.0f / 60.0f, world);
        roleOk &= Expect(HasCombatActionEvent(sim, rogue::Faction::Enemy, weapon, element, shape, 1.0f), actionMessage);
        if (expectProjectile) {
            roleOk &= Expect(HasCombatEventWithFaction(sim, rogue::CombatEventType::ProjectileSpawned, rogue::Faction::Enemy), feedbackMessage);
            roleOk &= Expect(HasActiveProjectileWithOwner(sim, rogue::Faction::Enemy), "enemy projectile remains visible for gameplay/DXR feedback");
        } else if (expectedStatus != rogue::StatusKind::None) {
            roleOk &= Expect(PlayerHasStatus(sim, expectedStatus), feedbackMessage);
        }
        return roleOk;
    };

    ok &= runRole(
        rogue::EnemyKind::Caster,
        4.4f,
        rogue::WeaponId::Staff,
        rogue::Element::Electricity,
        rogue::AttackShape::Projectile,
        true,
        rogue::StatusKind::None,
        "enemy pressure roles find caster",
        "enemy pressure roles place player near caster",
        "caster uses electric staff projectile",
        "caster projectile uses enemy projectile feedback");
    ok &= runRole(
        rogue::EnemyKind::Skirmisher,
        3.2f,
        rogue::WeaponId::Katana,
        rogue::Element::Air,
        rogue::AttackShape::Wave,
        true,
        rogue::StatusKind::None,
        "enemy pressure roles find skirmisher",
        "enemy pressure roles place player near skirmisher",
        "skirmisher uses air katana wave",
        "skirmisher wave uses enemy projectile feedback");
    ok &= runRole(
        rogue::EnemyKind::Bulwark,
        2.4f,
        rogue::WeaponId::Shotgun,
        rogue::Element::Ice,
        rogue::AttackShape::Cone,
        false,
        rogue::StatusKind::Chilled,
        "enemy pressure roles find bulwark",
        "enemy pressure roles place player near bulwark",
        "bulwark uses ice shotgun cone",
        "bulwark cone applies chilled through player status pipeline");

    return ok;
}

bool TestBossFinalRoomPhaseContract() {
    auto runBossPhase = [](
                            float setupDamage,
                            float distance,
                            rogue::WeaponId weapon,
                            rogue::Element element,
                            rogue::AttackShape shape,
                            float actionValue,
                            bool expectProjectile,
                            rogue::StatusKind expectedStatus,
                            const char* actionMessage,
                            const char* feedbackMessage,
                            const char* statusMessage) {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        KeepOnlyFinalBossSpawn(world);
        rogue::CombatSim sim;
        sim.Reset(world);
        const int finalRoom = world.roomCount - 1;
        world.rooms[finalRoom].locked = false;
        world.rooms[finalRoom].lifecycle = rogue::RoomLifecycle::Active;
        sim.ActivateEnemiesInRoom(finalRoom);
        if (setupDamage > 0.0f) {
            sim.DamageEnemiesInRoom(finalRoom, setupDamage);
        }

        bool phaseOk = true;
        const int bossIndex = FindActiveEnemyKind(sim, rogue::EnemyKind::Boss);
        phaseOk &= Expect(bossIndex >= 0, "boss phase test finds active boss");
        if (bossIndex < 0) {
            return phaseOk;
        }
        phaseOk &= Expect(PlacePlayerAtEnemyRange(sim, world, bossIndex, distance), "boss phase test places player in attack range");
        if (!phaseOk) {
            return phaseOk;
        }

        sim.Tick(rogue::InputState{}, 1.0f / 60.0f, world);
        phaseOk &= Expect(HasCombatActionEvent(sim, rogue::Faction::Enemy, weapon, element, shape, actionValue), actionMessage);
        if (expectProjectile) {
            phaseOk &= Expect(HasCombatEventWithFaction(sim, rogue::CombatEventType::ProjectileSpawned, rogue::Faction::Enemy), feedbackMessage);
            phaseOk &= Expect(HasActiveProjectileWithOwner(sim, rogue::Faction::Enemy), "boss projectile remains visible for DXR feedback");
        } else {
            phaseOk &= Expect(HasCombatEventWithFaction(sim, rogue::CombatEventType::ActorDamaged, rogue::Faction::Player), feedbackMessage);
            if (expectedStatus != rogue::StatusKind::None) {
                const bool hasStatus = PlayerHasStatus(sim, expectedStatus) ||
                    HasCombatStatusEvent(sim, rogue::Faction::Player, expectedStatus);
                phaseOk &= Expect(hasStatus, statusMessage);
            }
        }
        return phaseOk;
    };

    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    KeepOnlyFinalBossSpawn(world);
    rogue::CombatSim sim;
    sim.Reset(world);
    const int finalRoom = world.roomCount - 1;
    world.rooms[finalRoom].locked = false;
    world.rooms[finalRoom].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(finalRoom);

    const int bossIndex = FindActiveEnemyKind(sim, rogue::EnemyKind::Boss);
    const auto proxies = rogue::BuildEntityProxies(sim);
    const auto geometry = rogue::GenerateRTGeometry(proxies);

    bool ok = true;
    ok &= Expect(world.rooms[finalRoom].objective.kind == rogue::RoomObjectiveKind::KillAll, "final room requires killing the boss pressure");
    ok &= Expect(bossIndex >= 0, "final room activates a boss actor");
    if (bossIndex >= 0) {
        ok &= Expect(sim.Enemies()[bossIndex].hp >= 250.0f, "boss starts with a larger HP pool");
        ok &= Expect(sim.Enemies()[bossIndex].shield > 0.0f, "boss starts with shield pressure");
    }
    ok &= Expect(HasProxyKind(proxies, rogue::EntityProxyKind::EnemyBoss), "RT proxies include boss silhouette");
    ok &= Expect(!geometry.triangles.empty(), "boss silhouette emits procedural RT geometry");

    ok &= runBossPhase(
        0.0f,
        5.2f,
        rogue::WeaponId::Scepter,
        rogue::Element::Fire,
        rogue::AttackShape::TargetArea,
        0.0f,
        false,
        rogue::StatusKind::Burning,
        "boss phase 1 uses fire scepter target-area pressure",
        "boss phase 1 damages player through shared pipeline",
        "boss phase 1 applies burning through shared player status pipeline");
    ok &= runBossPhase(
        180.0f,
        5.6f,
        rogue::WeaponId::Staff,
        rogue::Element::Electricity,
        rogue::AttackShape::Projectile,
        1.0f,
        true,
        rogue::StatusKind::None,
        "boss phase 2 uses electric staff projectile pressure",
        "boss phase 2 spawns enemy projectile feedback",
        "");
    ok &= runBossPhase(
        255.0f,
        2.6f,
        rogue::WeaponId::Shotgun,
        rogue::Element::Ice,
        rogue::AttackShape::Cone,
        1.0f,
        false,
        rogue::StatusKind::Chilled,
        "boss phase 3 uses ice shotgun close pressure",
        "boss phase 3 damages player through shared pipeline",
        "boss phase 3 applies chilled through shared player status pipeline");
    return ok;
}

bool TestChargedDischargeHitsLaneAndDoesNotRepeatSameInstance() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    const std::array<rogue::Vec2, 3> enemyPositions = PlaceFirstThreeRoomSpawnsForDischarge(world);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);

    const int left = FindActiveEnemyNear(sim, 1, enemyPositions[0]);
    const int right = FindActiveEnemyNear(sim, 1, enemyPositions[1]);
    const int middle = FindActiveEnemyNear(sim, 1, enemyPositions[2]);
    bool ok = true;
    ok &= Expect(left >= 0 && right >= 0 && middle >= 0, "discharge test finds three deterministic room enemies");
    if (left < 0 || right < 0 || middle < 0) {
        return ok;
    }

    CastGlovesElementAtEnemy(sim, world, left, rogue::Element::Electricity);
    ok &= Expect(EnemyHasStatus(sim, left, rogue::StatusKind::Charged), "first endpoint receives charged");
    ok &= Expect(!EnemyHasStatus(sim, middle, rogue::StatusKind::Charged), "middle target is not directly charged by first hit");
    ok &= Expect(CountCombatReaction(sim, rogue::ReactionKind::ElectricDischarge) == 0, "one charged endpoint cannot discharge alone");

    const float middleBeforeDischarge = sim.Enemies()[middle].hp;
    CastGlovesElementAtEnemy(sim, world, right, rogue::Element::Electricity);
    const float middleAfterDischarge = sim.Enemies()[middle].hp;

    ok &= Expect(EnemyHasStatus(sim, right, rogue::StatusKind::Charged), "second endpoint receives charged");
    ok &= Expect(!EnemyHasStatus(sim, middle, rogue::StatusKind::Charged), "middle target is hit by discharge, not direct charged status");
    ok &= Expect(CountCombatReaction(sim, rogue::ReactionKind::ElectricDischarge) == 1, "second charged endpoint creates one discharge link");
    ok &= Expect(CountCombatDamageWithReaction(sim, rogue::ReactionKind::ElectricDischarge) >= 3, "discharge damages both endpoints and lane target");
    ok &= Expect(middleAfterDischarge < middleBeforeDischarge, "lane target loses hp from electric discharge");

    sim.Tick(rogue::InputState{}, 1.0f / 60.0f, world);
    ok &= Expect(CountCombatReaction(sim, rogue::ReactionKind::ElectricDischarge) == 0, "existing charged instance does not repeat discharge passively");
    return ok;
}

bool TestChargedRefreshResetsDischargeUniqueness() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    const std::array<rogue::Vec2, 3> enemyPositions = PlaceFirstThreeRoomSpawnsForDischarge(world);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);

    const int left = FindActiveEnemyNear(sim, 1, enemyPositions[0]);
    const int right = FindActiveEnemyNear(sim, 1, enemyPositions[1]);
    const int middle = FindActiveEnemyNear(sim, 1, enemyPositions[2]);
    bool ok = true;
    ok &= Expect(left >= 0 && right >= 0 && middle >= 0, "refresh discharge test finds three deterministic room enemies");
    if (left < 0 || right < 0 || middle < 0) {
        return ok;
    }

    CastGlovesElementAtEnemy(sim, world, left, rogue::Element::Electricity);
    CastGlovesElementAtEnemy(sim, world, right, rogue::Element::Electricity);
    const float middleAfterFirstDischarge = sim.Enemies()[middle].hp;
    ok &= Expect(CountCombatReaction(sim, rogue::ReactionKind::ElectricDischarge) == 1, "initial charged pair discharges once");

    CastGlovesElementAtEnemy(sim, world, right, rogue::Element::Electricity);
    ok &= Expect(HasCombatEvent(sim, rogue::CombatEventType::StatusRefreshed), "refreshing charged emits status refresh");
    ok &= Expect(CountCombatReaction(sim, rogue::ReactionKind::ElectricDischarge) == 1, "new charged instance can discharge again");
    ok &= Expect(sim.Enemies()[middle].hp < middleAfterFirstDischarge, "refreshed charged instance damages lane target again");
    ok &= Expect(EnemyHasStatus(sim, left, rogue::StatusKind::Charged), "discharge does not remove left charged status");
    ok &= Expect(EnemyHasStatus(sim, right, rogue::StatusKind::Charged), "discharge does not remove right charged status");
    return ok;
}

bool TestEnemyDamagedEvent() {
    rogue::GameSession session;
    session.Start(0x96u);
    session.TryEnterRoom(1);

    session.DamageRoomEnemies(1, 10.0f);

    bool ok = true;
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::EnemyDamaged), "enemy damage event is emitted");
    ok &= Expect(!HasEvent(session.LastTick(), rogue::GameEventType::EnemyKilled), "non-lethal damage does not emit kill event");
    ok &= Expect(FirstEventValue(session.LastTick(), rogue::GameEventType::EnemyDamaged) == 10.0f, "enemy damage event stores damage value");
    return ok;
}

bool TestPlayerDamageFeedbackContract() {
    rogue::GameSession session;
    session.Start(0x96u);
    session.TryEnterRoom(1);

    rogue::GameEvent damageEvent{};
    bool sawPlayerDamage = false;
    for (int frame = 0; frame < 600 && session.Status() == rogue::RunStatus::InProgress && !sawPlayerDamage; ++frame) {
        session.Tick(rogue::InputState{}, 1.0f / 60.0f);
        const rogue::GameSessionTickResult& tick = session.LastTick();
        for (int i = 0; i < tick.eventCount; ++i) {
            if (tick.events[i].type == rogue::GameEventType::PlayerDamaged) {
                damageEvent = tick.events[i];
                sawPlayerDamage = true;
                break;
            }
        }
    }

    const rogue::RenderScene scene = rogue::BuildRenderScene(
        session.World(),
        session.Combat(),
        1280,
        720,
        2.0f,
        19u);

    bool ok = true;
    ok &= Expect(sawPlayerDamage, "room pressure eventually emits player damage");
    ok &= Expect(damageEvent.value > 0.0f, "player damage event stores damage value");
    ok &= Expect(static_cast<int>(damageEvent.weapon) >= 0 && static_cast<int>(damageEvent.weapon) < static_cast<int>(rogue::WeaponId::Count), "player damage event carries source weapon");
    ok &= Expect(damageEvent.element != rogue::Element::None, "player damage event carries source element");
    ok &= Expect(scene.overlay.overlayHp < 100u, "render overlay reflects player hp loss");
    return ok;
}

bool TestSurviveTimerObjectiveCompletion() {
    rogue::GameSession session;
    session.Start(0x96u);
    session.TryEnterRoom(1);
    session.DamageRoomEnemies(1, 9999.0f);
    ChooseReward(session, 0);

    bool ok = true;
    ok &= Expect(session.TryEnterRoom(2), "survival room can be entered after room 1 clear");
    ok &= Expect(session.World().rooms[2].objective.kind == rogue::RoomObjectiveKind::SurviveTimer, "room 2 is survival");

    rogue::InputState idle{};
    bool sawCompletion = false;
    for (int i = 0; i < 90 && session.Status() == rogue::RunStatus::InProgress; ++i) {
        session.Tick(idle, 1.0f / 60.0f);
        if (session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Completed) {
            sawCompletion = HasEvent(session.LastTick(), rogue::GameEventType::ObjectiveCompleted);
            break;
        }
    }

    ok &= Expect(session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Completed, "survival room completes after timer");
    ok &= Expect(session.Phase() == rogue::RunPhase::RewardChoice, "survival completion offers reward");
    ok &= Expect(session.Combat().ActiveEnemiesInRoom(2) == 0, "survival reward phase despawns active enemies");
    ok &= Expect(session.Combat().LivingEnemiesInRoom(2) == 0, "survival reward phase clears living room pressure");
    ok &= Expect(session.World().rooms[3].lifecycle == rogue::RoomLifecycle::Locked, "survival reward pending keeps next room locked");
    ok &= Expect(sawCompletion, "survival objective emits completion event");
    ChooseReward(session, 1);
    ok &= Expect(session.World().rooms[3].lifecycle == rogue::RoomLifecycle::Available, "survival reward selection unlocks next room");
    return ok;
}

bool TestControlPointObjectiveCompletion() {
    rogue::GameSession session;
    session.Start(0x96u);
    session.TryEnterRoom(1);
    session.DamageRoomEnemies(1, 9999.0f);
    ChooseReward(session, 0);
    session.TryEnterRoom(2);
    for (int i = 0; i < 90 && session.Status() == rogue::RunStatus::InProgress; ++i) {
        session.Tick(rogue::InputState{}, 1.0f / 60.0f);
        if (session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Completed) {
            break;
        }
    }
    ChooseReward(session, 1);

    bool ok = true;
    ok &= Expect(session.TryEnterRoom(3), "control point room can be entered");
    ok &= Expect(session.World().rooms[3].objective.kind == rogue::RoomObjectiveKind::ControlPoint, "room 3 is a control point objective");
    ok &= Expect(session.World().rooms[3].objective.controlRadius > 0.0f, "control point exposes a control zone");
    ok &= Expect(session.Combat().ActiveEnemiesInRoom(3) > 0, "control point starts with live pressure");
    session.DamageRoomEnemies(3, 9999.0f);
    ok &= Expect(session.World().rooms[3].lifecycle == rogue::RoomLifecycle::Active, "control point does not complete from kills alone");

    ok &= Expect(TickToward(session, session.World().rooms[3].objective.controlPoint, 240), "player can move into control zone");
    for (int i = 0; i < 30 && session.Status() == rogue::RunStatus::InProgress; ++i) {
        session.Tick(rogue::InputState{}, 1.0f / 60.0f);
    }
    const rogue::RenderScene controlScene = rogue::BuildRenderScene(
        session.World(),
        session.Combat(),
        1280,
        720,
        2.0f,
        32u);
    ok &= Expect(controlScene.overlay.overlayObjectiveKind == static_cast<uint32_t>(rogue::RoomObjectiveKind::ControlPoint), "overlay exposes control objective kind");
    ok &= Expect(controlScene.overlay.overlayObjectiveProgressPercent > 0u, "overlay exposes control objective progress");
    ok &= Expect(controlScene.overlay.overlayObjectiveProgressPercent < 100u, "overlay keeps control objective incomplete until hold finishes");

    ok &= Expect(CompleteCurrentRoomObjective(session), "control point completes by holding the control zone");
    ok &= Expect(session.Phase() == rogue::RunPhase::RewardChoice, "control point completion offers reward");
    return ok;
}

bool TestFullFloorRouteProgressesAfterEveryReward() {
    rogue::GameSession session;
    session.Start(0x96u);

    bool ok = true;
    for (int room = 1; room < session.World().roomCount; ++room) {
        ok &= Expect(session.TryEnterRoom(room), "next room can be entered after previous reward");
        ok &= Expect(session.CurrentRoom() == room, "session current room advances");
        ok &= Expect(session.World().rooms[room].lifecycle == rogue::RoomLifecycle::Active, "entered room activates");

        ok &= Expect(CompleteCurrentRoomObjective(session), "active room objective can complete");
        ok &= Expect(session.World().rooms[room].lifecycle == rogue::RoomLifecycle::Completed, "completed room lifecycle is stored");

        if (room == session.World().roomCount - 1) {
            ok &= Expect(session.Status() == rogue::RunStatus::FloorComplete, "final room completes the floor");
            ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::FloorCompleted), "final room emits floor completion");
            break;
        }

        ok &= Expect(session.Status() == rogue::RunStatus::InProgress, "non-final clear keeps run in progress");
        ok &= Expect(session.Phase() == rogue::RunPhase::RewardChoice, "non-final clear pauses for reward");
        ok &= Expect(session.RewardOptionCount() == rogue::kRewardChoiceCount, "each reward pause offers three choices");
        ok &= Expect(session.World().rooms[room + 1].lifecycle == rogue::RoomLifecycle::Locked, "next room waits for reward selection");
        const rogue::PlayerState rewardPlayer = session.Combat().Player();
        const rogue::RewardOption& weaponSwap = session.RewardOptions()[0];
        const rogue::RewardOption& infusion = session.RewardOptions()[1];
        ok &= Expect(
            rogue::GetWeaponSpec(weaponSwap.weapon).category ==
                rogue::GetWeaponSpec(rewardPlayer.weaponSlots[weaponSwap.targetSlot].weapon).category,
            "full route weapon reward preserves the target role");
        ok &= Expect(
            RewardElementSynergyScore(rewardPlayer, weaponSwap.targetSlot, weaponSwap.element) > 0,
            "full route weapon reward keeps an elemental combo hook");
        ok &= Expect(weaponSwap.synergyElement != rogue::Element::None, "full route weapon reward exposes a synergy hint");
        ok &= Expect(
            RewardElementSynergyScore(rewardPlayer, infusion.targetSlot, infusion.element) > 0,
            "full route infusion reward keeps an elemental combo hook");
        ok &= Expect(infusion.synergyElement != rogue::Element::None, "full route infusion reward exposes a synergy hint");
        ok &= Expect(
            weaponSwap.element != infusion.element,
            "full route reward cards offer distinct elemental build hooks");

        rogue::InputState choose{};
        rogue::ApplyNumberInputBinding(choose, room % rogue::kRewardChoiceCount);
        session.Tick(choose, 0.0f);

        ok &= Expect(session.Phase() == rogue::RunPhase::Exploring, "reward selection resumes exploration");
        ok &= Expect(session.RewardOptionCount() == 0, "reward choices clear after selection");
        ok &= Expect(session.World().rooms[room + 1].lifecycle == rogue::RoomLifecycle::Available, "reward selection unlocks the next room");
        ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RewardSelected), "reward selection emits an event");
        ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::PortalOpened), "reward selection opens forward portal");
    }
    return ok;
}

bool TestAdvanceFloorCarriesBuildProgress() {
    rogue::GameSession session;
    session.Start(0x96u);

    bool ok = true;
    for (int room = 1; room < session.World().roomCount; ++room) {
        ok &= Expect(session.TryEnterRoom(room), "advance-floor run can enter the next room");
        ok &= Expect(CompleteCurrentRoomObjective(session), "advance-floor run can complete room objective");
        if (room == session.World().roomCount - 1) {
            break;
        }

        rogue::InputState choose{};
        rogue::ApplyNumberInputBinding(choose, room % rogue::kRewardChoiceCount);
        session.Tick(choose, 0.0f);
        ok &= Expect(session.Status() == rogue::RunStatus::InProgress, "advance-floor run resumes after reward");
    }

    ok &= Expect(session.Status() == rogue::RunStatus::FloorComplete, "advance-floor run reaches floor clear");
    const rogue::PlayerState before = session.Combat().Player();
    const uint32_t oldWorldHash = rogue::HashWorld(session.World());

    rogue::InputState advance{};
    advance.advanceFloor = true;
    session.Tick(advance, 0.0f);

    const rogue::PlayerState after = session.Combat().Player();
    ok &= Expect(session.Status() == rogue::RunStatus::InProgress, "advance-floor input starts the next floor");
    ok &= Expect(session.Phase() == rogue::RunPhase::Exploring, "advance-floor resumes exploration");
    ok &= Expect(session.FloorIndex() == 1, "advance-floor increments session floor index");
    ok &= Expect(session.World().floorIndex == 1, "advance-floor regenerates world with floor index");
    ok &= Expect(session.World().descent > 0.0f, "advance-floor exposes deeper descent");
    ok &= Expect(rogue::HashWorld(session.World()) != oldWorldHash, "advance-floor changes world hash");
    ok &= Expect(session.CurrentRoom() == 0, "advance-floor starts in room zero");
    ok &= Expect(after.roomIndex == 0, "advance-floor places player in start room");
    ok &= Expect(rogue::LengthSq(after.position - session.World().rooms[0].center) < 0.001f, "advance-floor places player at new start center");
    ok &= Expect(session.World().rooms[1].lifecycle == rogue::RoomLifecycle::Available, "advance-floor opens the first combat room");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::FloorStarted), "advance-floor emits floor started event");
    ok &= Expect(after.activeWeaponSlot == before.activeWeaponSlot, "advance-floor preserves active weapon slot");
    for (int slot = 0; slot < rogue::kPlayerWeaponSlots; ++slot) {
        ok &= Expect(after.weaponSlots[slot].weapon == before.weaponSlots[slot].weapon, "advance-floor preserves loadout weapon");
        ok &= Expect(after.weaponSlots[slot].element == before.weaponSlots[slot].element, "advance-floor preserves loadout element");
    }
    ok &= Expect(after.maxHp == before.maxHp, "advance-floor preserves max HP upgrades");
    ok &= Expect(after.damageMultiplier == before.damageMultiplier, "advance-floor preserves damage upgrades");
    ok &= Expect(after.cooldownMultiplier == before.cooldownMultiplier, "advance-floor preserves cooldown upgrades");
    ok &= Expect(after.speedMultiplier == before.speedMultiplier, "advance-floor preserves speed upgrades");
    ok &= Expect(after.areaMultiplier == before.areaMultiplier, "advance-floor preserves area upgrades");
    ok &= Expect(after.hp >= std::min(before.hp, before.maxHp * 0.50f), "advance-floor keeps survivable HP");
    ok &= Expect(after.weaponSlots[after.activeWeaponSlot].cooldowns[0] == 0.0f, "advance-floor clears Q cooldown");
    ok &= Expect(after.weaponSlots[after.activeWeaponSlot].cooldowns[1] == 0.0f, "advance-floor clears E cooldown");
    ok &= Expect(after.statuses.statusLock == 0.0f && after.statuses.stun == 0.0f, "advance-floor clears combat statuses");
    return ok;
}

bool TestPlayableFirstRoomRealCombatSmoke() {
    rogue::GameSession session;
    session.Start(0x96u);

    const PlayableCombatSmokeResult result = PlayFirstCombatRoomWithRealInputs(session, 900);
    bool ok = true;
    ok &= Expect(result.sawPrimary, "playable smoke uses Q primary action");
    ok &= Expect(result.sawAbility, "playable smoke uses E ability action");
    ok &= Expect(result.sawEnemyDamage, "playable smoke damages enemies through real combat");
    ok &= Expect(result.sawLivePressureHud, "playable smoke HUD tracks active slot and live room pressure");
    ok &= Expect(result.sawPrimaryCooldownHud, "playable smoke HUD exposes Q cooldown in real combat");
    ok &= Expect(result.sawAbilityCooldownHud, "playable smoke HUD exposes E cooldown in real combat");
    ok &= Expect(result.completed, "playable smoke clears the first combat room without debug damage");
    ok &= Expect(session.Phase() == rogue::RunPhase::RewardChoice, "real combat clear enters reward choice");
    ok &= Expect(session.RewardOptionCount() == rogue::kRewardChoiceCount, "real combat clear offers three rewards");
    ok &= Expect(result.frames <= 780, "first combat room real TTK stays onboarding-friendly");
    ok &= Expect(result.hp >= 45.0f, "first combat room leaves a readable survival buffer");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RoomCompleted), "real combat clear emits room completed");
    return ok;
}

bool TestPlayableEarlyRunRewardSurvivalSmoke() {
    rogue::GameSession session;
    session.Start(0x96u);

    const PlayableEarlyRunSmokeResult result = PlayEarlyRunWithRealInputs(session, 900, 210);
    bool ok = true;
    ok &= Expect(result.room1.completed, "early run clears room 1 through real combat");
    ok &= Expect(result.rewardSelected, "early run selects a reward through keyboard input");
    ok &= Expect(result.portalOpened, "early run reward selection opens the next portal");
    ok &= Expect(result.rewardCleared, "early run reward selection resumes exploration");
    ok &= Expect(result.room2Entered, "early run enters room 2 after reward");
    ok &= Expect(result.sawRoom2SurvivalHud, "early run HUD exposes room 2 survival objective");
    ok &= Expect(result.sawRoom2SurvivalProgressHud, "early run HUD shows live survival progress");
    ok &= Expect(result.sawRoom2LivePressureHud, "early run HUD tracks room 2 live pressure");
    ok &= Expect(result.sawRoom2Primary, "early run uses Q in room 2");
    ok &= Expect(result.sawRoom2Ability, "early run uses E in room 2");
    ok &= Expect(result.sawRoom2EnemyDamage, "early run damages room 2 enemies through real combat");
    ok &= Expect(result.room2Completed, "early run completes room 2 without debug damage");
    ok &= Expect(result.room2RewardOffered, "early run room 2 completion offers another reward");
    ok &= Expect(session.Phase() == rogue::RunPhase::RewardChoice, "early run pauses at room 2 reward choice");
    ok &= Expect(result.room2Frames <= 150, "early run survival room completes at readable pacing");
    ok &= Expect(result.hp >= 25.0f, "early run keeps the player alive after two rooms");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RoomCompleted), "early run room 2 emits room completed");
    return ok;
}

bool TestPlayableControlRoomRealInputSmoke() {
    rogue::GameSession session;
    session.Start(0x96u);

    const PlayableControlRunSmokeResult result = PlayControlRunWithRealInputs(session, 900, 210, 420);
    bool ok = true;
    ok &= Expect(result.early.room2RewardOffered, "control run reaches the room 2 reward through real input");
    ok &= Expect(result.room2RewardSelected, "control run selects the room 2 reward through keyboard input");
    ok &= Expect(result.room2PortalOpened, "control run reward selection opens room 3 portal");
    ok &= Expect(result.room3Unlocked, "control run reward selection unlocks room 3");
    ok &= Expect(result.room3Entered, "control run enters room 3");
    ok &= Expect(result.sawRoom3ControlHud, "control run HUD exposes control objective");
    ok &= Expect(result.sawRoom3ControlProgressHud, "control run HUD shows live control progress");
    ok &= Expect(result.sawRoom3ControlMarkerGeometry, "control run emits procedural control marker geometry");
    ok &= Expect(result.sawRoom3LivePressureHud, "control run HUD tracks room 3 live pressure");
    ok &= Expect(result.sawRoom3Primary, "control run uses Q while holding the zone");
    ok &= Expect(result.sawRoom3Ability, "control run uses E while holding the zone");
    ok &= Expect(result.sawRoom3EnemyDamage, "control run damages room 3 enemies through real combat");
    ok &= Expect(result.room3Completed, "control run completes room 3 without debug damage");
    ok &= Expect(result.room3RewardOffered, "control run completion offers a third reward");
    ok &= Expect(session.Phase() == rogue::RunPhase::RewardChoice, "control run pauses at room 3 reward choice");
    ok &= Expect(result.room3Frames <= 330, "control room completes at readable pacing");
    ok &= Expect(result.hp >= 12.0f, "control run keeps the player alive after three rooms");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RoomCompleted), "control run room 3 emits room completed");
    return ok;
}

bool TestPlayableFullRunBossSmoke() {
    rogue::GameSession session;
    session.Start(0x96u);

    const PlayableFullRunSmokeResult result = PlayFullRunWithRealInputs(session);
    bool ok = true;
    ok &= Expect(result.control.room3RewardOffered, "full run reaches the room 3 reward through real input");
    ok &= Expect(result.room3RewardSelected, "full run selects the room 3 reward through keyboard input");
    ok &= Expect(result.room3PortalOpened, "full run opens the room 4 portal");
    ok &= Expect(result.room4Unlocked, "full run unlocks room 4");
    ok &= Expect(result.room4.entered, "full run enters room 4");
    ok &= Expect(result.room4.sawLivePressureHud, "full run HUD tracks room 4 pressure");
    ok &= Expect(result.room4.sawPrimary, "full run uses Q in room 4");
    ok &= Expect(result.room4.sawAbility, "full run uses E in room 4");
    ok &= Expect(result.room4.sawEnemyDamage, "full run damages room 4 enemies through real combat");
    ok &= Expect(result.room4.completed, "full run clears room 4 without debug damage");
    ok &= Expect(result.room4.rewardOffered, "full run room 4 offers a reward");
    ok &= Expect(result.room4.frames <= 960, "full run room 4 TTK stays readable");
    ok &= Expect(result.room4RewardSelected, "full run selects the room 4 reward through keyboard input");
    ok &= Expect(result.room4PortalOpened, "full run opens the final room portal");
    ok &= Expect(result.finalRoomUnlocked, "full run unlocks the final room");
    ok &= Expect(result.final.entered, "full run enters the final room");
    ok &= Expect(result.final.sawLivePressureHud, "full run HUD tracks final room pressure");
    ok &= Expect(result.final.sawBossProxy, "full run renders a boss proxy in the final room");
    ok &= Expect(result.final.sawPrimary, "full run uses Q against final pressure");
    ok &= Expect(result.final.sawAbility, "full run uses E against final pressure");
    ok &= Expect(result.final.sawEnemyDamage, "full run damages final pressure through real combat");
    ok &= Expect(result.final.sawBossPhase2, "full run pushes boss into phase 2");
    ok &= Expect(result.final.sawBossPhase3, "full run pushes boss into phase 3");
    ok &= Expect(result.final.completed, "full run clears the final room without debug damage");
    ok &= Expect(result.final.floorCompleted, "full run reaches floor complete status");
    ok &= Expect(result.floorCompletedEvent, "full run emits floor completed event");
    ok &= Expect(result.final.frames <= 1380, "full run final boss TTK stays readable");
    ok &= Expect(result.hp > 0.0f, "full run keeps the player alive through the boss");
    return ok;
}

bool TestCombatDamageAndRtProxy() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.PlacePlayer(world.rooms[1].center, 1);
    sim.ActivateEnemiesInRoom(1);

    rogue::InputState cast{};
    cast.ranged = true;
    cast.aimX = 1.0f;
    for (int i = 0; i < 10; ++i) {
        sim.Tick(cast, 1.0f / 60.0f, world);
        cast.ranged = false;
    }

    const auto proxies = rogue::BuildEntityProxies(sim);
    const auto geometry = rogue::GenerateRTGeometry(proxies);

    bool ok = true;
    ok &= Expect(!proxies.empty(), "RT proxies are generated from combat state");
    ok &= Expect(!geometry.triangles.empty(), "RT proxy geometry emits triangles");
    return ok;
}

bool TestEnemyVarietyRtProxies() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);
    for (int room = 1; room < world.roomCount; ++room) {
        world.rooms[room].locked = false;
        world.rooms[room].lifecycle = rogue::RoomLifecycle::Active;
        sim.ActivateEnemiesInRoom(room);
    }

    const auto proxies = rogue::BuildEntityProxies(sim);
    const auto geometry = rogue::GenerateRTGeometry(proxies);

    bool ok = true;
    ok &= Expect(HasProxyKind(proxies, rogue::EntityProxyKind::EnemyBrute), "RT proxies include brute silhouette");
    ok &= Expect(HasProxyKind(proxies, rogue::EntityProxyKind::EnemyCaster), "RT proxies include caster silhouette");
    ok &= Expect(HasProxyKind(proxies, rogue::EntityProxyKind::EnemySkirmisher), "RT proxies include skirmisher silhouette");
    ok &= Expect(HasProxyKind(proxies, rogue::EntityProxyKind::EnemyBulwark), "RT proxies include bulwark silhouette");
    ok &= Expect(HasProxyKind(proxies, rogue::EntityProxyKind::EnemyBoss), "RT proxies include boss silhouette");
    ok &= Expect(!geometry.triangles.empty(), "enemy variety emits RT geometry");
    return ok;
}

bool TestProceduralCharacterAnatomyGeometry() {
    auto hashFor = [](std::span<const rogue::EntityRTProxy> proxies) {
        return rogue::HashPackedRTGeometry(rogue::PackRTGeometry(rogue::GenerateRTGeometry(proxies)));
    };
    auto materialCount = [](const rogue::GeneratedRTGeometry& geometry) {
        std::array<bool, 32> seen{};
        int count = 0;
        for (const rogue::RtTriangle& tri : geometry.triangles) {
            if (tri.materialId < seen.size() && !seen[tri.materialId]) {
                seen[tri.materialId] = true;
                ++count;
            }
        }
        return count;
    };
    auto enemyTag = [](rogue::EnemyKind kind, rogue::WeaponId weapon, rogue::Element element) {
        return static_cast<uint32_t>(kind) |
            (static_cast<uint32_t>(weapon) << 8u) |
            (static_cast<uint32_t>(element) << 16u);
    };

    const std::array<rogue::EntityRTProxy, 1> player{
        rogue::EntityRTProxy{
            rogue::EntityProxyKind::PlayerCore,
            rogue::Vec3{0.0f, 0.05f, 0.0f},
            rogue::Vec3{1.0f, 0.0f, 0.0f},
            0.70f,
            2u,
            0.52f,
            static_cast<uint32_t>(rogue::WeaponId::Katana) | (static_cast<uint32_t>(rogue::Element::Fire) << 8u)}
    };
    const std::array<rogue::EntityRTProxy, 1> brute{
        rogue::EntityRTProxy{rogue::EntityProxyKind::EnemyBrute, rogue::Vec3{0.0f, 0.0f, 0.0f}, rogue::Vec3{1.0f, 0.0f, 0.0f}, 0.84f, 4u, 0.48f, enemyTag(rogue::EnemyKind::Brute, rogue::WeaponId::Hammer, rogue::Element::Fire)}
    };
    const std::array<rogue::EntityRTProxy, 1> caster{
        rogue::EntityRTProxy{rogue::EntityProxyKind::EnemyCaster, rogue::Vec3{0.0f, 0.0f, 0.0f}, rogue::Vec3{1.0f, 0.0f, 0.0f}, 0.62f, 5u, 0.55f, enemyTag(rogue::EnemyKind::Caster, rogue::WeaponId::Staff, rogue::Element::Electricity)}
    };
    const std::array<rogue::EntityRTProxy, 1> skirmisher{
        rogue::EntityRTProxy{rogue::EntityProxyKind::EnemySkirmisher, rogue::Vec3{0.0f, 0.0f, 0.0f}, rogue::Vec3{1.0f, 0.0f, 0.0f}, 0.55f, 12u, 0.42f, enemyTag(rogue::EnemyKind::Skirmisher, rogue::WeaponId::Spear, rogue::Element::Air)}
    };
    const std::array<rogue::EntityRTProxy, 1> bulwark{
        rogue::EntityRTProxy{rogue::EntityProxyKind::EnemyBulwark, rogue::Vec3{0.0f, 0.0f, 0.0f}, rogue::Vec3{1.0f, 0.0f, 0.0f}, 0.96f, 13u, 0.50f, enemyTag(rogue::EnemyKind::Bulwark, rogue::WeaponId::Hammer, rogue::Element::Stone)}
    };
    const std::array<rogue::EntityRTProxy, 1> boss{
        rogue::EntityRTProxy{rogue::EntityProxyKind::EnemyBoss, rogue::Vec3{0.0f, 0.0f, 0.0f}, rogue::Vec3{1.0f, 0.0f, 0.0f}, 1.52f, 14u, 0.62f, enemyTag(rogue::EnemyKind::Boss, rogue::WeaponId::Scepter, rogue::Element::Fire)}
    };
    const std::array<rogue::EntityRTProxy, 1> bruteVariant{
        rogue::EntityRTProxy{rogue::EntityProxyKind::EnemyBrute, rogue::Vec3{0.0f, 0.0f, 0.0f}, rogue::Vec3{1.0f, 0.0f, 0.0f}, 0.84f, 4u, 0.48f, enemyTag(rogue::EnemyKind::Brute, rogue::WeaponId::Hammer, rogue::Element::Fire) | (3u << 29u)}
    };
    const std::array<rogue::EntityRTProxy, 1> playerVariant{
        rogue::EntityRTProxy{
            rogue::EntityProxyKind::PlayerCore,
            rogue::Vec3{0.0f, 0.05f, 0.0f},
            rogue::Vec3{1.0f, 0.0f, 0.0f},
            0.70f,
            2u,
            0.52f,
            static_cast<uint32_t>(rogue::WeaponId::Katana) | (static_cast<uint32_t>(rogue::Element::Fire) << 8u) | (5u << 24u)}
    };

    const auto playerGeometry = rogue::GenerateRTGeometry(player);
    const auto bruteGeometry = rogue::GenerateRTGeometry(brute);
    const auto casterGeometry = rogue::GenerateRTGeometry(caster);
    const auto skirmisherGeometry = rogue::GenerateRTGeometry(skirmisher);
    const auto bulwarkGeometry = rogue::GenerateRTGeometry(bulwark);
    const auto bossGeometry = rogue::GenerateRTGeometry(boss);

    bool ok = true;
    ok &= Expect(playerGeometry.triangles.size() > 520u, "player procedural anatomy has reference-grade body kit costume and weapon geometry");
    ok &= Expect(bruteGeometry.triangles.size() > 500u, "brute procedural anatomy has skull ribs armor mask and weapon geometry");
    ok &= Expect(casterGeometry.triangles.size() > 540u, "caster procedural anatomy has hood runes cape ornaments and orbit geometry");
    ok &= Expect(skirmisherGeometry.triangles.size() > 500u, "skirmisher procedural anatomy has antlers tails silhouette fins and dual weapon geometry");
    ok &= Expect(bulwarkGeometry.triangles.size() > 540u, "bulwark procedural anatomy has shield armor skull mask and bone geometry");
    ok &= Expect(bossGeometry.triangles.size() > 640u, "boss procedural anatomy has crown mantle mask weapon and corrupted silhouette geometry");
    ok &= Expect(materialCount(playerGeometry) >= 3, "player anatomy uses body accent and elemental procedural material channels");
    ok &= Expect(materialCount(bossGeometry) >= 3, "boss anatomy uses multiple procedural material channels");
    ok &= Expect(hashFor(brute) != hashFor(caster), "brute and caster anatomy hashes differ");
    ok &= Expect(hashFor(caster) != hashFor(skirmisher), "caster and skirmisher anatomy hashes differ");
    ok &= Expect(hashFor(skirmisher) != hashFor(bulwark), "skirmisher and bulwark anatomy hashes differ");
    ok &= Expect(hashFor(bulwark) != hashFor(boss), "bulwark and boss anatomy hashes differ");
    ok &= Expect(hashFor(brute) != hashFor(bruteVariant), "same enemy role uses packed procedural kit variants");
    ok &= Expect(hashFor(player) != hashFor(playerVariant), "player core uses packed procedural kit variants");
    return ok;
}

bool TestEnemyReadabilityRtProxies() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    KeepOnlyRoomSpawn(world, 1, world.rooms[1].center, 3);
    rogue::CombatSim sim;
    sim.Reset(world);
    world.rooms[1].locked = false;
    world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim.ActivateEnemiesInRoom(1);

    const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
    bool ok = true;
    ok &= Expect(enemyIndex >= 0, "readability proxy test finds a shielded enemy");
    if (enemyIndex < 0) {
        return ok;
    }

    sim.DamageEnemiesInRoom(1, 20.0f);
    CastGlovesElementAtEnemy(sim, world, enemyIndex, rogue::Element::Water);

    const auto proxies = rogue::BuildEntityProxies(sim);
    const auto geometryA = rogue::GenerateRTGeometry(proxies);
    const auto geometryB = rogue::GenerateRTGeometry(proxies);
    const rogue::EntityRTProxy* healthBack = FindProxyKind(proxies, rogue::EntityProxyKind::EnemyHealthBack);
    const rogue::EntityRTProxy* healthFill = FindProxyKind(proxies, rogue::EntityProxyKind::EnemyHealthFill);

    ok &= Expect(HasProxyKind(proxies, rogue::EntityProxyKind::EnemyBulwark), "readability proxy test keeps enemy silhouette");
    ok &= Expect(healthBack != nullptr, "enemy readability emits HP back strip");
    ok &= Expect(healthFill != nullptr, "enemy readability emits HP fill strip");
    ok &= Expect(healthBack == nullptr || healthFill == nullptr || healthFill->radius < healthBack->radius, "damaged enemy HP fill is shorter than back strip");
    ok &= Expect(HasProxyKind(proxies, rogue::EntityProxyKind::EnemyShieldFill), "shielded enemy emits shield strip");
    ok &= Expect(CountProxyKind(proxies, rogue::EntityProxyKind::EnemyStatusPip) >= 1, "statused enemy emits status pips");
    ok &= Expect(!geometryA.triangles.empty(), "enemy readability proxies emit geometry");
    ok &= Expect(rogue::HashPackedRTGeometry(rogue::PackRTGeometry(geometryA)) == rogue::HashPackedRTGeometry(rogue::PackRTGeometry(geometryB)), "enemy readability geometry is deterministic");
    return ok;
}

bool TestEnemyThreatTellRtProxies() {
    bool ok = true;
    auto runRoomRole = [](
                           int archetype,
                           float distance,
                           rogue::EntityProxyKind expectedTell,
                           rogue::WeaponId weapon,
                           rogue::Element element,
                           rogue::AttackShape shape,
                           const char* findMessage,
                           const char* placeMessage,
                           const char* tellMessage) {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        KeepOnlyRoomSpawn(world, 1, world.rooms[1].center, archetype);
        rogue::CombatSim sim;
        sim.Reset(world);
        world.rooms[1].locked = false;
        world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
        sim.ActivateEnemiesInRoom(1);

        bool roleOk = true;
        const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
        roleOk &= Expect(enemyIndex >= 0, findMessage);
        if (enemyIndex < 0) {
            return roleOk;
        }
        roleOk &= Expect(PlacePlayerAtEnemyRange(sim, world, enemyIndex, distance), placeMessage);
        if (!roleOk) {
            return roleOk;
        }

        const rogue::EnemyAttackIntent intent = rogue::EnemyAttackIntentFor(sim.Enemies()[enemyIndex], sim.Player());
        const auto proxies = rogue::BuildEntityProxies(sim);
        const auto geometryA = rogue::GenerateRTGeometry(proxies);
        const auto geometryB = rogue::GenerateRTGeometry(proxies);

        roleOk &= Expect(intent.active, "enemy tell intent is active");
        roleOk &= Expect(intent.inRange, "enemy tell intent is in range");
        roleOk &= Expect(intent.readiness > 0.98f, "enemy tell intent is ready before attack");
        roleOk &= Expect(intent.weapon == weapon, "enemy tell intent uses gameplay weapon");
        roleOk &= Expect(intent.element == element, "enemy tell intent uses gameplay element");
        roleOk &= Expect(intent.shape == shape, "enemy tell intent uses gameplay action shape");
        roleOk &= Expect(HasProxyKind(proxies, expectedTell), tellMessage);
        roleOk &= Expect(!geometryA.triangles.empty(), "enemy tell emits RT geometry");
        roleOk &= Expect(
            rogue::HashPackedRTGeometry(rogue::PackRTGeometry(geometryA)) ==
                rogue::HashPackedRTGeometry(rogue::PackRTGeometry(geometryB)),
            "enemy tell geometry is deterministic");
        return roleOk;
    };

    ok &= runRoomRole(0, 1.20f, rogue::EntityProxyKind::EnemyTellRing, rogue::WeaponId::Hammer, rogue::Element::Stone, rogue::AttackShape::Circle, "tell test finds brute", "tell test places player by brute", "brute emits ring tell");
    ok &= runRoomRole(1, 5.00f, rogue::EntityProxyKind::EnemyTellLine, rogue::WeaponId::Staff, rogue::Element::Electricity, rogue::AttackShape::Projectile, "tell test finds caster", "tell test places player by caster", "caster emits line tell");
    ok &= runRoomRole(2, 3.20f, rogue::EntityProxyKind::EnemyTellLine, rogue::WeaponId::Katana, rogue::Element::Air, rogue::AttackShape::Wave, "tell test finds skirmisher", "tell test places player by skirmisher", "skirmisher emits line tell");
    ok &= runRoomRole(3, 2.30f, rogue::EntityProxyKind::EnemyTellCone, rogue::WeaponId::Shotgun, rogue::Element::Ice, rogue::AttackShape::Cone, "tell test finds bulwark", "tell test places player by bulwark", "bulwark emits cone tell");

    {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        KeepOnlyFinalBossSpawn(world);
        rogue::CombatSim sim;
        sim.Reset(world);
        const int finalRoom = world.roomCount - 1;
        world.rooms[finalRoom].locked = false;
        world.rooms[finalRoom].lifecycle = rogue::RoomLifecycle::Active;
        sim.ActivateEnemiesInRoom(finalRoom);
        const int bossIndex = FindActiveEnemyKind(sim, rogue::EnemyKind::Boss);
        ok &= Expect(bossIndex >= 0, "tell test finds boss");
        if (bossIndex >= 0) {
            ok &= Expect(PlacePlayerAtEnemyRange(sim, world, bossIndex, 5.20f), "tell test places player by boss");
            const rogue::EnemyAttackIntent intent = rogue::EnemyAttackIntentFor(sim.Enemies()[bossIndex], sim.Player());
            const auto proxies = rogue::BuildEntityProxies(sim);
            ok &= Expect(intent.weapon == rogue::WeaponId::Scepter, "boss tell uses phase 1 scepter");
            ok &= Expect(intent.element == rogue::Element::Fire, "boss tell uses phase 1 fire");
            ok &= Expect(intent.shape == rogue::AttackShape::TargetArea, "boss tell uses target-area shape");
            ok &= Expect(HasProxyKind(proxies, rogue::EntityProxyKind::EnemyTellRing), "boss emits target-area ring tell");
        }
    }

    {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        KeepOnlyRoomSpawn(world, 1, world.rooms[1].center, 0);
        rogue::CombatSim sim;
        sim.Reset(world);
        world.rooms[1].locked = false;
        world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
        sim.ActivateEnemiesInRoom(1);
        const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
        ok &= Expect(enemyIndex >= 0, "tell range gate finds brute");
        if (enemyIndex >= 0) {
            ok &= Expect(PlacePlayerAtEnemyRange(sim, world, enemyIndex, 3.10f), "tell range gate places player outside melee range");
            const rogue::EnemyAttackIntent intent = rogue::EnemyAttackIntentFor(sim.Enemies()[enemyIndex], sim.Player());
            const auto proxies = rogue::BuildEntityProxies(sim);
            ok &= Expect(intent.active && !intent.inRange, "out-of-range enemy intent stays inactive for tell");
            ok &= Expect(CountEnemyTellProxies(proxies) == 0, "out-of-range enemy emits no tell proxy");
        }
    }

    {
        rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
        KeepOnlyRoomSpawn(world, 1, world.rooms[1].center, 0);
        rogue::CombatSim sim;
        sim.Reset(world);
        world.rooms[1].locked = false;
        world.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
        sim.ActivateEnemiesInRoom(1);
        const int enemyIndex = FirstLivingEnemyInRoom(sim, 1);
        ok &= Expect(enemyIndex >= 0, "tell cooldown gate finds brute");
        if (enemyIndex >= 0) {
            ok &= Expect(PlacePlayerAtEnemyRange(sim, world, enemyIndex, 1.20f), "tell cooldown gate places player in melee range");
            sim.Tick(rogue::InputState{}, 1.0f / 60.0f, world);
            const rogue::EnemyAttackIntent intent = rogue::EnemyAttackIntentFor(sim.Enemies()[enemyIndex], sim.Player());
            const auto proxies = rogue::BuildEntityProxies(sim);
            ok &= Expect(sim.Enemies()[enemyIndex].actionTimer > 0.0f, "recently fired hammer enemy enters active windup");
            ok &= Expect(intent.active && intent.readiness > 0.98f, "hammer windup keeps danger intent active");
            ok &= Expect(HasProxyKind(proxies, rogue::EntityProxyKind::EnemyTellRing), "hammer windup keeps ring tell visible until impact");
        }
    }

    return ok;
}

bool TestRunFailureStopsProgression() {
    rogue::GameSession session;
    session.Start(0x96u);
    session.TryEnterRoom(1);

    rogue::InputState idle{};
    for (int i = 0; i < 1800 && session.Status() == rogue::RunStatus::InProgress; ++i) {
        session.Tick(idle, 1.0f / 60.0f);
    }

    bool ok = true;
    ok &= Expect(session.Status() == rogue::RunStatus::Failed, "player death fails the run");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RunFailed), "run failed event is emitted");
    const uint32_t hashAfterFailure = session.EventHash();
    session.Tick(idle, 1.0f / 60.0f);
    ok &= Expect(session.EventHash() == hashAfterFailure, "failed run no longer advances events");
    return ok;
}

uint32_t RunScriptedEventHash() {
    rogue::GameSession session;
    session.Start(0x964bu);
    session.TryEnterRoom(1);
    session.DamageRoomEnemies(1, 9999.0f);
    ChooseReward(session, 0);
    session.TryEnterRoom(2);
    for (int i = 0; i < 90 && session.Status() == rogue::RunStatus::InProgress; ++i) {
        session.Tick(rogue::InputState{}, 1.0f / 60.0f);
        if (session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Completed) {
            break;
        }
    }
    ChooseReward(session, 1);
    session.TryEnterRoom(3);
    session.DamageRoomEnemies(3, 9999.0f);
    ChooseReward(session, 2);
    return session.EventHash();
}

bool TestScriptedEventHashDeterminism() {
    const uint32_t a = RunScriptedEventHash();
    const uint32_t b = RunScriptedEventHash();
    return Expect(a == b, "scripted session event hash is deterministic");
}

bool TestPackedGeometryDeterminism() {
    const std::vector<rogue::EntityRTProxy> proxies{
        rogue::EntityRTProxy{rogue::EntityProxyKind::EnemyBrute, rogue::Vec3{1.0f, 0.0f, 2.0f}, rogue::Vec3{1.0f, 0.0f, 0.0f}, 0.75f, 2u},
        rogue::EntityRTProxy{rogue::EntityProxyKind::EnemyCaster, rogue::Vec3{-2.0f, 0.0f, 1.0f}, rogue::Vec3{1.0f, 0.0f, 0.0f}, 0.55f, 3u},
        rogue::EntityRTProxy{rogue::EntityProxyKind::Projectile, rogue::Vec3{0.0f, 0.3f, 0.0f}, rogue::Vec3{1.0f, 0.0f, 0.0f}, 0.30f, 4u},
    };

    const auto generatedA = rogue::GenerateRTGeometry(proxies);
    const auto generatedB = rogue::GenerateRTGeometry(proxies);
    const auto packedA = rogue::PackRTGeometry(generatedA);
    const auto packedB = rogue::PackRTGeometry(generatedB);

    bool ok = true;
    ok &= Expect(!packedA.vertices.empty(), "packed geometry has vertices");
    ok &= Expect(!packedA.indices.empty(), "packed geometry has indices");
    ok &= Expect(packedA.indices.size() == generatedA.triangles.size() * 3u, "packed index count matches triangle count");
    ok &= Expect(packedA.vertices.size() == generatedA.triangles.size() * 3u, "packed vertex count matches triangle count");
    ok &= Expect(packedA.triangleMetadata.size() == generatedA.triangles.size(), "packed metadata count matches triangle count");
    ok &= Expect(packedA.triangleMetadata[0].materialId == generatedA.triangles[0].materialId, "triangle metadata stores material id");
    ok &= Expect(rogue::HashPackedRTGeometry(packedA) == rogue::HashPackedRTGeometry(packedB), "packed geometry hash is deterministic");
    return ok;
}

bool TestWorldRtGeometry() {
    const rogue::RoomGraph worldA = rogue::GenerateWorld(0x96u);
    const rogue::RoomGraph worldB = rogue::GenerateWorld(0x96u);
    const auto geometryA = rogue::GenerateWorldGeometry(worldA);
    const auto geometryB = rogue::GenerateWorldGeometry(worldB);
    const auto packedA = rogue::PackRTGeometry(geometryA);
    const auto packedB = rogue::PackRTGeometry(geometryB);

    bool ok = true;
    ok &= Expect(geometryA.triangles.size() > static_cast<std::size_t>(worldA.roomCount * 2), "world RT geometry contains room detail");
    ok &= Expect(packedA.triangleMetadata.size() == geometryA.triangles.size(), "world RT metadata tracks triangles");
    ok &= Expect(rogue::HashPackedRTGeometry(packedA) == rogue::HashPackedRTGeometry(packedB), "world RT geometry hash is deterministic");
    return ok;
}

float TriangleMaxY(const rogue::RtTriangle& tri) {
    return std::max({tri.a.position.y, tri.b.position.y, tri.c.position.y});
}

std::size_t TallTriangleCount(const rogue::GeneratedRTGeometry& geometry, float minY) {
    std::size_t count = 0;
    for (const rogue::RtTriangle& tri : geometry.triangles) {
        if (TriangleMaxY(tri) >= minY) {
            ++count;
        }
    }
    return count;
}

int MaterialDiversity(const rogue::GeneratedRTGeometry& geometry) {
    std::array<bool, 32> seen{};
    int count = 0;
    for (const rogue::RtTriangle& tri : geometry.triangles) {
        if (tri.materialId < seen.size() && !seen[tri.materialId]) {
            seen[tri.materialId] = true;
            ++count;
        }
    }
    return count;
}

bool TestProceduralPropGrammarGeometry() {
    const rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    const rogue::RoomVisualStyle sunlit = rogue::BuildVisualStyle(world, 0);
    const rogue::RoomVisualStyle library = rogue::BuildVisualStyle(world, 3);
    const rogue::RoomVisualStyle abyss = rogue::BuildVisualStyle(world, world.roomCount - 1);
    const rogue::RoomGraph deepWorld = rogue::GenerateWorld(0x96u, 5);
    const rogue::RoomVisualStyle deepStart = rogue::BuildVisualStyle(deepWorld, 0);

    const auto base = rogue::GenerateWorldGeometry(world);
    const auto sunlitA = rogue::GenerateWorldGeometry(world, &sunlit);
    const auto sunlitB = rogue::GenerateWorldGeometry(world, &sunlit);
    const auto libraryGeometry = rogue::GenerateWorldGeometry(world, &library);
    const auto abyssGeometry = rogue::GenerateWorldGeometry(world, &abyss);
    const auto deepGeometry = rogue::GenerateWorldGeometry(deepWorld, &deepStart);

    const auto packedSunlitA = rogue::PackRTGeometry(sunlitA);
    const auto packedSunlitB = rogue::PackRTGeometry(sunlitB);
    const auto packedLibrary = rogue::PackRTGeometry(libraryGeometry);
    const auto packedAbyss = rogue::PackRTGeometry(abyssGeometry);

    bool ok = true;
    ok &= Expect(sunlitA.triangles.size() > base.triangles.size() + static_cast<std::size_t>(world.roomCount * 48), "prop grammar adds substantial procedural geometry");
    const std::size_t baseTall = TallTriangleCount(base, 0.80f);
    ok &= Expect(TallTriangleCount(sunlitA, 0.80f) > baseTall, "sunlit prop grammar adds vertical columns and anchors");
    ok &= Expect(TallTriangleCount(libraryGeometry, 0.80f) > TallTriangleCount(base, 0.80f), "library prop grammar adds tall shelves and candles");
    ok &= Expect(TallTriangleCount(abyssGeometry, 1.10f) > 0u, "abyss prop grammar adds tall spires");
    ok &= Expect(TallTriangleCount(sunlitA, 1.45f) > 0u, "sunlit prop grammar adds overhead silhouettes");
    ok &= Expect(TallTriangleCount(libraryGeometry, 1.55f) > 0u, "library prop grammar adds high gothic silhouettes");
    ok &= Expect(TallTriangleCount(abyssGeometry, 1.65f) > 0u, "abyss prop grammar adds high overhead chains and ribs");
    ok &= Expect(MaterialDiversity(sunlitA) > MaterialDiversity(base), "prop grammar increases material variety");
    ok &= Expect(rogue::HashPackedRTGeometry(packedSunlitA) == rogue::HashPackedRTGeometry(packedSunlitB), "prop grammar geometry is deterministic");
    ok &= Expect(rogue::HashPackedRTGeometry(packedLibrary) != rogue::HashPackedRTGeometry(packedSunlitA), "library prop grammar changes geometry hash");
    ok &= Expect(rogue::HashPackedRTGeometry(packedAbyss) != rogue::HashPackedRTGeometry(packedLibrary), "abyss prop grammar changes geometry hash");
    ok &= Expect(deepStart.biome == rogue::VisualBiome::AbyssCrypt, "deep start prop grammar uses abyss biome");
    ok &= Expect(TallTriangleCount(deepGeometry, 1.10f) > 0u, "deep floors receive abyss vertical props");
    return ok;
}

bool TestVisualStyleLayerContract() {
    const rogue::RoomGraph worldA = rogue::GenerateWorld(0x96u);
    const rogue::RoomGraph worldB = rogue::GenerateWorld(0x96u);
    const rogue::RoomVisualStyle startA = rogue::BuildVisualStyle(worldA, 0);
    const rogue::RoomVisualStyle startB = rogue::BuildVisualStyle(worldB, 0);
    const rogue::RoomVisualStyle firstCombat = rogue::BuildVisualStyle(worldA, 1);
    const rogue::RoomVisualStyle control = rogue::BuildVisualStyle(worldA, 3);
    const rogue::RoomVisualStyle finalRoom = rogue::BuildVisualStyle(worldA, worldA.roomCount - 1);
    const rogue::VisualStylePacked packedStart = rogue::PackVisualStyle(startA);
    const rogue::VisualStylePacked packedFinal = rogue::PackVisualStyle(finalRoom);
    const auto baseGeometry = rogue::GenerateWorldGeometry(worldA);
    const auto styledGeometry = rogue::GenerateWorldGeometry(worldA, &startA);

    bool ok = true;
    ok &= Expect(rogue::VisualStyleHash(startA) == rogue::VisualStyleHash(startB), "visual style is deterministic for matching seed and room");
    ok &= Expect(packedStart.variant == rogue::VisualStyleHash(startA), "packed visual style carries deterministic variant hash");
    ok &= Expect(rogue::PackedVisualStyleBiome(packedStart) == rogue::VisualBiome::SunlitRuins, "start room uses sunlit ruins biome");
    ok &= Expect(firstCombat.biome != rogue::VisualBiome::AbyssCrypt, "early combat room avoids abyss biome before descent");
    ok &= Expect(control.biome == rogue::VisualBiome::ArcaneLibrary, "control objective maps to arcane library biome");
    ok &= Expect(rogue::PackedVisualStyleBiome(packedFinal) == rogue::VisualBiome::AbyssCrypt, "final room uses abyss crypt biome");
    ok &= Expect(rogue::PackedVisualStylePalette(packedStart) == startA.paletteId, "visual packing stores palette id");
    ok &= Expect(rogue::PackedVisualStyleFloorPattern(packedStart) == startA.floorPatternId, "visual packing stores floor pattern id");
    ok &= Expect(rogue::PackedVisualStylePropGrammar(packedFinal) == finalRoom.propGrammarId, "visual packing stores prop grammar id");
    ok &= Expect(rogue::PackedVisualStyleLightRig(packedFinal) == finalRoom.lightRigId, "visual packing stores light rig id");
    ok &= Expect(
        std::abs(rogue::VisualStylePackedWeight(packedStart.surface, rogue::kVisualStyleMossShift) - startA.moss) <= (1.0f / 15.0f),
        "visual packing round-trips moss weight within nibble precision");
    ok &= Expect(
        rogue::VisualStylePackedWeight(packedFinal.atmosphere, rogue::kVisualStyleCorruptionShift) > 0.60f,
        "final visual packing keeps high corruption readable");
    ok &= Expect(styledGeometry.triangles.size() > baseGeometry.triangles.size(), "styled world geometry adds procedural style detail");
    return ok;
}

bool TestReferenceArenaFramingContract() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);

    constexpr int roomIndex = 1;
    world.rooms[roomIndex].locked = false;
    world.rooms[roomIndex].lifecycle = rogue::RoomLifecycle::Active;
    const rogue::Vec2 playerOffset{2.10f, -1.35f};
    sim.PlacePlayer(world.rooms[roomIndex].center + playerOffset, roomIndex);

    const rogue::RenderScene scene = rogue::BuildRenderScene(world, sim, 1280, 720, 1.0f, 1u);
    const rogue::Room& room = world.rooms[roomIndex];
    const auto fullStyledGeometry = rogue::GenerateWorldGeometry(world, &scene.visualStyle);
    const float targetDx = std::abs(scene.camera.target.x - room.center.x);
    const float targetDz = std::abs(scene.camera.target.z - room.center.y);
    const float pullbackZ = scene.camera.target.z - scene.camera.position.z;
    const float sideOffset = scene.camera.target.x - scene.camera.position.x;

    bool ok = true;
    ok &= Expect(targetDx < room.halfSize.x * 0.18f, "reference camera targets active room instead of hard-locking to player x");
    ok &= Expect(targetDz < room.halfSize.y * 0.18f, "reference camera targets active room instead of hard-locking to player z");
    ok &= Expect(scene.camera.position.y >= 18.0f, "reference camera uses high isometric arena height");
    ok &= Expect(pullbackZ >= room.halfSize.y * 2.35f, "reference camera pulls back enough to show room edges");
    ok &= Expect(sideOffset >= room.halfSize.x * 0.32f, "reference camera keeps a diagonal isometric side offset");
    ok &= Expect(scene.camera.tiltRadians > 0.98f, "reference camera stores steeper tilt intent");
    ok &= Expect(scene.generatedGeometry.triangles.size() < fullStyledGeometry.triangles.size(), "render scene focuses RT arena geometry to the active room");
    ok &= Expect(scene.generatedGeometry.triangles.size() > 90u, "focused RT arena still keeps procedural room detail");
    return ok;
}

bool TestFloorDepthProgressionContract() {
    const rogue::RoomGraph floor0 = rogue::GenerateWorld(0x96u);
    const rogue::RoomGraph floor0Explicit = rogue::GenerateWorld(0x96u, 0);
    rogue::RoomGraph floor4 = rogue::GenerateWorld(0x96u, 4);
    const rogue::RoomVisualStyle start0 = rogue::BuildVisualStyle(floor0, 0);
    const rogue::RoomVisualStyle start4 = rogue::BuildVisualStyle(floor4, 0);
    const rogue::VisualStylePacked packed4 = rogue::PackVisualStyle(start4);

    rogue::RoomGraph combat0 = rogue::GenerateWorld(0x96u, 0);
    rogue::RoomGraph combat4 = rogue::GenerateWorld(0x96u, 4);
    KeepOnlyRoomSpawn(combat0, 1, combat0.rooms[1].center, 0);
    KeepOnlyRoomSpawn(combat4, 1, combat4.rooms[1].center, 0);
    rogue::CombatSim sim0;
    rogue::CombatSim sim4;
    sim0.Reset(combat0);
    sim4.Reset(combat4);
    combat0.rooms[1].locked = false;
    combat0.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    combat4.rooms[1].locked = false;
    combat4.rooms[1].lifecycle = rogue::RoomLifecycle::Active;
    sim0.ActivateEnemiesInRoom(1);
    sim4.ActivateEnemiesInRoom(1);
    const int enemy0 = FirstLivingEnemyInRoom(sim0, 1);
    const int enemy4 = FirstLivingEnemyInRoom(sim4, 1);

    rogue::CombatSim renderSim;
    renderSim.Reset(floor4);
    const rogue::RenderScene scene4 = rogue::BuildRenderScene(floor4, renderSim, 1280, 720, 2.0f, 21u);

    bool ok = true;
    ok &= Expect(floor0.floorIndex == 0 && floor0.descent == 0.0f, "default world starts at floor zero depth");
    ok &= Expect(rogue::HashWorld(floor0) == rogue::HashWorld(floor0Explicit), "explicit floor zero preserves default generation");
    ok &= Expect(floor4.floorIndex == 4, "deep world stores floor index");
    ok &= Expect(floor4.descent > 0.70f, "deep world exposes descent weight");
    ok &= Expect(rogue::HashWorld(floor0) != rogue::HashWorld(floor4), "deep floor generation changes deterministic world hash");
    ok &= Expect(start4.floorIndex == 4u, "deep visual style carries floor index");
    ok &= Expect(start4.descent > start0.descent, "deep visual style carries stronger descent");
    ok &= Expect(start4.corruption > start0.corruption, "deep visual style increases corruption");
    ok &= Expect(start4.fog > start0.fog, "deep visual style increases fog");
    ok &= Expect(rogue::PackedVisualStyleDescent(packed4) > 0.70f, "packed visual style stores descent");
    ok &= Expect(
        start4.biome == rogue::VisualBiome::ArcaneLibrary || start4.biome == rogue::VisualBiome::AbyssCrypt,
        "deep start room shifts away from friendly sunlit biome");
    ok &= Expect(enemy0 >= 0 && enemy4 >= 0, "floor depth combat scaling finds comparable enemies");
    if (enemy0 >= 0 && enemy4 >= 0) {
        ok &= Expect(sim4.Enemies()[enemy4].maxHp > sim0.Enemies()[enemy0].maxHp, "deep floor scales enemy hp upward");
    }
    ok &= Expect(scene4.overlay.overlayFloorIndex == 4u, "render overlay publishes floor index");
    ok &= Expect(scene4.overlay.overlayDescentPercent >= 70u, "render overlay publishes descent percent");
    ok &= Expect(rogue::PackedVisualStyleDescent(scene4.visualStylePacked) > 0.70f, "render scene frame carries packed descent");
    return ok;
}

bool TestRenderSceneContract() {
    rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);

    const rogue::RenderScene scene = rogue::BuildRenderScene(world, sim, 1280, 720, 1.25f, 7u);
    const uint32_t loadoutSlot0 = scene.overlay.overlayLoadoutSlot0;
    const uint32_t loadoutSlot1 = scene.overlay.overlayLoadoutSlot1;
    const uint32_t loadoutSlot2 = scene.overlay.overlayLoadoutSlot2;
    bool ok = true;
    ok &= Expect(scene.frame.outputWidth == 1280, "render scene stores output width");
    ok &= Expect(scene.frame.outputHeight == 720, "render scene stores output height");
    ok &= Expect(scene.frame.displayWidth == 1280, "render scene defaults display width to output width");
    ok &= Expect(scene.frame.displayHeight == 720, "render scene defaults display height to output height");
    ok &= Expect(scene.frame.frameIndex == 7u, "render scene stores frame index");
    ok &= Expect(scene.frame.visualStyleIdentity == scene.visualStylePacked.identity, "render scene frame stores visual identity");
    ok &= Expect(scene.frame.visualStyleSurface == scene.visualStylePacked.surface, "render scene frame stores visual surface weights");
    ok &= Expect(scene.frame.visualStyleAtmosphere == scene.visualStylePacked.atmosphere, "render scene frame stores visual atmosphere weights");
    ok &= Expect(scene.frame.visualStyleVariant == scene.visualStylePacked.variant, "render scene frame stores visual variant");
    ok &= Expect(scene.frame.renderQuality == 2u, "render scene defaults to rich DXR quality");
    ok &= Expect(scene.visualStyle.biome == rogue::VisualBiome::SunlitRuins, "render scene computes current room visual style");
    ok &= Expect(scene.overlay.overlayWeaponId == static_cast<uint32_t>(rogue::WeaponId::Katana), "render scene stores HUD weapon id");
    ok &= Expect(scene.overlay.overlayActiveSlot == 1u, "render scene stores HUD active slot");
    ok &= Expect(scene.overlay.overlayQReadyPercent == 100u, "render scene stores HUD Q readiness");
    ok &= Expect(scene.overlay.overlayEReadyPercent == 100u, "render scene stores HUD E readiness");
    ok &= Expect(scene.overlay.overlayQActionShape == static_cast<uint32_t>(rogue::AttackShape::Cone), "render scene stores Q action shape");
    ok &= Expect(scene.overlay.overlayEActionShape == static_cast<uint32_t>(rogue::AttackShape::Wave), "render scene stores E action shape");
    ok &= Expect(rogue::LoadoutOverlaySlotActive(loadoutSlot0), "render scene marks slot 1 active in loadout strip");
    ok &= Expect(!rogue::LoadoutOverlaySlotActive(loadoutSlot1), "render scene marks slot 2 inactive in loadout strip");
    ok &= Expect(!rogue::LoadoutOverlaySlotActive(loadoutSlot2), "render scene marks slot 3 inactive in loadout strip");
    ok &= Expect(rogue::LoadoutOverlaySlotWeapon(loadoutSlot0) == rogue::WeaponId::Katana, "loadout strip stores slot 1 weapon");
    ok &= Expect(rogue::LoadoutOverlaySlotWeapon(loadoutSlot1) == rogue::WeaponId::Pistol, "loadout strip stores slot 2 weapon");
    ok &= Expect(rogue::LoadoutOverlaySlotWeapon(loadoutSlot2) == rogue::WeaponId::Gloves, "loadout strip stores slot 3 weapon");
    ok &= Expect(rogue::LoadoutOverlaySlotElement(loadoutSlot0) == rogue::Element::Fire, "loadout strip stores slot 1 element");
    ok &= Expect(rogue::LoadoutOverlaySlotElement(loadoutSlot1) == rogue::Element::Electricity, "loadout strip stores slot 2 element");
    ok &= Expect(rogue::LoadoutOverlaySlotElement(loadoutSlot2) == rogue::Element::Ice, "loadout strip stores slot 3 element");
    ok &= Expect(rogue::LoadoutOverlaySlotQReady(loadoutSlot0) == 100u, "loadout strip stores slot 1 Q readiness");
    ok &= Expect(rogue::LoadoutOverlaySlotEReady(loadoutSlot0) == 100u, "loadout strip stores slot 1 E readiness");
    ok &= Expect(scene.overlay.overlayHp == 100u, "render scene stores HUD hp");
    ok &= Expect(scene.overlay.overlayCurrentRoom == 1u, "render scene stores HUD room index");
    ok &= Expect(scene.overlay.overlayRoomCount == static_cast<uint32_t>(world.roomCount), "render scene stores HUD room count");
    ok &= Expect(scene.overlay.overlayActiveEnemies == 0u, "render scene stores HUD active enemy count");
    ok &= Expect(scene.overlay.overlayRunStatus == static_cast<uint32_t>(rogue::RunStatus::InProgress), "render scene stores default run status");
    ok &= Expect(scene.overlay.overlayPlayerStatusMask == 0u, "render scene starts with empty player status mask");
    ok &= Expect(scene.overlay.overlayFloorIndex == 0u, "render scene stores floor index");
    ok &= Expect(scene.overlay.overlayDescentPercent == 0u, "render scene stores floor descent");
    ok &= Expect(scene.materialCount > 0, "render scene stores materials");
    ok &= Expect(scene.packedGeometry.triangleMetadata.size() == scene.generatedGeometry.triangles.size(), "render scene packs triangle metadata");
    ok &= Expect(scene.frame.invViewProj[0] != 1.0f || scene.frame.invViewProj[5] != 1.0f, "render scene stores a real camera inverse matrix");
    ok &= Expect(scene.geometryHash == rogue::HashPackedRTGeometry(scene.packedGeometry), "render scene geometry hash matches packed geometry");
    ok &= Expect(!HasProxyKind(scene.proxies, rogue::EntityProxyKind::PlayerPrimaryGuide), "render scene hides persistent Q guide proxy");
    ok &= Expect(!HasProxyKind(scene.proxies, rogue::EntityProxyKind::PlayerAbilityLine), "render scene hides persistent E guide proxy for starting weapon");
    ok &= Expect(HasProxyKind(scene.proxies, rogue::EntityProxyKind::PlayerBlade), "render scene keeps small facing indicator");

    rogue::InputState action{};
    action.action2 = true;
    sim.Tick(action, 1.0f / 60.0f, world);
    const rogue::RenderScene cooldownScene = rogue::BuildRenderScene(world, sim, 1280, 720, 1.33f, 8u);
    ok &= Expect(
        rogue::LoadoutOverlaySlotEReady(cooldownScene.overlay.overlayLoadoutSlot0) < 100u,
        "loadout strip stores non-ready E cooldown");

    sim.SetPlayerWeaponSlot(0, rogue::WeaponId::Spear, rogue::Element::Air);
    const rogue::RenderScene spearScene = rogue::BuildRenderScene(world, sim, 1280, 720, 1.50f, 9u);
    ok &= Expect(spearScene.overlay.overlayWeaponId == static_cast<uint32_t>(rogue::WeaponId::Spear), "render scene HUD follows changed weapon");
    ok &= Expect(rogue::LoadoutOverlaySlotActive(spearScene.overlay.overlayLoadoutSlot0), "loadout strip keeps changed slot active");
    ok &= Expect(rogue::LoadoutOverlaySlotWeapon(spearScene.overlay.overlayLoadoutSlot0) == rogue::WeaponId::Spear, "loadout strip follows changed slot weapon");
    ok &= Expect(rogue::LoadoutOverlaySlotElement(spearScene.overlay.overlayLoadoutSlot0) == rogue::Element::Air, "loadout strip follows changed slot element");
    ok &= Expect(spearScene.overlay.overlayQActionShape == static_cast<uint32_t>(rogue::AttackShape::Cone), "render scene Q shape follows changed weapon");
    ok &= Expect(spearScene.overlay.overlayEActionShape == static_cast<uint32_t>(rogue::AttackShape::Dash), "render scene E shape follows changed weapon");

    const rogue::RenderScene clearScene = rogue::BuildRenderScene(
        world,
        sim,
        1280,
        720,
        1.75f,
        10u,
        std::span<const rogue::RenderVfxPulse>{},
        static_cast<uint32_t>(rogue::RunStatus::FloorComplete));
    ok &= Expect(clearScene.overlay.overlayRunStatus == static_cast<uint32_t>(rogue::RunStatus::FloorComplete), "render scene exposes floor-complete run status");

    const rogue::RenderScene scaledScene = rogue::BuildRenderScene(
        world,
        sim,
        960,
        540,
        1.80f,
        14u,
        std::span<const rogue::RenderVfxPulse>{},
        static_cast<uint32_t>(rogue::RunStatus::InProgress),
        3840,
        2160,
        1u);
    ok &= Expect(scaledScene.frame.outputWidth == 960u, "render scene stores scaled render width");
    ok &= Expect(scaledScene.frame.outputHeight == 540u, "render scene stores scaled render height");
    ok &= Expect(scaledScene.frame.displayWidth == 3840u, "render scene stores display width separately");
    ok &= Expect(scaledScene.frame.displayHeight == 2160u, "render scene stores display height separately");
    ok &= Expect(scaledScene.frame.renderQuality == 1u, "render scene stores explicit DXR quality tier");

    rogue::RoomGraph bossWorld = rogue::GenerateWorld(0x96u);
    KeepOnlyFinalBossSpawn(bossWorld);
    rogue::CombatSim bossSim;
    bossSim.Reset(bossWorld);
    const int finalRoom = bossWorld.roomCount - 1;
    bossWorld.rooms[finalRoom].locked = false;
    bossWorld.rooms[finalRoom].lifecycle = rogue::RoomLifecycle::Active;
    bossSim.ActivateEnemiesInRoom(finalRoom);
    bossSim.PlacePlayer(bossWorld.rooms[finalRoom].center + rogue::Vec2{-3.0f, 0.0f}, finalRoom);
    const rogue::RenderScene bossScene = rogue::BuildRenderScene(bossWorld, bossSim, 1280, 720, 2.0f, 11u);
    ok &= Expect(bossScene.overlay.overlayBossPhase == 1u, "render scene exposes boss phase 1 while boss is healthy");
    ok &= Expect(bossScene.overlay.overlayBossHpPercent == 100u, "render scene exposes full boss hp percent");

    bossSim.DamageEnemiesInRoom(finalRoom, 120.0f);
    const rogue::RenderScene bossPhase2Scene = rogue::BuildRenderScene(bossWorld, bossSim, 1280, 720, 2.1f, 12u);
    ok &= Expect(bossPhase2Scene.overlay.overlayBossPhase == 2u, "render scene exposes boss phase 2 after hp threshold");
    ok &= Expect(bossPhase2Scene.overlay.overlayBossHpPercent < 100u, "render scene updates boss hp percent after damage");

    bossSim.DamageEnemiesInRoom(finalRoom, 90.0f);
    const rogue::RenderScene bossPhase3Scene = rogue::BuildRenderScene(bossWorld, bossSim, 1280, 720, 2.2f, 13u);
    ok &= Expect(bossPhase3Scene.overlay.overlayBossPhase == 3u, "render scene exposes boss phase 3 after low hp threshold");
    return ok;
}

bool TestRenderSceneVfxContract() {
    const rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);

    const std::array<rogue::RenderVfxPulse, 7> pulses{
        rogue::RenderVfxPulse{rogue::RenderVfxKind::HitSpark, rogue::Vec3{1.0f, 0.55f, 1.0f}, 0.45f, 0.20f, 0.30f, 1.0f},
        rogue::RenderVfxPulse{rogue::RenderVfxKind::RoomClearPulse, rogue::Vec3{0.0f, 0.10f, 0.0f}, 3.80f, 0.70f, 0.95f, 1.0f},
        rogue::RenderVfxPulse{rogue::RenderVfxKind::PortalPulse, rogue::Vec3{3.0f, 0.12f, 0.0f}, 1.65f, 0.80f, 1.10f, 1.1f},
        rogue::RenderVfxPulse{rogue::RenderVfxKind::WeaponCone, rogue::Vec3{1.0f, 0.12f, 2.0f}, 3.20f, 0.28f, 0.28f, 1.0f, rogue::Vec3{1.0f, 0.0f, 0.0f}},
        rogue::RenderVfxPulse{rogue::RenderVfxKind::WeaponLine, rogue::Vec3{2.0f, 0.12f, 2.0f}, 4.40f, 0.24f, 0.24f, 1.0f, rogue::Vec3{0.0f, 0.0f, 1.0f}},
        rogue::RenderVfxPulse{rogue::RenderVfxKind::WeaponRing, rogue::Vec3{3.0f, 0.12f, 2.0f}, 2.35f, 0.34f, 0.34f, 1.0f},
        rogue::RenderVfxPulse{rogue::RenderVfxKind::WeaponBurst, rogue::Vec3{4.0f, 0.12f, 2.0f}, 3.10f, 0.30f, 0.30f, 1.0f, rogue::Vec3{1.0f, 0.0f, 0.0f}}
    };

    const auto baseProxies = rogue::BuildEntityProxies(sim);
    const rogue::RenderScene sceneA = rogue::BuildRenderScene(
        world,
        sim,
        1280,
        720,
        1.25f,
        7u,
        std::span<const rogue::RenderVfxPulse>(pulses.data(), pulses.size()));
    const rogue::RenderScene sceneB = rogue::BuildRenderScene(
        world,
        sim,
        1280,
        720,
        1.25f,
        7u,
        std::span<const rogue::RenderVfxPulse>(pulses.data(), pulses.size()));

    bool ok = true;
    ok &= Expect(sceneA.proxies.size() > baseProxies.size(), "render scene includes transient VFX proxies");
    ok &= Expect(HasProxyKind(sceneA.proxies, rogue::EntityProxyKind::PlayerActionCone), "VFX scene includes weapon cone footprint");
    ok &= Expect(HasProxyKind(sceneA.proxies, rogue::EntityProxyKind::PlayerActionLine), "VFX scene includes weapon line footprint");
    ok &= Expect(!HasProxyKind(sceneA.proxies, rogue::EntityProxyKind::PlayerActionRing), "VFX scene avoids legacy weapon ring footprint");
    ok &= Expect(HasProxyKind(sceneA.proxies, rogue::EntityProxyKind::PlayerActionBurst), "VFX scene includes weapon burst footprint");
    ok &= Expect(sceneA.materialCount == 15u, "render scene exposes all DXR materials including enemy variety boss and VFX");
    ok &= Expect(sceneA.packedGeometry.triangleMetadata.size() == sceneA.generatedGeometry.triangles.size(), "VFX render scene packs triangle metadata");
    ok &= Expect(sceneA.geometryHash == sceneB.geometryHash, "VFX render scene hash is deterministic");
    return ok;
}

}

int main() {
    bool ok = true;
    auto run = [&ok](const char* name, bool (*test)()) {
        std::printf("running %s\n", name);
        std::fflush(stdout);
        const bool passed = test();
        ok &= passed;
        std::printf("finished %s: %s\n", name, passed ? "passed" : "failed");
        std::fflush(stdout);
    };

    run("world determinism", TestWorldDeterminism);
    run("world enemy archetypes and pressure", TestWorldEnemyArchetypeDiversityAndPressure);
    run("portal traversal predicates", TestPortalTraversalPredicates);
    run("portal traversal by movement", TestPortalTraversalByMovement);
    run("closed portal blocks movement", TestClosedPortalBlocksMovement);
    run("room lifecycle and activation", TestRoomLifecycleAndActivation);
    run("kill-all objective", TestKillAllObjectiveCompletion);
    run("reward choice contract", TestRewardChoiceContract);
    run("reward selection recovery reset beat", TestRewardSelectionRecoveryResetBeat);
    run("reward choice loadout diversity", TestRewardChoiceLoadoutDiversity);
    run("player upgrade gameplay scalars", TestPlayerUpgradeGameplayScalars);
    run("input action bindings", TestInputActionBindings);
    run("input navigation bindings", TestInputNavigationBindingsAreNotInverted);
    run("weapon roster contract", TestWeaponRosterContract);
    run("combat action timing contract", TestCombatActionTimingContract);
    run("weapon action visual language contract", TestWeaponActionVisualLanguageContract);
    run("enemy action visual language contract", TestEnemyActionVisualLanguageContract);
    run("active weapon slot actions", TestActiveWeaponSlotActions);
    run("full weapon roster runtime actions", TestFullWeaponRosterRuntimeActions);
    run("special action shape gameplay", TestSpecialActionShapeGameplaySemantics);
    run("reward choice keyboard binding", TestRewardChoiceKeyboardBinding);
    run("player ability feedback contract", TestPlayerAbilityFeedbackContract);
    run("objective overlay contract", TestObjectiveOverlayContract);
    run("game session player action events", TestGameSessionPlayerActionEvents);
    run("enemy action event faction", TestEnemyActionEventFaction);
    run("wet fire reaction pipeline", TestWetFireReactionThroughWeaponPipeline);
    run("water ice multi-status pipeline", TestWaterIceKeepsMultiStatusSnapshotPipeline);
    run("chilled water boosted slow pipeline", TestChilledWaterBoostsSlowAndKeepsWet);
    run("charged water status lock pipeline", TestChargedWaterCreatesStatusLockAndClearsCatalyst);
    run("burning water clears statuses pipeline", TestBurningWaterClearsBothStatuses);
    run("burning electricity micro explosion pipeline", TestBurningElectricityCreatesAreaMicroExplosion);
    run("player micro explosion shared pipeline", TestPlayerMicroExplosionUsesSharedAreaPipeline);
    run("wet air transfer pipeline", TestWetAirTransfersWetToNearbyNonHitTargets);
    run("charged chilled air reaction pipeline", TestAirReactsWithChargedAndChilledWithoutClearingCatalysts);
    run("stone shield bypass and chilled strip", TestStoneShieldBypassAndChilledStoneStrip);
    run("enemy weapons apply player statuses", TestEnemyWeaponsApplyPlayerStatusesThroughSamePipeline);
    run("player status overlay contract", TestPlayerStatusOverlayContract);
    run("enemy pressure roles use distinct actions", TestEnemyPressureRolesUseDistinctActions);
    run("boss final room phase contract", TestBossFinalRoomPhaseContract);
    run("charged discharge lane uniqueness", TestChargedDischargeHitsLaneAndDoesNotRepeatSameInstance);
    run("charged refresh resets discharge uniqueness", TestChargedRefreshResetsDischargeUniqueness);
    run("enemy damaged event", TestEnemyDamagedEvent);
    run("player damage feedback contract", TestPlayerDamageFeedbackContract);
    run("survival objective", TestSurviveTimerObjectiveCompletion);
    run("control point objective", TestControlPointObjectiveCompletion);
    run("full floor route", TestFullFloorRouteProgressesAfterEveryReward);
    run("advance floor carries build progress", TestAdvanceFloorCarriesBuildProgress);
    run("playable first room real combat smoke", TestPlayableFirstRoomRealCombatSmoke);
    run("playable early run reward survival smoke", TestPlayableEarlyRunRewardSurvivalSmoke);
    run("playable control room real input smoke", TestPlayableControlRoomRealInputSmoke);
    run("playable full run boss smoke", TestPlayableFullRunBossSmoke);
    run("combat damage and rt proxy", TestCombatDamageAndRtProxy);
    run("enemy variety rt proxies", TestEnemyVarietyRtProxies);
    run("procedural character anatomy geometry", TestProceduralCharacterAnatomyGeometry);
    run("enemy readability rt proxies", TestEnemyReadabilityRtProxies);
    run("enemy threat tell rt proxies", TestEnemyThreatTellRtProxies);
    run("run failure", TestRunFailureStopsProgression);
    run("scripted event hash", TestScriptedEventHashDeterminism);
    run("packed geometry determinism", TestPackedGeometryDeterminism);
    run("world rt geometry", TestWorldRtGeometry);
    run("procedural prop grammar geometry", TestProceduralPropGrammarGeometry);
    run("visual style layer contract", TestVisualStyleLayerContract);
    run("reference arena framing contract", TestReferenceArenaFramingContract);
    run("floor depth progression contract", TestFloorDepthProgressionContract);
    run("render scene contract", TestRenderSceneContract);
    run("render scene vfx contract", TestRenderSceneVfxContract);

    if (!ok) {
        return 1;
    }

    std::printf("rogue96 core tests passed\n");
    return 0;
}
