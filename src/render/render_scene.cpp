#include "render/render_scene.h"

#include <DirectXMath.h>

namespace rogue {

namespace {

void StoreMatrix(float (&out)[16], DirectX::FXMMATRIX matrix) {
    DirectX::XMFLOAT4X4 stored{};
    DirectX::XMStoreFloat4x4(&stored, matrix);
    const float* values = &stored._11;
    for (int i = 0; i < 16; ++i) {
        out[i] = values[i];
    }
}

void UpdateCamera(RenderScene& scene, const CombatSim& combat, uint32_t outputWidth, uint32_t outputHeight) {
    scene.camera.target = Vec3{combat.Player().position.x, 0.0f, combat.Player().position.y};
    scene.camera.position = Vec3{
        scene.camera.target.x,
        16.0f,
        scene.camera.target.z - 12.5f
    };
    scene.camera.tiltRadians = 0.92f;

    const float aspect = outputHeight > 0
        ? static_cast<float>(outputWidth) / static_cast<float>(outputHeight)
        : 16.0f / 9.0f;

    const DirectX::XMVECTOR eye = DirectX::XMVectorSet(scene.camera.position.x, scene.camera.position.y, scene.camera.position.z, 1.0f);
    const DirectX::XMVECTOR target = DirectX::XMVectorSet(scene.camera.target.x, scene.camera.target.y, scene.camera.target.z, 1.0f);
    const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, target, up);
    const DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(0.82f, aspect, 0.05f, 120.0f);
    const DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(nullptr, DirectX::XMMatrixMultiply(view, proj));
    StoreMatrix(scene.frame.invViewProj, invViewProj);
}

std::array<EntityMaterial, kMaxDxrMaterials> DefaultMaterials() {
    std::array<EntityMaterial, kMaxDxrMaterials> materials{};
    materials[0] = EntityMaterial{Vec3{0.105f, 0.090f, 0.095f}, 0.00f};
    materials[1] = EntityMaterial{Vec3{0.30f, 0.17f, 0.18f}, 0.03f};
    materials[2] = EntityMaterial{Vec3{0.15f, 0.82f, 0.92f}, 0.42f};
    materials[3] = EntityMaterial{Vec3{1.00f, 0.50f, 0.16f}, 0.55f};
    materials[4] = EntityMaterial{Vec3{0.72f, 0.08f, 0.05f}, 0.10f};
    materials[5] = EntityMaterial{Vec3{0.38f, 0.12f, 0.78f}, 0.20f};
    materials[6] = EntityMaterial{Vec3{0.20f, 0.56f, 1.00f}, 0.65f};
    materials[7] = EntityMaterial{Vec3{0.08f, 0.95f, 0.68f}, 0.75f};
    materials[8] = EntityMaterial{Vec3{0.94f, 0.20f, 0.62f}, 0.58f};
    materials[9] = EntityMaterial{Vec3{0.14f, 0.115f, 0.12f}, 0.04f};
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

    UpdateCamera(scene, combat, outputWidth, outputHeight);
    scene.frame.cameraPosition[0] = scene.camera.position.x;
    scene.frame.cameraPosition[1] = scene.camera.position.y;
    scene.frame.cameraPosition[2] = scene.camera.position.z;
    scene.frame.timeSeconds = timeSeconds;
    scene.frame.frameIndex = frameIndex;
    scene.frame.materialCount = 10;
    scene.frame.outputWidth = outputWidth;
    scene.frame.outputHeight = outputHeight;

    scene.proxies = BuildEntityProxies(combat);
    scene.generatedGeometry = GenerateWorldGeometry(world);
    GeneratedRTGeometry entityGeometry = GenerateRTGeometry(scene.proxies);
    scene.generatedGeometry.triangles.insert(
        scene.generatedGeometry.triangles.end(),
        entityGeometry.triangles.begin(),
        entityGeometry.triangles.end());
    scene.packedGeometry = PackRTGeometry(scene.generatedGeometry);
    scene.geometryHash = HashPackedRTGeometry(scene.packedGeometry);
    scene.materials = DefaultMaterials();
    scene.materialCount = scene.frame.materialCount;
    return scene;
}

}

