#include "game/combat_sim.h"
#include "game/game_session.h"
#include "game/world_gen.h"
#include "render/entity_rt_proxy.h"
#include "render/render_scene.h"

#include <cstdio>
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
    ok &= Expect(a.rooms[3].objective.kind == rogue::RoomObjectiveKind::CustomPlaceholder, "room 3 uses custom placeholder objective");
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
    ok &= Expect(session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Available, "completed room unlocks next room");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::EnemyKilled), "enemy killed event is emitted");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::ObjectiveCompleted), "objective completed event is emitted");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::RoomCompleted), "room completed event is emitted");
    ok &= Expect(HasEvent(session.LastTick(), rogue::GameEventType::PortalOpened), "portal opened event is emitted");
    return ok;
}

bool TestSurviveTimerObjectiveCompletion() {
    rogue::GameSession session;
    session.Start(0x96u);
    session.TryEnterRoom(1);
    session.DamageRoomEnemies(1, 9999.0f);

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
    ok &= Expect(session.World().rooms[3].lifecycle == rogue::RoomLifecycle::Available, "survival completion unlocks next room");
    ok &= Expect(sawCompletion, "survival objective emits completion event");
    return ok;
}

bool TestCustomPlaceholderObjectiveCompletion() {
    rogue::GameSession session;
    session.Start(0x96u);
    session.TryEnterRoom(1);
    session.DamageRoomEnemies(1, 9999.0f);
    session.TryEnterRoom(2);
    for (int i = 0; i < 90 && session.Status() == rogue::RunStatus::InProgress; ++i) {
        session.Tick(rogue::InputState{}, 1.0f / 60.0f);
        if (session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Completed) {
            break;
        }
    }

    bool ok = true;
    ok &= Expect(session.TryEnterRoom(3), "custom placeholder room can be entered");
    ok &= Expect(session.World().rooms[3].objective.kind == rogue::RoomObjectiveKind::CustomPlaceholder, "room 3 is custom placeholder");
    session.DamageRoomEnemies(3, 9999.0f);
    ok &= Expect(session.World().rooms[3].lifecycle == rogue::RoomLifecycle::Completed, "custom placeholder currently completes like kill-all");
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
    session.TryEnterRoom(2);
    for (int i = 0; i < 90 && session.Status() == rogue::RunStatus::InProgress; ++i) {
        session.Tick(rogue::InputState{}, 1.0f / 60.0f);
        if (session.World().rooms[2].lifecycle == rogue::RoomLifecycle::Completed) {
            break;
        }
    }
    session.TryEnterRoom(3);
    session.DamageRoomEnemies(3, 9999.0f);
    return session.EventHash();
}

bool TestScriptedEventHashDeterminism() {
    const uint32_t a = RunScriptedEventHash();
    const uint32_t b = RunScriptedEventHash();
    return Expect(a == b, "scripted session event hash is deterministic");
}

bool TestPackedGeometryDeterminism() {
    const std::vector<rogue::EntityRTProxy> proxies{
        rogue::EntityRTProxy{rogue::EntityProxyKind::EnemyBrute, rogue::Vec3{1.0f, 0.0f, 2.0f}, 0.75f, 2u},
        rogue::EntityRTProxy{rogue::EntityProxyKind::EnemyCaster, rogue::Vec3{-2.0f, 0.0f, 1.0f}, 0.55f, 3u},
        rogue::EntityRTProxy{rogue::EntityProxyKind::Projectile, rogue::Vec3{0.0f, 0.3f, 0.0f}, 0.30f, 4u},
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
    ok &= Expect(rogue::HashPackedRTGeometry(packedA) == rogue::HashPackedRTGeometry(packedB), "packed geometry hash is deterministic");
    return ok;
}

bool TestRenderSceneContract() {
    const rogue::RoomGraph world = rogue::GenerateWorld(0x96u);
    rogue::CombatSim sim;
    sim.Reset(world);

    const rogue::RenderScene scene = rogue::BuildRenderScene(world, sim, 1280, 720, 1.25f, 7u);
    bool ok = true;
    ok &= Expect(scene.frame.outputWidth == 1280, "render scene stores output width");
    ok &= Expect(scene.frame.outputHeight == 720, "render scene stores output height");
    ok &= Expect(scene.frame.frameIndex == 7u, "render scene stores frame index");
    ok &= Expect(scene.materialCount > 0, "render scene stores materials");
    ok &= Expect(scene.geometryHash == rogue::HashPackedRTGeometry(scene.packedGeometry), "render scene geometry hash matches packed geometry");
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
    run("room lifecycle and activation", TestRoomLifecycleAndActivation);
    run("kill-all objective", TestKillAllObjectiveCompletion);
    run("survival objective", TestSurviveTimerObjectiveCompletion);
    run("custom placeholder objective", TestCustomPlaceholderObjectiveCompletion);
    run("combat damage and rt proxy", TestCombatDamageAndRtProxy);
    run("run failure", TestRunFailureStopsProgression);
    run("scripted event hash", TestScriptedEventHashDeterminism);
    run("packed geometry determinism", TestPackedGeometryDeterminism);
    run("render scene contract", TestRenderSceneContract);

    if (!ok) {
        return 1;
    }

    std::printf("rogue96 core tests passed\n");
    return 0;
}
