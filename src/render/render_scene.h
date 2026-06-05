#pragma once

#include "game/combat_sim.h"
#include "game/math.h"
#include "game/world_gen.h"
#include "render/entity_rt_proxy.h"

#include <array>
#include <cstdint>
#include <vector>

namespace rogue {

constexpr uint32_t kMaxDxrMaterials = 8;

struct CameraParams {
    Vec3 position{0.0f, 18.0f, -14.0f};
    Vec3 target{0.0f, 0.0f, 0.0f};
    float tiltRadians = 0.92f;
};

struct DxrFrameConstants {
    float invViewProj[16]{};
    float cameraPosition[3]{};
    float timeSeconds = 0.0f;
    uint32_t frameIndex = 0;
    uint32_t materialCount = 0;
    uint32_t outputWidth = 0;
    uint32_t outputHeight = 0;
};

struct EntityMaterial {
    Vec3 baseColor{};
    float emission = 0.0f;
};

struct RenderScene {
    CameraParams camera{};
    DxrFrameConstants frame{};
    RoomGraph world{};
    std::vector<EntityRTProxy> proxies;
    GeneratedRTGeometry generatedGeometry;
    PackedRTGeometry packedGeometry;
    std::array<EntityMaterial, kMaxDxrMaterials> materials{};
    uint32_t materialCount = 0;
    uint32_t geometryHash = 0;
};

RenderScene BuildRenderScene(
    const RoomGraph& world,
    const CombatSim& combat,
    uint32_t outputWidth,
    uint32_t outputHeight,
    float timeSeconds,
    uint32_t frameIndex);

}

