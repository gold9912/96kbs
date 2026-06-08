#pragma once

#include "game/game_session.h"
#include "game/world_gen.h"
#include "platform/d3d12_context.h"
#include "render/dxr_scene_resources.h"
#include "render/dxr_pipeline.h"
#include "render/render_scene.h"

#include <array>
#include <cstdint>

namespace rogue {

class Application {
public:
    bool Initialize(HWND hwnd, const EngineConfig& config);
    void Shutdown();
    void Tick(const InputState& input, float dt);
    void Render();
    void UpdateWindowTitle(HWND hwnd, float elapsedSeconds);

private:
    static D3D12FrameCallbackResult RenderDxrFrame(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, void* userData);
    void UpdateVfxPulses(float dt);
    void CollectVfxPulses(const GameSessionTickResult& tick);
    void AddVfxPulse(
        RenderVfxKind kind,
        Vec3 position,
        float radius,
        float duration,
        float intensity = 1.0f,
        Vec3 direction = Vec3{1.0f, 0.0f, 0.0f});
    void DrawOverlay();

    HWND hwnd_ = nullptr;
    EngineConfig config_{};
    GameSession session_{};
    D3D12Context d3d_{};
    DxrPipeline dxr_{};
    DxrSceneResources dxrSceneResources_{};
    RenderScene renderScene_{};
    std::array<RenderVfxPulse, kMaxRenderVfxPulses> vfxPulses_{};
    float timeSeconds_ = 0.0f;
    float titleAccumulator_ = 0.0f;
    float reactionFeedbackTtl_ = 0.0f;
    float playerDamageFeedbackTtl_ = 0.0f;
    float rewardAppliedFeedbackTtl_ = 0.0f;
    uint32_t reactionFeedbackKind_ = 0;
    uint32_t playerDamageElement_ = static_cast<uint32_t>(Element::None);
    uint32_t rewardAppliedPacked_ = 0;
    uint32_t frameIndex_ = 0;
};

}
