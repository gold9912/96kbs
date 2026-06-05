#include "render/render_scene.h"

namespace rogue {

namespace {

void SetIdentity(float (&m)[16]) {
    for (float& v : m) {
        v = 0.0f;
    }
    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

std::array<EntityMaterial, kMaxDxrMaterials> DefaultMaterials() {
    std::array<EntityMaterial, kMaxDxrMaterials> materials{};
    materials[0] = EntityMaterial{Vec3{0.10f, 0.08f, 0.09f}, 0.00f};
    materials[1] = EntityMaterial{Vec3{0.95f, 0.42f, 0.15f}, 0.35f};
    materials[2] = EntityMaterial{Vec3{0.62f, 0.06f, 0.04f}, 0.08f};
    materials[3] = EntityMaterial{Vec3{0.22f, 0.10f, 0.52f}, 0.16f};
    materials[4] = EntityMaterial{Vec3{0.25f, 0.58f, 0.95f}, 0.45f};
    materials[5] = EntityMaterial{Vec3{0.75f, 0.11f, 0.45f}, 0.30f};
    return materials;
}

}

RenderScene BuildRenderScene(
    const RoomGraph& world,
    const CombatSim& combat,
    uint32_t outputWidth,
    uint32_t outputHeight,
    float timeSeconds,
    uint32_t frameIndex) {
    RenderScene scene{};
    scene.world = world;
    scene.camera.target = Vec3{combat.Player().position.x, 0.0f, combat.Player().position.y};
    scene.camera.position = Vec3{
        scene.camera.target.x,
        18.0f,
        scene.camera.target.z - 14.0f
    };

    SetIdentity(scene.frame.invViewProj);
    scene.frame.cameraPosition[0] = scene.camera.position.x;
    scene.frame.cameraPosition[1] = scene.camera.position.y;
    scene.frame.cameraPosition[2] = scene.camera.position.z;
    scene.frame.timeSeconds = timeSeconds;
    scene.frame.frameIndex = frameIndex;
    scene.frame.materialCount = 6;
    scene.frame.outputWidth = outputWidth;
    scene.frame.outputHeight = outputHeight;

    scene.proxies = BuildEntityProxies(combat);
    scene.generatedGeometry = GenerateRTGeometry(scene.proxies);
    scene.packedGeometry = PackRTGeometry(scene.generatedGeometry);
    scene.geometryHash = HashPackedRTGeometry(scene.packedGeometry);
    scene.materials = DefaultMaterials();
    scene.materialCount = scene.frame.materialCount;
    return scene;
}

}

