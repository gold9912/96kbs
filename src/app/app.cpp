#include "app/app.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <span>

namespace rogue {

namespace {

constexpr int kRewardChoicePipCapacity = (kRewardChoiceCount * (kRewardChoiceCount + 1)) / 2;
constexpr int kFrameVfxPulseCapacity = kMaxRenderVfxPulses + kRewardChoiceCount + kRewardChoicePipCapacity;
constexpr float kReactionFeedbackDuration = 1.15f;
constexpr float kPlayerDamageFeedbackDuration = 0.72f;
constexpr float kRewardAppliedFeedbackDuration = 1.65f;

uint32_t ScaledRenderDimension(uint32_t displayDimension, uint32_t renderScalePercent) {
    const uint32_t scale = std::max(25u, std::min(renderScalePercent, 100u));
    return std::max(1u, (displayDimension * scale + 50u) / 100u);
}

Vec3 MakePulsePosition(Vec2 p, float y) {
    return Vec3{p.x, y, p.y};
}

Vec2 RewardIconPosition(const PlayerState& player, int index) {
    const Vec2 facing = NormalizeOr(player.facing, Vec2{1.0f, 0.0f});
    const Vec2 right{-facing.y, facing.x};
    const float lane = static_cast<float>(index - 1) * 1.18f;
    return player.position + facing * 2.35f + right * lane;
}

Vec2 RewardPipPosition(const PlayerState& player, int optionIndex, int pipIndex, int pipCount) {
    const Vec2 facing = NormalizeOr(player.facing, Vec2{1.0f, 0.0f});
    const Vec2 right{-facing.y, facing.x};
    const float lane = (static_cast<float>(pipIndex) - static_cast<float>(pipCount - 1) * 0.5f) * 0.24f;
    return RewardIconPosition(player, optionIndex) + facing * 0.48f + right * lane;
}

RenderVfxKind RewardIconKind(const RewardOption& option) {
    switch (option.kind) {
    case RewardKind::WeaponSwap:
        return RenderVfxKind::PortalPulse;
    case RewardKind::ElementInfusion:
        return RenderVfxKind::HitSpark;
    case RewardKind::PlayerUpgrade:
        return RenderVfxKind::RoomClearPulse;
    }
    return RenderVfxKind::HitSpark;
}

Vec3 FacingVfxDirection(Vec2 facing) {
    const Vec2 normalized = NormalizeOr(facing, Vec2{1.0f, 0.0f});
    return Vec3{normalized.x, 0.0f, normalized.y};
}

RenderVfxKind WeaponActionVfxKind(AttackShape shape) {
    switch (shape) {
    case AttackShape::Projectile:
    case AttackShape::Wave:
    case AttackShape::Dash:
        return RenderVfxKind::WeaponLine;
    case AttackShape::Burst:
        return RenderVfxKind::WeaponBurst;
    case AttackShape::Circle:
    case AttackShape::Orbit:
    case AttackShape::TargetArea:
        return RenderVfxKind::WeaponRing;
    case AttackShape::Cone:
        return RenderVfxKind::WeaponCone;
    }
    return RenderVfxKind::WeaponCone;
}

float WeaponActionVfxRadius(const WeaponActionSpec& action, AttackShape shape) {
    switch (shape) {
    case AttackShape::Projectile:
    case AttackShape::Wave:
    case AttackShape::Dash:
        return Clamp(action.range > 0.0f ? action.range : action.speed * action.duration, 1.2f, 6.8f);
    case AttackShape::Burst:
        return Clamp(action.range * 0.68f, 2.0f, 5.7f);
    case AttackShape::Circle:
    case AttackShape::Orbit:
    case AttackShape::TargetArea:
        return Clamp(action.radius, 1.0f, 5.2f);
    case AttackShape::Cone:
        return Clamp(action.range > 0.0f ? action.range : action.radius, 1.4f, 4.8f);
    }
    return 1.0f;
}

Vec2 WeaponActionVfxPosition(const PlayerState& player, const WeaponActionSpec& action, AttackShape shape) {
    const Vec2 facing = NormalizeOr(player.facing, Vec2{1.0f, 0.0f});
    switch (shape) {
    case AttackShape::TargetArea:
        return player.position + facing * Clamp(action.range * 0.55f, 1.0f, 3.8f);
    case AttackShape::Projectile:
    case AttackShape::Wave:
    case AttackShape::Dash:
    case AttackShape::Cone:
    case AttackShape::Burst:
        return player.position + facing * 0.42f;
    case AttackShape::Circle:
    case AttackShape::Orbit:
        return player.position;
    }
    return player.position;
}

const char* SafeActionName(const WeaponSpec& weapon, int actionIndex) {
    const char* name = weapon.actionNames[static_cast<std::size_t>(actionIndex)];
    return name && name[0] != '\0' ? name : (actionIndex == 0 ? "Attack" : "Ability");
}

const char* ShapeName(AttackShape shape) {
    switch (shape) {
    case AttackShape::Circle:
        return "Pulse";
    case AttackShape::Cone:
        return "Cone";
    case AttackShape::Projectile:
        return "Shot";
    case AttackShape::Burst:
        return "Burst";
    case AttackShape::Dash:
        return "Dash";
    case AttackShape::Wave:
        return "Wave";
    case AttackShape::Orbit:
        return "Orbit";
    case AttackShape::TargetArea:
        return "Mark";
    }
    return "Shape";
}

const char* ElementName(Element element) {
    switch (element) {
    case Element::Water:
        return "Water";
    case Element::Fire:
        return "Fire";
    case Element::Stone:
        return "Stone";
    case Element::Electricity:
        return "Volt";
    case Element::Ice:
        return "Ice";
    case Element::Air:
        return "Air";
    case Element::None:
        return "None";
    }
    return "None";
}

const char* UpgradeName(PlayerUpgradeKind upgrade) {
    switch (upgrade) {
    case PlayerUpgradeKind::Damage:
        return "Damage";
    case PlayerUpgradeKind::Cooldown:
        return "Cooldown";
    case PlayerUpgradeKind::Speed:
        return "Speed";
    case PlayerUpgradeKind::Area:
        return "Area";
    case PlayerUpgradeKind::MaxHp:
        return "MaxHP";
    case PlayerUpgradeKind::Heal:
        return "Heal";
    }
    return "Upgrade";
}

const char* ObjectiveName(RoomObjectiveKind objective) {
    switch (objective) {
    case RoomObjectiveKind::KillAll:
        return "KILL";
    case RoomObjectiveKind::SurviveTimer:
        return "SURV";
    case RoomObjectiveKind::ControlPoint:
        return "CTRL";
    }
    return "OBJ";
}

const char* ReactionName(ReactionKind reaction) {
    switch (reaction) {
    case ReactionKind::WetFire:
        return "WFIR";
    case ReactionKind::BurningWater:
        return "BWAT";
    case ReactionKind::WetElectricity:
        return "WVLT";
    case ReactionKind::WetIce:
        return "WICE";
    case ReactionKind::WetAir:
        return "WAIR";
    case ReactionKind::BurningStone:
        return "BSTN";
    case ReactionKind::BurningElectricity:
        return "BVLT";
    case ReactionKind::BurningIce:
        return "BICE";
    case ReactionKind::BurningAir:
        return "BAIR";
    case ReactionKind::ChargedWater:
        return "CWAT";
    case ReactionKind::ChargedFire:
        return "CFIR";
    case ReactionKind::ChargedElectricity:
        return "CVLT";
    case ReactionKind::ChargedIce:
        return "CICE";
    case ReactionKind::ChilledWater:
        return "IWAT";
    case ReactionKind::ChilledFire:
        return "IFIR";
    case ReactionKind::ChilledStone:
        return "ISTN";
    case ReactionKind::ChilledElectricity:
        return "IVLT";
    case ReactionKind::ChilledIce:
        return "IICE";
    case ReactionKind::ChargedAir:
        return "CAIR";
    case ReactionKind::ChilledAir:
        return "IAIR";
    case ReactionKind::ElectricDischarge:
        return "ARC";
    case ReactionKind::None:
        return "NONE";
    }
    return "NONE";
}

const char* RunStatusName(RunStatus status) {
    switch (status) {
    case RunStatus::InProgress:
        return "RUN";
    case RunStatus::Failed:
        return "DOWN";
    case RunStatus::FloorComplete:
        return "CLEAR";
    }
    return "RUN";
}

void AppendStatusLabel(char* out, std::size_t size, const char* label) {
    if (!out || size == 0 || !label) {
        return;
    }
    const std::size_t used = std::strlen(out);
    if (used + 1u >= size) {
        return;
    }
    std::snprintf(out + used, size - used, "%s%s", used > 0 ? " " : "", label);
}

void FormatStatusMask(uint32_t mask, char* out, std::size_t size) {
    if (!out || size == 0) {
        return;
    }
    out[0] = '\0';
    if ((mask & kOverlayStatusWetBit) != 0u) {
        AppendStatusLabel(out, size, "WET");
    }
    if ((mask & kOverlayStatusBurningBit) != 0u) {
        AppendStatusLabel(out, size, "BURN");
    }
    if ((mask & kOverlayStatusChargedBit) != 0u) {
        AppendStatusLabel(out, size, "CHRG");
    }
    if ((mask & kOverlayStatusChilledBit) != 0u) {
        AppendStatusLabel(out, size, "CHILL");
    }
    if (out[0] == '\0') {
        std::snprintf(out, size, "OK");
    }
}

uint32_t ObjectiveProgressPercent(const RoomGraph& world, const CombatSnapshot& snapshot) {
    if (snapshot.currentRoom < 0 || snapshot.currentRoom >= world.roomCount) {
        return 0u;
    }

    const Room& room = world.rooms[snapshot.currentRoom];
    if (room.lifecycle == RoomLifecycle::Completed || room.objective.completed) {
        return 100u;
    }

    switch (room.objective.kind) {
    case RoomObjectiveKind::SurviveTimer:
    case RoomObjectiveKind::ControlPoint:
        if (room.objective.targetSeconds <= 0.0001f) {
            return 100u;
        }
        return static_cast<uint32_t>(std::lround(Clamp(
            room.objective.elapsedSeconds / room.objective.targetSeconds,
            0.0f,
            1.0f) * 100.0f));
    case RoomObjectiveKind::KillAll:
        return snapshot.activeEnemies == 0 ? 100u : 0u;
    }
    return 0u;
}

void FormatRewardOptionLabel(const RewardOption& option, int index, char* out, std::size_t size) {
    if (!out || size == 0) {
        return;
    }

    switch (option.kind) {
    case RewardKind::WeaponSwap:
        if (option.synergyElement != Element::None) {
            std::snprintf(out, size, "%d S%d %s %s RX %s", index + 1, option.targetSlot + 1, GetWeaponSpec(option.weapon).name, ElementName(option.element), ElementName(option.synergyElement));
        } else {
            std::snprintf(out, size, "%d S%d %s %s", index + 1, option.targetSlot + 1, GetWeaponSpec(option.weapon).name, ElementName(option.element));
        }
        break;
    case RewardKind::ElementInfusion:
        if (option.synergyElement != Element::None) {
            std::snprintf(out, size, "%d S%d %s +%s RX %s", index + 1, option.targetSlot + 1, GetWeaponSpec(option.weapon).name, ElementName(option.element), ElementName(option.synergyElement));
        } else {
            std::snprintf(out, size, "%d S%d %s +%s", index + 1, option.targetSlot + 1, GetWeaponSpec(option.weapon).name, ElementName(option.element));
        }
        break;
    case RewardKind::PlayerUpgrade:
        std::snprintf(out, size, "%d +%s", index + 1, UpgradeName(option.upgrade));
        break;
    }
}

void FormatRewardAppliedLabel(uint32_t packed, char* out, std::size_t size) {
    if (!out || size == 0) {
        return;
    }
    if (!RewardOverlayOptionActive(packed)) {
        std::snprintf(out, size, "GAIN");
        return;
    }

    const RewardKind kind = RewardOverlayOptionKind(packed);
    const int slot = RewardOverlayOptionSlot(packed);
    const WeaponId weapon = RewardOverlayOptionWeapon(packed);
    const Element element = RewardOverlayOptionElement(packed);
    const PlayerUpgradeKind upgrade = RewardOverlayOptionUpgrade(packed);
    switch (kind) {
    case RewardKind::WeaponSwap:
        std::snprintf(out, size, "GAIN S%d %s %s", slot + 1, GetWeaponSpec(weapon).name, ElementName(element));
        break;
    case RewardKind::ElementInfusion:
        std::snprintf(out, size, "GAIN S%d %s +%s", slot + 1, GetWeaponSpec(weapon).name, ElementName(element));
        break;
    case RewardKind::PlayerUpgrade:
        std::snprintf(out, size, "GAIN +%s", UpgradeName(upgrade));
        break;
    }
}

void FillRectRgb(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    if (brush) {
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    }
}

void DrawCooldownBar(HDC dc, int x, int y, int width, int height, float readyPercent, COLORREF fillColor) {
    RECT back{x, y, x + width, y + height};
    FillRectRgb(dc, back, RGB(28, 24, 30));
    FrameRect(dc, &back, reinterpret_cast<HBRUSH>(GetStockObject(DKGRAY_BRUSH)));

    const float ready = Clamp(readyPercent, 0.0f, 100.0f) / 100.0f;
    RECT fill{x + 1, y + 1, x + 1 + static_cast<int>((width - 2) * ready), y + height - 1};
    if (fill.right > fill.left) {
        FillRectRgb(dc, fill, fillColor);
    }
}

D3D12OverlayConstants MakeOverlayConstants(
    const RenderScene& scene,
    const GameSession& session,
    uint32_t reactionKind,
    uint32_t reactionFlashPercent,
    uint32_t damageFlashPercent,
    uint32_t damageElementId,
    uint32_t rewardAppliedFlashPercent,
    uint32_t rewardAppliedOption) {
    D3D12OverlayConstants overlay{};
    const RunPhase phase = session.Phase();
    overlay.enabled = 1;
    overlay.weaponId = scene.overlay.overlayWeaponId;
    overlay.elementId = scene.overlay.overlayElementId;
    overlay.activeSlot = scene.overlay.overlayActiveSlot;
    overlay.qReadyPercent = scene.overlay.overlayQReadyPercent;
    overlay.eReadyPercent = scene.overlay.overlayEReadyPercent;
    overlay.qActionShape = scene.overlay.overlayQActionShape;
    overlay.eActionShape = scene.overlay.overlayEActionShape;
    overlay.loadoutSlot0 = scene.overlay.overlayLoadoutSlot0;
    overlay.loadoutSlot1 = scene.overlay.overlayLoadoutSlot1;
    overlay.loadoutSlot2 = scene.overlay.overlayLoadoutSlot2;
    overlay.hp = scene.overlay.overlayHp;
    overlay.currentRoom = scene.overlay.overlayCurrentRoom;
    overlay.roomCount = scene.overlay.overlayRoomCount;
    overlay.activeEnemies = scene.overlay.overlayActiveEnemies;
    overlay.bossHpPercent = scene.overlay.overlayBossHpPercent;
    overlay.bossPhase = scene.overlay.overlayBossPhase;
    overlay.objectiveKind = scene.overlay.overlayObjectiveKind;
    overlay.objectiveProgressPercent = scene.overlay.overlayObjectiveProgressPercent;
    overlay.playerStatusMask = scene.overlay.overlayPlayerStatusMask;
    overlay.reactionKind = reactionKind;
    overlay.reactionFlashPercent = reactionFlashPercent;
    overlay.phase = phase == RunPhase::RewardChoice ? 1u : 0u;
    overlay.runStatus = scene.overlay.overlayRunStatus;
    overlay.outputWidth = scene.frame.displayWidth;
    overlay.outputHeight = scene.frame.displayHeight;
    overlay.reserved0 = scene.frame.frameIndex;
    overlay.damageFlashPercent = damageFlashPercent;
    overlay.damageElementId = damageElementId;
    overlay.rewardAppliedFlashPercent = rewardAppliedFlashPercent;
    overlay.rewardAppliedOption = rewardAppliedOption;
    overlay.visualStyleIdentity = scene.frame.visualStyleIdentity;
    overlay.visualStyleSurface = scene.frame.visualStyleSurface;
    overlay.visualStyleAtmosphere = scene.frame.visualStyleAtmosphere;
    overlay.visualStyleVariant = scene.frame.visualStyleVariant;
    overlay.renderWidth = scene.frame.outputWidth;
    overlay.renderHeight = scene.frame.outputHeight;
    overlay.floorIndex = scene.overlay.overlayFloorIndex;
    overlay.descentPercent = scene.overlay.overlayDescentPercent;
    overlay.spriteCount = scene.spriteCount;
    if (phase == RunPhase::RewardChoice) {
        const auto& options = session.RewardOptions();
        overlay.rewardOptionCount = static_cast<uint32_t>(session.RewardOptionCount());
        overlay.rewardOption0 = session.RewardOptionCount() > 0 ? PackRewardOverlayOption(options[0]) : 0u;
        overlay.rewardOption1 = session.RewardOptionCount() > 1 ? PackRewardOverlayOption(options[1]) : 0u;
        overlay.rewardOption2 = session.RewardOptionCount() > 2 ? PackRewardOverlayOption(options[2]) : 0u;
    }
    return overlay;
}

}

bool Application::Initialize(HWND hwnd, const EngineConfig& config) {
    hwnd_ = hwnd;
    config_ = config;
    session_.Start(config.seed, config.startFloorIndex);
    if (config.smokeCombatRoom >= 0) {
        session_.TryEnterRoom(config.smokeCombatRoom);
    }

    if (!d3d_.Initialize(hwnd, config.width, config.height, config.requireDxr)) {
        char message[512]{};
        std::snprintf(message, sizeof(message),
            config.requireDxr
                ? "D3D12/DXR initialization failed.\n\nAdapter: %s\nReason: %s\n\nThis prototype currently requires a DXR-capable GPU."
                : "D3D12 initialization failed.\n\nAdapter: %s\nReason: %s\n\nRaster test mode still requires a working D3D12 device.",
            d3d_.AdapterName().empty() ? "<not selected>" : d3d_.AdapterName().c_str(),
            d3d_.LastError().empty() ? "unknown initialization failure" : d3d_.LastError().c_str());
        MessageBoxA(hwnd,
            message,
            "Rogue96DXR",
            MB_ICONERROR | MB_OK);
        return false;
    }

    if (config.requireDxr && d3d_.InitResult().dxrSupported && !dxr_.Initialize(d3d_.DxrDevice())) {
        MessageBoxA(hwnd,
            "DXR state object creation failed. Offline embedded DXIL or driver support is invalid.",
            "Rogue96DXR",
            MB_ICONERROR | MB_OK);
        return false;
    }

    return true;
}

void Application::Shutdown() {
    d3d_.Shutdown();
}

void Application::Tick(const InputState& input, float dt) {
    timeSeconds_ += dt;
    titleAccumulator_ += dt;
    UpdateVfxPulses(dt);
    InputState effectiveInput = input;
    if (config_.smokeCombatRoom >= 0) {
        const float phase = std::fmod(timeSeconds_, 1.38f);
        effectiveInput.aimX = std::cos(timeSeconds_ * 0.84f) * 0.34f + 0.84f;
        effectiveInput.aimY = std::sin(timeSeconds_ * 0.62f) * 0.28f + 0.18f;
        effectiveInput.moveX = std::sin(timeSeconds_ * 0.70f) * 0.22f;
        effectiveInput.moveY = std::cos(timeSeconds_ * 0.56f) * 0.18f;
        effectiveInput.action1 = phase < 0.20f || (phase > 0.44f && phase < 0.56f);
        effectiveInput.action2 = phase > 0.78f && phase < 1.18f;
        effectiveInput.melee = effectiveInput.action1;
        effectiveInput.ranged = effectiveInput.action2;
        effectiveInput.control = effectiveInput.action2;
    }
    const GameSessionTickResult& tick = session_.Tick(effectiveInput, dt);
    CollectVfxPulses(tick);

    std::array<RenderVfxPulse, kFrameVfxPulseCapacity> framePulses{};
    int framePulseCount = 0;
    for (const RenderVfxPulse& pulse : vfxPulses_) {
        if (pulse.ttl > 0.0f && framePulseCount < kFrameVfxPulseCapacity) {
            framePulses[framePulseCount++] = pulse;
        }
    }
    if (session_.Phase() == RunPhase::RewardChoice) {
        const PlayerState& player = session_.Combat().Player();
        const auto& options = session_.RewardOptions();
        for (int i = 0; i < session_.RewardOptionCount() && i < kRewardChoiceCount && framePulseCount < kFrameVfxPulseCapacity; ++i) {
            const RewardOption& option = options[i];
            if (!option.active) {
                continue;
            }
            const uint32_t iconPhase = option.iconSeed ^ static_cast<uint32_t>(i * 977);
            const float pulse = 0.5f + 0.5f * std::sin(timeSeconds_ * 4.0f + static_cast<float>(iconPhase & 255u) * 0.02454369f);
            framePulses[framePulseCount++] = RenderVfxPulse{
                RewardIconKind(option),
                MakePulsePosition(RewardIconPosition(player, i), 0.32f + pulse * 0.08f),
                0.46f + static_cast<float>((option.iconSeed >> 4) & 7u) * 0.035f,
                0.52f + pulse * 0.42f,
                1.0f,
                0.95f + pulse * 0.35f
            };
            for (int pip = 0; pip <= i && framePulseCount < kFrameVfxPulseCapacity; ++pip) {
                framePulses[framePulseCount++] = RenderVfxPulse{
                    RenderVfxKind::HitSpark,
                    MakePulsePosition(RewardPipPosition(player, i, pip, i + 1), 0.62f + pulse * 0.04f),
                    0.12f,
                    0.48f + pulse * 0.24f,
                    1.0f,
                    1.05f
                };
            }
        }
    }

    const uint32_t renderWidth = ScaledRenderDimension(config_.width, config_.renderScalePercent);
    const uint32_t renderHeight = ScaledRenderDimension(config_.height, config_.renderScalePercent);
    renderScene_ = BuildRenderScene(
        session_.World(),
        session_.Combat(),
        renderWidth,
        renderHeight,
        timeSeconds_,
        frameIndex_++,
        std::span<const RenderVfxPulse>(framePulses.data(), static_cast<std::size_t>(framePulseCount)),
        static_cast<uint32_t>(session_.Status()),
        config_.width,
        config_.height,
        config_.renderQuality);
}

void Application::Render() {
    const float pulse = 0.5f + 0.5f * std::sin(timeSeconds_ * 1.7f);
    const uint32_t reactionPercent = static_cast<uint32_t>(std::lround(
        Clamp(reactionFeedbackTtl_ / kReactionFeedbackDuration, 0.0f, 1.0f) * 100.0f));
    const uint32_t damagePercent = static_cast<uint32_t>(std::lround(
        Clamp(playerDamageFeedbackTtl_ / kPlayerDamageFeedbackDuration, 0.0f, 1.0f) * 100.0f));
    const uint32_t rewardPercent = static_cast<uint32_t>(std::lround(
        Clamp(rewardAppliedFeedbackTtl_ / kRewardAppliedFeedbackDuration, 0.0f, 1.0f) * 100.0f));
    const D3D12OverlayConstants overlay = MakeOverlayConstants(
        renderScene_,
        session_,
        reactionFeedbackKind_,
        reactionPercent,
        damagePercent,
        playerDamageElement_,
        rewardPercent,
        rewardAppliedPacked_);
    if (config_.requireDxr && d3d_.InitResult().dxrSupported) {
        d3d_.RenderFrame(pulse, &Application::RenderDxrFrame, this, &overlay);
        return;
    }
    d3d_.RenderFrame(pulse, nullptr, nullptr, &overlay);
    DrawOverlay();
}

D3D12FrameCallbackResult Application::RenderDxrFrame(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, void* userData) {
    Application* app = static_cast<Application*>(userData);
    if (!app || !device || !commandList) {
        return D3D12FrameCallbackResult{false, nullptr, D3D12_GPU_DESCRIPTOR_HANDLE{}};
    }

    if (!app->dxrSceneResources_.Update(device, commandList, app->renderScene_)) {
        return D3D12FrameCallbackResult{false, nullptr, D3D12_GPU_DESCRIPTOR_HANDLE{}};
    }

    app->dxr_.Dispatch(commandList, app->dxrSceneResources_, app->renderScene_.frame.outputWidth, app->renderScene_.frame.outputHeight);
    app->dxrSceneResources_.InsertOutputUavBarrier(commandList);
    app->dxrSceneResources_.TransitionOutput(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    return D3D12FrameCallbackResult{
        true,
        app->dxrSceneResources_.DescriptorHeap(),
        app->dxrSceneResources_.OutputSrvGpu()
    };
}

void Application::UpdateVfxPulses(float dt) {
    if (reactionFeedbackTtl_ > 0.0f) {
        reactionFeedbackTtl_ = Clamp(reactionFeedbackTtl_ - dt, 0.0f, kReactionFeedbackDuration);
        if (reactionFeedbackTtl_ <= 0.0f) {
            reactionFeedbackKind_ = 0;
        }
    }
    if (playerDamageFeedbackTtl_ > 0.0f) {
        playerDamageFeedbackTtl_ = Clamp(playerDamageFeedbackTtl_ - dt, 0.0f, kPlayerDamageFeedbackDuration);
        if (playerDamageFeedbackTtl_ <= 0.0f) {
            playerDamageElement_ = static_cast<uint32_t>(Element::None);
        }
    }
    if (rewardAppliedFeedbackTtl_ > 0.0f) {
        rewardAppliedFeedbackTtl_ = Clamp(rewardAppliedFeedbackTtl_ - dt, 0.0f, kRewardAppliedFeedbackDuration);
        if (rewardAppliedFeedbackTtl_ <= 0.0f) {
            rewardAppliedPacked_ = 0u;
        }
    }

    for (RenderVfxPulse& pulse : vfxPulses_) {
        if (pulse.ttl > 0.0f) {
            pulse.ttl = Clamp(pulse.ttl - dt, 0.0f, pulse.duration);
        }
    }
}

void Application::CollectVfxPulses(const GameSessionTickResult& tick) {
    const RoomGraph& world = session_.World();
    const auto& enemies = session_.Combat().Enemies();

    for (int i = 0; i < tick.eventCount; ++i) {
        const GameEvent& event = tick.events[i];
        switch (event.type) {
        case GameEventType::EnemyDamaged:
            if (event.entityIndex >= 0 && event.entityIndex < kMaxEnemies) {
                const EnemyState& enemy = enemies[event.entityIndex];
                const float radius = 0.34f + Clamp(event.value, 0.0f, 48.0f) * 0.012f;
                AddVfxPulse(RenderVfxKind::HitSpark, MakePulsePosition(enemy.position, 0.52f), radius, 0.28f, 1.0f);
            }
            break;
        case GameEventType::EnemyKilled:
            if (event.entityIndex >= 0 && event.entityIndex < kMaxEnemies) {
                const EnemyState& enemy = enemies[event.entityIndex];
                AddVfxPulse(RenderVfxKind::HitSpark, MakePulsePosition(enemy.position, 0.62f), 0.90f, 0.38f, 1.35f);
            }
            break;
        case GameEventType::RoomCompleted:
            if (event.roomIndex >= 0 && event.roomIndex < world.roomCount) {
                const Room& room = world.rooms[event.roomIndex];
                const float radius = (room.halfSize.x < room.halfSize.y ? room.halfSize.x : room.halfSize.y) * 0.82f;
                AddVfxPulse(RenderVfxKind::RoomClearPulse, MakePulsePosition(room.center, 0.10f), radius, 0.95f, 1.0f);
            }
            break;
        case GameEventType::RewardOffered:
            if (event.roomIndex >= 0 && event.roomIndex < world.roomCount) {
                const Room& room = world.rooms[event.roomIndex];
                AddVfxPulse(RenderVfxKind::RoomClearPulse, MakePulsePosition(room.center, 0.18f), 1.45f, 0.90f, 1.25f);
            }
            break;
        case GameEventType::RewardSelected:
            rewardAppliedPacked_ = event.payload;
            rewardAppliedFeedbackTtl_ = kRewardAppliedFeedbackDuration;
            AddVfxPulse(RenderVfxKind::HitSpark, MakePulsePosition(session_.Combat().Player().position, 0.70f), 0.82f, 0.34f, 1.25f);
            break;
        case GameEventType::PlayerActionUsed:
        case GameEventType::PlayerAbilityUsed: {
            const PlayerState& player = session_.Combat().Player();
            const int actionSlot = event.value > 0.5f ? 1 : 0;
            const WeaponSpec& weapon = GetWeaponSpec(event.weapon);
            const WeaponActionSpec& action = weapon.actions[static_cast<std::size_t>(actionSlot)];
            const AttackShape shape = event.actionShape;
            const float duration = event.type == GameEventType::PlayerAbilityUsed ? 0.38f : 0.22f;
            const float intensity = event.type == GameEventType::PlayerAbilityUsed ? 1.25f : 0.85f;
            AddVfxPulse(
                WeaponActionVfxKind(shape),
                MakePulsePosition(WeaponActionVfxPosition(player, action, shape), 0.12f),
                WeaponActionVfxRadius(action, shape),
                duration,
                intensity,
                FacingVfxDirection(player.facing));
            break;
        }
        case GameEventType::ReactionTriggered: {
            reactionFeedbackKind_ = static_cast<uint32_t>(Clamp(
                event.value,
                static_cast<float>(ReactionKind::None),
                static_cast<float>(ReactionKind::ElectricDischarge)));
            reactionFeedbackTtl_ = kReactionFeedbackDuration;
            Vec2 position = session_.Combat().Player().position;
            if (event.entityIndex >= 0 && event.entityIndex < kMaxEnemies) {
                position = enemies[event.entityIndex].position;
            }
            AddVfxPulse(RenderVfxKind::RoomClearPulse, MakePulsePosition(position, 0.18f), 0.92f, 0.44f, 1.15f);
            break;
        }
        case GameEventType::PortalOpened:
            if (event.entityIndex >= 0 && event.entityIndex < world.portalCount) {
                const Portal& portal = world.portals[event.entityIndex];
                AddVfxPulse(RenderVfxKind::PortalPulse, MakePulsePosition(portal.position, 0.12f), portal.radius * 1.35f, 1.10f, 1.15f);
            }
            break;
        case GameEventType::PlayerDamaged:
            playerDamageFeedbackTtl_ = kPlayerDamageFeedbackDuration;
            playerDamageElement_ = static_cast<uint32_t>(event.element);
            AddVfxPulse(
                RenderVfxKind::HitSpark,
                MakePulsePosition(session_.Combat().Player().position, 0.58f),
                0.48f + Clamp(event.value, 0.0f, 30.0f) * 0.018f,
                0.30f,
                1.05f);
            break;
        default:
            break;
        }
    }
}

void Application::AddVfxPulse(RenderVfxKind kind, Vec3 position, float radius, float duration, float intensity, Vec3 direction) {
    if (radius <= 0.0f || duration <= 0.0f) {
        return;
    }

    int slot = 0;
    float lowestTtl = vfxPulses_[0].ttl;
    for (int i = 0; i < kMaxRenderVfxPulses; ++i) {
        if (vfxPulses_[i].ttl <= 0.0f) {
            slot = i;
            break;
        }
        if (vfxPulses_[i].ttl < lowestTtl) {
            lowestTtl = vfxPulses_[i].ttl;
            slot = i;
        }
    }

    vfxPulses_[slot] = RenderVfxPulse{
        kind,
        position,
        radius,
        duration,
        duration,
        intensity,
        direction
    };
}

void Application::DrawOverlay() {
    if (!hwnd_) {
        return;
    }

    HDC dc = GetDC(hwnd_);
    if (!dc) {
        return;
    }

    HFONT font = CreateFontA(
        16,
        0,
        0,
        0,
        FW_SEMIBOLD,
        FALSE,
        FALSE,
        FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN,
        "Consolas");
    HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
    const int oldBk = SetBkMode(dc, TRANSPARENT);
    const COLORREF oldColor = SetTextColor(dc, RGB(238, 246, 240));

    RECT panel{12, 12, 444, session_.Phase() == RunPhase::RewardChoice ? 148 : 188};
    FillRectRgb(dc, panel, RGB(12, 10, 16));
    FrameRect(dc, &panel, reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));

    const CombatSnapshot snapshot = session_.Snapshot();
    char line[192]{};
    if (session_.Phase() == RunPhase::RewardChoice) {
        SetTextColor(dc, RGB(110, 232, 205));
        TextOutA(dc, 26, 26, "REWARD 1/2/3", 12);
        const auto& options = session_.RewardOptions();
        for (int i = 0; i < session_.RewardOptionCount() && i < 3; ++i) {
            if (!options[i].active) {
                continue;
            }
            FormatRewardOptionLabel(options[i], i, line, sizeof(line));
            SetTextColor(dc, i == 0 ? RGB(255, 188, 96) : (i == 1 ? RGB(232, 86, 170) : RGB(120, 232, 210)));
            TextOutA(dc, 26, 52 + i * 26, line, static_cast<int>(std::strlen(line)));
        }
    } else {
        const PlayerState& player = snapshot.player;
        const int activeSlot = player.activeWeaponSlot >= 0 && player.activeWeaponSlot < kPlayerWeaponSlots ? player.activeWeaponSlot : 0;
        const WeaponSlot& slot = player.weaponSlots[activeSlot];
        const WeaponSpec& weapon = GetWeaponSpec(slot.weapon);
        const float qReady = (1.0f - PlayerActionCooldownRatio(player, WeaponActionIndex::Action1)) * 100.0f;
        const float eReady = (1.0f - PlayerActionCooldownRatio(player, WeaponActionIndex::Action2)) * 100.0f;
        const Room& room = session_.World().rooms[snapshot.currentRoom];
        const uint32_t objectiveProgress = ObjectiveProgressPercent(session_.World(), snapshot);

        std::snprintf(line, sizeof(line), "%-5s HP %03.0f   ROOM %02d/%02d   ENEMY %02d",
            RunStatusName(session_.Status()),
            player.hp,
            snapshot.currentRoom + 1,
            session_.World().roomCount,
            snapshot.activeEnemies);
        SetTextColor(dc, session_.Status() == RunStatus::Failed ? RGB(255, 92, 92) : RGB(114, 220, 220));
        TextOutA(dc, 26, 26, line, static_cast<int>(std::strlen(line)));

        if (renderScene_.overlay.overlayBossPhase > 0u) {
            std::snprintf(line, sizeof(line), "OBJ %-5s %03u%%   BOS P%u %03u%%",
                ObjectiveName(room.objective.kind),
                objectiveProgress,
                renderScene_.overlay.overlayBossPhase,
                renderScene_.overlay.overlayBossHpPercent);
        } else {
            std::snprintf(line, sizeof(line), "OBJ %-5s %03u%%",
                ObjectiveName(room.objective.kind),
                objectiveProgress);
        }
        SetTextColor(dc, RGB(110, 232, 205));
        TextOutA(dc, 26, 52, line, static_cast<int>(std::strlen(line)));

        char statusLine[64]{};
        FormatStatusMask(renderScene_.overlay.overlayPlayerStatusMask, statusLine, sizeof(statusLine));
        if (reactionFeedbackKind_ > 0 && reactionFeedbackTtl_ > 0.0f) {
            std::snprintf(line, sizeof(line), "STS %-18s RX %s",
                statusLine,
                ReactionName(static_cast<ReactionKind>(reactionFeedbackKind_)));
        } else {
            std::snprintf(line, sizeof(line), "STS %s", statusLine);
        }
        SetTextColor(dc, RGB(170, 238, 224));
        TextOutA(dc, 26, 76, line, static_cast<int>(std::strlen(line)));

        std::snprintf(line, sizeof(line), "WPN %s %s   SLOT %d",
            weapon.name,
            ElementName(slot.element),
            activeSlot + 1);
        SetTextColor(dc, RGB(238, 246, 240));
        TextOutA(dc, 26, 100, line, static_cast<int>(std::strlen(line)));

        std::snprintf(line, sizeof(line), "Q  %-10s %-5s %03.0f%%",
            SafeActionName(weapon, 0),
            ShapeName(weapon.actions[0].shape),
            Clamp(qReady, 0.0f, 100.0f));
        SetTextColor(dc, RGB(255, 171, 86));
        TextOutA(dc, 26, 126, line, static_cast<int>(std::strlen(line)));
        DrawCooldownBar(dc, 304, 130, 104, 9, qReady, RGB(255, 136, 54));

        std::snprintf(line, sizeof(line), "E  %-10s %-5s %03.0f%%",
            SafeActionName(weapon, 1),
            ShapeName(weapon.actions[1].shape),
            Clamp(eReady, 0.0f, 100.0f));
        SetTextColor(dc, RGB(242, 84, 176));
        TextOutA(dc, 26, 148, line, static_cast<int>(std::strlen(line)));
        DrawCooldownBar(dc, 304, 152, 104, 9, eReady, RGB(220, 70, 190));

        if (rewardAppliedFeedbackTtl_ > 0.0f && rewardAppliedPacked_ != 0u) {
            FormatRewardAppliedLabel(rewardAppliedPacked_, line, sizeof(line));
            SetTextColor(dc, RGB(120, 232, 210));
            TextOutA(dc, 26, 170, line, static_cast<int>(std::strlen(line)));
        }
    }

    SetTextColor(dc, oldColor);
    SetBkMode(dc, oldBk);
    if (oldFont) {
        SelectObject(dc, oldFont);
    }
    if (font) {
        DeleteObject(font);
    }
    ReleaseDC(hwnd_, dc);
}

void Application::UpdateWindowTitle(HWND hwnd, float elapsedSeconds) {
    (void)elapsedSeconds;
    if (titleAccumulator_ < 0.25f) {
        return;
    }
    titleAccumulator_ = 0.0f;

    const CombatSnapshot snapshot = session_.Snapshot();
    const char* mode = (config_.requireDxr && d3d_.InitResult().dxrSupported) ? "DXR" : "raster-test";
    if (session_.Phase() == RunPhase::RewardChoice) {
        const auto& options = session_.RewardOptions();
        char option0[56]{"1"};
        char option1[56]{"2"};
        char option2[56]{"3"};
        if (session_.RewardOptionCount() > 0 && options[0].active) {
            FormatRewardOptionLabel(options[0], 0, option0, sizeof(option0));
        }
        if (session_.RewardOptionCount() > 1 && options[1].active) {
            FormatRewardOptionLabel(options[1], 1, option1, sizeof(option1));
        }
        if (session_.RewardOptionCount() > 2 && options[2].active) {
            FormatRewardOptionLabel(options[2], 2, option2, sizeof(option2));
        }

        char rewardTitle[256]{};
        std::snprintf(rewardTitle, sizeof(rewardTitle),
            "Rogue96DXR | %s | floor %d | choose reward | %s | %s | %s | hp %.0f | seed 0x%08x",
            mode,
            session_.FloorIndex() + 1,
            option0,
            option1,
            option2,
            snapshot.player.hp,
            session_.World().seed);
        SetWindowTextA(hwnd, rewardTitle);
        return;
    }

    const char* phase = session_.Status() == RunStatus::Failed
        ? "run failed"
        : (session_.Status() == RunStatus::FloorComplete ? "floor clear" : "combat");
    const PlayerState& player = snapshot.player;
    const int activeSlot = player.activeWeaponSlot >= 0 && player.activeWeaponSlot < kPlayerWeaponSlots ? player.activeWeaponSlot : 0;
    const WeaponSpec& weapon = GetWeaponSpec(player.weaponSlots[activeSlot].weapon);
    const Room& room = session_.World().rooms[snapshot.currentRoom];
    const uint32_t objectiveProgress = ObjectiveProgressPercent(session_.World(), snapshot);
    const float qReady = (1.0f - PlayerActionCooldownRatio(player, WeaponActionIndex::Action1)) * 100.0f;
    const float eReady = (1.0f - PlayerActionCooldownRatio(player, WeaponActionIndex::Action2)) * 100.0f;
    char statusLine[64]{};
    FormatStatusMask(renderScene_.overlay.overlayPlayerStatusMask, statusLine, sizeof(statusLine));
    const char* reaction = (reactionFeedbackKind_ > 0 && reactionFeedbackTtl_ > 0.0f)
        ? ReactionName(static_cast<ReactionKind>(reactionFeedbackKind_))
        : "NONE";
    char rewardLine[80]{"GAIN NONE"};
    if (rewardAppliedFeedbackTtl_ > 0.0f && rewardAppliedPacked_ != 0u) {
        FormatRewardAppliedLabel(rewardAppliedPacked_, rewardLine, sizeof(rewardLine));
    }
    char title[384]{};
    std::snprintf(title, sizeof(title),
        "Rogue96DXR | %s | %s | floor %d depth %u%% | room %d/%d | obj %s %u%% | enemies %d | hp %.0f | sts %s | rx %s | %s | %s | Q %s/%s %.0f%% | E %s/%s %.0f%% | seed 0x%08x",
        mode,
        phase,
        session_.FloorIndex() + 1,
        renderScene_.overlay.overlayDescentPercent,
        snapshot.currentRoom + 1,
        session_.World().roomCount,
        ObjectiveName(room.objective.kind),
        objectiveProgress,
        snapshot.activeEnemies,
        snapshot.player.hp,
        statusLine,
        reaction,
        rewardLine,
        weapon.name,
        SafeActionName(weapon, 0),
        ShapeName(weapon.actions[0].shape),
        qReady,
        SafeActionName(weapon, 1),
        ShapeName(weapon.actions[1].shape),
        eReady,
        session_.World().seed);
    SetWindowTextA(hwnd, title);
}

}
