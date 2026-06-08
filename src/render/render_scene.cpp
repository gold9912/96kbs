#include "render/render_scene.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstddef>

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

DirectX::XMMATRIX UpdateCamera(
    RenderScene& scene,
    const RoomGraph& world,
    const CombatSim& combat,
    uint32_t outputWidth,
    uint32_t outputHeight) {
    const PlayerState& player = combat.Player();
    Vec2 roomCenter = player.position;
    Vec2 roomHalfSize{6.0f, 4.5f};
    if (player.roomIndex >= 0 && player.roomIndex < world.roomCount) {
        const Room& room = world.rooms[player.roomIndex];
        roomCenter = room.center;
        roomHalfSize = room.halfSize;
    }

    const Vec2 playerBias = (player.position - roomCenter) * 0.12f;
    scene.camera.target = Vec3{roomCenter.x + playerBias.x, 0.24f, roomCenter.y + playerBias.y};
    const float arenaRadius = std::max(roomHalfSize.x, roomHalfSize.y);
    const float aspect = outputHeight > 0
        ? static_cast<float>(outputWidth) / static_cast<float>(outputHeight)
        : 16.0f / 9.0f;
    const float narrowScale = aspect < 1.45f ? 1.12f : 1.0f;
    const float height = (20.6f + arenaRadius * 0.46f) * narrowScale;
    const float pullback = (13.2f + arenaRadius * 0.82f) * narrowScale;
    const float sideOffset = arenaRadius * 0.58f;
    scene.camera.position = Vec3{
        scene.camera.target.x - sideOffset,
        height,
        scene.camera.target.z - pullback
    };
    scene.camera.tiltRadians = 1.02f;

    const DirectX::XMVECTOR eye = DirectX::XMVectorSet(scene.camera.position.x, scene.camera.position.y, scene.camera.position.z, 1.0f);
    const DirectX::XMVECTOR target = DirectX::XMVectorSet(scene.camera.target.x, scene.camera.target.y, scene.camera.target.z, 1.0f);
    const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, target, up);
    const DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(0.420f, aspect, 0.05f, 220.0f);
    const DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);
    const DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(nullptr, viewProj);
    StoreMatrix(scene.frame.invViewProj, invViewProj);
    return viewProj;
}

Vec3 ScaleColor(Vec3 color, float scale) {
    return Vec3{
        Clamp(color.x * scale, 0.0f, 1.0f),
        Clamp(color.y * scale, 0.0f, 1.0f),
        Clamp(color.z * scale, 0.0f, 1.0f)
    };
}

Vec3 MixColor(Vec3 a, Vec3 b, float t) {
    const float u = Clamp(t, 0.0f, 1.0f);
    return Vec3{
        a.x + (b.x - a.x) * u,
        a.y + (b.y - a.y) * u,
        a.z + (b.z - a.z) * u
    };
}

std::array<EntityMaterial, kMaxDxrMaterials> DefaultMaterials(const RoomVisualStyle& style) {
    std::array<EntityMaterial, kMaxDxrMaterials> materials{};
    const Vec3 floor = VisualStyleColor(style, VisualStyleColorRole::Floor);
    const Vec3 wall = VisualStyleColor(style, VisualStyleColorRole::Wall);
    const Vec3 corridor = VisualStyleColor(style, VisualStyleColorRole::Corridor);
    const Vec3 light = VisualStyleColor(style, VisualStyleColorRole::Light);
    const Vec3 danger = VisualStyleColor(style, VisualStyleColorRole::Danger);
    const Vec3 control = VisualStyleColor(style, VisualStyleColorRole::Control);
    const Vec3 foliage = style.biome == VisualBiome::SunlitRuins || style.biome == VisualBiome::OvergrownSanctuary
        ? MixColor(Vec3{0.016f, 0.052f, 0.025f}, Vec3{0.046f, 0.124f, 0.044f}, 0.28f + style.moss * 0.20f)
        : MixColor(Vec3{0.050f, 0.110f, 0.086f}, control, 0.12f);
    const float darkGlow = style.glow * 0.30f + style.corruption * 0.16f;
    materials[0] = EntityMaterial{ScaleColor(floor, 0.90f), style.wetness * 0.024f};
    materials[1] = EntityMaterial{ScaleColor(wall, 0.86f), 0.003f + darkGlow * 0.030f};
    materials[2] = EntityMaterial{MixColor(Vec3{0.014f, 0.045f, 0.072f}, VisualStyleColor(style, VisualStyleColorRole::Hud), 0.18f), 0.052f + style.glow * 0.026f};
    materials[3] = EntityMaterial{ScaleColor(MixColor(light, Vec3{0.08f, 0.76f, 0.98f}, 0.56f), 0.68f), 0.24f + style.glow * 0.076f};
    materials[4] = EntityMaterial{ScaleColor(MixColor(Vec3{0.105f, 0.060f, 0.052f}, danger, 0.30f), 0.72f), 0.024f + style.corruption * 0.040f};
    materials[5] = EntityMaterial{ScaleColor(MixColor(VisualStyleColor(style, VisualStyleColorRole::Hud), Vec3{0.30f, 0.085f, 0.66f}, 0.58f), 0.68f), 0.065f + style.glow * 0.052f};
    materials[6] = EntityMaterial{Vec3{0.120f, 0.360f, 0.700f}, 0.34f};
    materials[7] = EntityMaterial{ScaleColor(MixColor(control, Vec3{0.045f, 0.74f, 0.54f}, 0.42f), 0.78f), 0.30f + style.glow * 0.090f};
    materials[8] = EntityMaterial{ScaleColor(MixColor(light, Vec3{0.78f, 0.16f, 0.52f}, 0.44f), 0.74f), 0.12f + style.glow * 0.060f};
    materials[9] = EntityMaterial{ScaleColor(MixColor(corridor, floor, 0.42f), 0.80f), 0.004f + style.wetness * 0.014f};
    materials[10] = EntityMaterial{ScaleColor(light, 0.86f), 0.36f + style.glow * 0.090f};
    materials[11] = EntityMaterial{ScaleColor(MixColor(control, Vec3{0.18f, 0.82f, 0.68f}, 0.36f), 0.68f), 0.135f + style.glow * 0.052f};
    materials[12] = EntityMaterial{ScaleColor(foliage, 0.50f), 0.002f + style.moss * 0.004f};
    materials[13] = EntityMaterial{MixColor(Vec3{0.48f, 0.58f, 0.70f}, floor, 0.25f), 0.09f + style.wetness * 0.055f};
    materials[14] = EntityMaterial{ScaleColor(MixColor(Vec3{0.090f, 0.040f, 0.066f}, danger, 0.46f), 0.70f), 0.22f + style.corruption * 0.075f};
    return materials;
}

uint32_t PercentReady(const PlayerState& player, WeaponActionIndex action) {
    const float ready = (1.0f - PlayerActionCooldownRatio(player, action)) * 100.0f;
    return static_cast<uint32_t>(std::lround(Clamp(ready, 0.0f, 100.0f)));
}

uint32_t PercentReadyForSlot(const PlayerState& player, int slotIndex, WeaponActionIndex action) {
    if (slotIndex < 0 || slotIndex >= kPlayerWeaponSlots) {
        return 0u;
    }
    const int actionSlot = action == WeaponActionIndex::Action2 ? 1 : 0;
    const WeaponSlot& slot = player.weaponSlots[slotIndex];
    const WeaponSpec& weapon = GetWeaponSpec(slot.weapon);
    const float cooldownScale = player.cooldownMultiplier > 0.0f ? player.cooldownMultiplier : 1.0f;
    const float maxCooldown = weapon.actions[static_cast<std::size_t>(actionSlot)].cooldown * cooldownScale;
    const float ratio = maxCooldown > 0.0001f
        ? Clamp(slot.cooldowns[static_cast<std::size_t>(actionSlot)] / maxCooldown, 0.0f, 1.0f)
        : 0.0f;
    return static_cast<uint32_t>(std::lround(Clamp((1.0f - ratio) * 100.0f, 0.0f, 100.0f)));
}

uint32_t PackPlayerLoadoutSlot(const PlayerState& player, int slotIndex, int activeSlot) {
    if (slotIndex < 0 || slotIndex >= kPlayerWeaponSlots) {
        return 0u;
    }
    const WeaponSlot& slot = player.weaponSlots[slotIndex];
    return PackLoadoutOverlaySlot(
        slot.weapon,
        slot.element,
        PercentReadyForSlot(player, slotIndex, WeaponActionIndex::Action1),
        PercentReadyForSlot(player, slotIndex, WeaponActionIndex::Action2),
        slotIndex == activeSlot);
}

uint32_t PlayerStatusMask(const PlayerState& player) {
    uint32_t mask = 0u;
    for (const StatusInstance& status : player.statuses.slots) {
        if (status.active) {
            mask |= OverlayStatusBit(status.kind);
        }
    }
    return mask;
}

uint32_t ObjectiveProgressPercent(const RoomGraph& world, const CombatSim& combat, int roomIndex) {
    if (roomIndex < 0 || roomIndex >= world.roomCount) {
        return 0u;
    }

    const Room& room = world.rooms[roomIndex];
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
        return combat.ActiveEnemiesInRoom(roomIndex) == 0 ? 100u : 0u;
    }
    return 0u;
}

struct BossOverlayInfo {
    uint32_t hpPercent = 0;
    uint32_t phase = 0;
};

BossOverlayInfo BossOverlayForRoom(const CombatSim& combat, int roomIndex) {
    if (roomIndex < 0) {
        return BossOverlayInfo{};
    }

    for (const EnemyState& enemy : combat.Enemies()) {
        if (!enemy.active || enemy.roomIndex != roomIndex || enemy.kind != EnemyKind::Boss || enemy.hp <= 0.0f) {
            continue;
        }

        const float ratio = enemy.maxHp > 0.0001f
            ? Clamp(enemy.hp / enemy.maxHp, 0.0f, 1.0f)
            : 0.0f;
        uint32_t hpPercent = static_cast<uint32_t>(std::lround(ratio * 100.0f));
        if (hpPercent == 0u && enemy.hp > 0.0f) {
            hpPercent = 1u;
        }
        const uint32_t phase = ratio <= 0.33f ? 3u : (ratio <= 0.66f ? 2u : 1u);
        return BossOverlayInfo{hpPercent, phase};
    }

    return BossOverlayInfo{};
}

int ActiveWeaponSlotIndex(const PlayerState& player) {
    if (player.activeWeaponSlot < 0 || player.activeWeaponSlot >= kPlayerWeaponSlots) {
        return 0;
    }
    return player.activeWeaponSlot;
}

Vec3 ElementSpriteColor(Element element) {
    switch (element) {
    case Element::Water:
        return Vec3{0.34f, 0.86f, 1.00f};
    case Element::Fire:
        return Vec3{1.00f, 0.40f, 0.16f};
    case Element::Stone:
        return Vec3{0.74f, 0.67f, 0.50f};
    case Element::Electricity:
        return Vec3{0.54f, 0.98f, 1.00f};
    case Element::Ice:
        return Vec3{0.70f, 0.90f, 1.00f};
    case Element::Air:
        return Vec3{0.72f, 1.00f, 0.76f};
    case Element::None:
        return Vec3{0.82f, 0.93f, 1.00f};
    }
    return Vec3{0.82f, 0.93f, 1.00f};
}

uint32_t AtlasForElement(Element element) {
    switch (element) {
    case Element::Water:
        return 1u;
    case Element::Fire:
        return 2u;
    case Element::Electricity:
        return 3u;
    case Element::Ice:
        return 4u;
    case Element::Stone:
        return 6u;
    case Element::Air:
        return 7u;
    case Element::None:
        return 0u;
    }
    return 0u;
}

Element StatusSpriteElement(const StatusInstance& status) {
    if (status.element != Element::None) {
        return status.element;
    }
    switch (status.kind) {
    case StatusKind::Wet:
        return Element::Water;
    case StatusKind::Burning:
        return Element::Fire;
    case StatusKind::Charged:
        return Element::Electricity;
    case StatusKind::Chilled:
        return Element::Ice;
    case StatusKind::None:
        return Element::None;
    }
    return Element::None;
}

uint32_t AtlasForVfxKind(RenderVfxKind kind) {
    switch (kind) {
    case RenderVfxKind::HitSpark:
        return 0u;
    case RenderVfxKind::RoomClearPulse:
        return 6u;
    case RenderVfxKind::PortalPulse:
        return 7u;
    case RenderVfxKind::WeaponCone:
    case RenderVfxKind::WeaponLine:
        return 5u;
    case RenderVfxKind::WeaponRing:
    case RenderVfxKind::WeaponBurst:
        return 0u;
    }
    return 0u;
}

Vec3 ColorForVfxKind(RenderVfxKind kind) {
    switch (kind) {
    case RenderVfxKind::HitSpark:
        return Vec3{0.68f, 0.95f, 1.00f};
    case RenderVfxKind::RoomClearPulse:
        return Vec3{0.34f, 1.00f, 0.70f};
    case RenderVfxKind::PortalPulse:
        return Vec3{0.42f, 0.92f, 1.00f};
    case RenderVfxKind::WeaponCone:
    case RenderVfxKind::WeaponLine:
        return Vec3{0.46f, 1.00f, 0.96f};
    case RenderVfxKind::WeaponRing:
    case RenderVfxKind::WeaponBurst:
        return Vec3{0.78f, 0.86f, 1.00f};
    }
    return Vec3{0.68f, 0.95f, 1.00f};
}

uint32_t AtlasForAction(AttackShape shape, Element element) {
    if (element == Element::Electricity) {
        return 3u;
    }
    switch (shape) {
    case AttackShape::Cone:
    case AttackShape::Dash:
    case AttackShape::Wave:
        return 5u;
    case AttackShape::Circle:
    case AttackShape::Burst:
    case AttackShape::Orbit:
    case AttackShape::TargetArea:
        return element == Element::Stone ? 6u : 0u;
    case AttackShape::Projectile:
        return AtlasForElement(element);
    }
    return AtlasForElement(element);
}

Vec3 WorldFromGround(Vec2 p, float height) {
    return Vec3{p.x, height, p.y};
}

uint32_t SpriteSeed(uint32_t frameIndex, uint32_t index, uint32_t salt) {
    uint32_t h = frameIndex * 747796405u + index * 2891336453u + salt * 277803737u;
    h ^= h >> 16u;
    h *= 2246822519u;
    h ^= h >> 13u;
    return h;
}

float SpriteHash01(uint32_t seed) {
    return static_cast<float>((seed >> 8u) & 0xffffu) / 65535.0f;
}

bool ProjectWorldToScreen(
    const DirectX::XMMATRIX& viewProj,
    Vec3 world,
    uint32_t width,
    uint32_t height,
    float& outX,
    float& outY) {
    if (width == 0u || height == 0u) {
        return false;
    }
    const DirectX::XMVECTOR p = DirectX::XMVectorSet(world.x, world.y, world.z, 1.0f);
    const DirectX::XMVECTOR ndc = DirectX::XMVector3TransformCoord(p, viewProj);
    const float x = DirectX::XMVectorGetX(ndc);
    const float y = DirectX::XMVectorGetY(ndc);
    const float z = DirectX::XMVectorGetZ(ndc);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || z < -0.02f || z > 1.06f) {
        return false;
    }
    outX = (x * 0.5f + 0.5f) * static_cast<float>(width);
    outY = (0.5f - y * 0.5f) * static_cast<float>(height);
    return true;
}

float WorldRadiusPixels(
    const DirectX::XMMATRIX& viewProj,
    Vec3 world,
    float worldRadius,
    uint32_t width,
    uint32_t height) {
    float cx = 0.0f;
    float cy = 0.0f;
    if (!ProjectWorldToScreen(viewProj, world, width, height, cx, cy)) {
        return 0.0f;
    }

    float radiusPx = 0.0f;
    auto measure = [&](Vec3 offset) {
        float sx = 0.0f;
        float sy = 0.0f;
        if (ProjectWorldToScreen(viewProj, Vec3{world.x + offset.x, world.y + offset.y, world.z + offset.z}, width, height, sx, sy)) {
            const float dx = sx - cx;
            const float dy = sy - cy;
            radiusPx = std::max(radiusPx, std::sqrt(dx * dx + dy * dy));
        }
    };
    measure(Vec3{worldRadius, 0.0f, 0.0f});
    measure(Vec3{0.0f, 0.0f, worldRadius});
    return Clamp(radiusPx, 3.0f, 140.0f);
}

float ScreenRotationForDirection(
    const DirectX::XMMATRIX& viewProj,
    Vec3 origin,
    Vec2 direction,
    uint32_t width,
    uint32_t height) {
    const Vec2 dir = NormalizeOr(direction, Vec2{1.0f, 0.0f});
    float ax = 0.0f;
    float ay = 0.0f;
    float bx = 0.0f;
    float by = 0.0f;
    if (ProjectWorldToScreen(viewProj, origin, width, height, ax, ay) &&
        ProjectWorldToScreen(viewProj, Vec3{origin.x + dir.x, origin.y, origin.z + dir.y}, width, height, bx, by)) {
        return std::atan2(by - ay, bx - ax);
    }
    return std::atan2(dir.y, dir.x);
}

void PushSprite(
    RenderScene& scene,
    float screenX,
    float screenY,
    float radiusPx,
    float rotation,
    Vec3 color,
    float alpha,
    uint32_t atlasIndex,
    uint32_t seed) {
    if (scene.spriteCount >= kMaxRenderSprites || radiusPx <= 0.5f || alpha <= 0.001f) {
        return;
    }
    RenderSprite& sprite = scene.sprites[scene.spriteCount++];
    sprite.positionSize[0] = screenX;
    sprite.positionSize[1] = screenY;
    sprite.positionSize[2] = Clamp(radiusPx, 1.0f, 220.0f);
    sprite.positionSize[3] = rotation;
    sprite.colorAlpha[0] = Clamp(color.x, 0.0f, 1.5f);
    sprite.colorAlpha[1] = Clamp(color.y, 0.0f, 1.5f);
    sprite.colorAlpha[2] = Clamp(color.z, 0.0f, 1.5f);
    sprite.colorAlpha[3] = Clamp(alpha, 0.0f, 1.0f);
    sprite.atlasIndex = atlasIndex & 15u;
    sprite.seed = seed;
}

void AddWorldSprite(
    RenderScene& scene,
    const DirectX::XMMATRIX& viewProj,
    Vec3 position,
    float worldRadius,
    Vec3 color,
    float alpha,
    uint32_t atlasIndex,
    uint32_t seed,
    float rotation,
    uint32_t displayWidth,
    uint32_t displayHeight) {
    float x = 0.0f;
    float y = 0.0f;
    if (!ProjectWorldToScreen(viewProj, position, displayWidth, displayHeight, x, y)) {
        return;
    }
    const float radiusPx = WorldRadiusPixels(viewProj, position, std::max(worldRadius, 0.04f), displayWidth, displayHeight);
    if (x < -radiusPx || y < -radiusPx ||
        x > static_cast<float>(displayWidth) + radiusPx ||
        y > static_cast<float>(displayHeight) + radiusPx) {
        return;
    }
    PushSprite(scene, x, y, radiusPx, rotation, color, alpha, atlasIndex, seed);
}

void AddActorStatusSprites(
    RenderScene& scene,
    const DirectX::XMMATRIX& viewProj,
    const ActorStatusSet& statuses,
    Vec2 position,
    float height,
    float timeSeconds,
    uint32_t frameIndex,
    uint32_t salt,
    uint32_t displayWidth,
    uint32_t displayHeight) {
    uint32_t localIndex = 0u;
    for (const StatusInstance& status : statuses.slots) {
        if (!status.active || status.kind == StatusKind::None || scene.spriteCount >= kMaxRenderSprites) {
            continue;
        }
        const Element element = StatusSpriteElement(status);
        const uint32_t seed = SpriteSeed(frameIndex, localIndex++, salt + static_cast<uint32_t>(status.kind) * 13u);
        const float phase = SpriteHash01(seed) * 6.2831853f + timeSeconds * (1.8f + SpriteHash01(seed >> 1u));
        const float orbit = 0.20f + SpriteHash01(seed >> 3u) * 0.18f;
        const Vec2 offset{std::cos(phase) * orbit, std::sin(phase) * orbit};
        AddWorldSprite(
            scene,
            viewProj,
            WorldFromGround(position + offset, height + 0.16f + SpriteHash01(seed >> 5u) * 0.20f),
            0.105f + status.intensity * 0.035f,
            ElementSpriteColor(element),
            Clamp(0.22f + status.intensity * 0.12f, 0.12f, 0.42f),
            AtlasForElement(element),
            seed,
            phase,
            displayWidth,
            displayHeight);
    }
}

void BuildRenderSprites(
    RenderScene& scene,
    const CombatSim& combat,
    std::span<const RenderVfxPulse> transientVfx,
    const DirectX::XMMATRIX& viewProj,
    float timeSeconds,
    uint32_t frameIndex,
    uint32_t displayWidth,
    uint32_t displayHeight) {
    scene.spriteCount = 0;
    const PlayerState& player = combat.Player();
    const int activeSlot = ActiveWeaponSlotIndex(player);

    const Vec2 facing = NormalizeOr(
        player.actionTimer > 0.0f ? player.activeActionDirection : player.facing,
        Vec2{1.0f, 0.0f});
    const WeaponSlot& slot = player.weaponSlots[activeSlot];
    const Element weaponElement = player.actionTimer > 0.0f ? player.activeActionElement : slot.element;
    const Vec3 weaponColor = ElementSpriteColor(weaponElement);
    const Vec3 weaponWorld = WorldFromGround(player.position + facing * 0.64f, 0.64f);
    const float weaponRotation = ScreenRotationForDirection(viewProj, weaponWorld, facing, displayWidth, displayHeight);
    AddWorldSprite(
        scene,
        viewProj,
        weaponWorld,
        0.28f,
        weaponColor,
        player.actionTimer > 0.0f ? 0.58f : 0.38f,
        AtlasForElement(weaponElement),
        SpriteSeed(frameIndex, 0u, 19u),
        weaponRotation,
        displayWidth,
        displayHeight);

    if (player.actionTimer > 0.0f && player.actionDuration > 0.0f) {
        const float elapsed = Clamp(player.actionDuration - player.actionTimer, 0.0f, player.actionDuration);
        const float progress = player.actionDuration > 0.0001f ? elapsed / player.actionDuration : 1.0f;
        const WeaponSpec& spec = GetWeaponSpec(player.activeActionWeapon);
        const WeaponActionSpec& action = spec.actions[static_cast<std::size_t>(
            player.activeActionIndex == WeaponActionIndex::Action2 ? 1 : 0)];
        const float impactPulse = 1.0f - Clamp(std::fabs(elapsed - player.actionImpactTime) / 0.22f, 0.0f, 1.0f);
        Vec2 actionCenter = player.activeActionOrigin + facing * std::min(action.range * 0.34f, 1.35f);
        if (player.activeActionShape == AttackShape::Dash || player.activeActionShape == AttackShape::TargetArea) {
            actionCenter = player.activeActionTarget;
        }
        float actionWorldRadius = Clamp(std::max(action.radius, 0.36f) * (0.38f + progress * 0.10f), 0.18f, 0.58f);
        float actionAlpha = Clamp(0.24f + impactPulse * 0.25f, 0.16f, 0.54f);
        switch (player.activeActionShape) {
        case AttackShape::Cone:
        case AttackShape::Dash:
        case AttackShape::Wave:
            actionWorldRadius = Clamp(std::max(action.radius, 0.36f) * (0.46f + progress * 0.14f), 0.22f, 0.86f);
            actionAlpha = Clamp(0.30f + impactPulse * 0.30f, 0.20f, 0.66f);
            break;
        case AttackShape::Projectile:
            actionWorldRadius = Clamp(std::max(action.radius, 0.24f) * 0.52f, 0.16f, 0.46f);
            actionAlpha = Clamp(0.28f + impactPulse * 0.24f, 0.18f, 0.56f);
            break;
        case AttackShape::TargetArea:
            actionWorldRadius = Clamp(std::max(action.radius, 0.30f) * 0.34f, 0.16f, 0.42f);
            actionAlpha = Clamp(0.22f + impactPulse * 0.22f, 0.14f, 0.48f);
            break;
        case AttackShape::Circle:
        case AttackShape::Burst:
        case AttackShape::Orbit:
            actionWorldRadius = Clamp(std::max(action.radius, 0.30f) * (0.18f + impactPulse * 0.06f), 0.12f, 0.32f);
            actionAlpha = Clamp(0.14f + impactPulse * 0.20f, 0.10f, 0.38f);
            break;
        }
        AddWorldSprite(
            scene,
            viewProj,
            WorldFromGround(actionCenter, 0.32f + impactPulse * 0.10f),
            actionWorldRadius,
            weaponColor,
            actionAlpha,
            AtlasForAction(player.activeActionShape, weaponElement),
            SpriteSeed(frameIndex, 1u, 23u),
            weaponRotation + progress * 1.25f,
            displayWidth,
            displayHeight);

        const Vec2 side{-facing.y, facing.x};
        for (uint32_t i = 0u; i < 2u; ++i) {
            const float sideSign = i == 0u ? -1.0f : 1.0f;
            const Vec2 motePos = player.position + facing * (0.42f + progress * 0.34f) + side * (0.18f * sideSign);
            AddWorldSprite(
                scene,
                viewProj,
                WorldFromGround(motePos, 0.72f + 0.08f * sideSign),
                0.11f,
                weaponColor,
                Clamp(0.25f + impactPulse * 0.18f, 0.18f, 0.48f),
                AtlasForElement(weaponElement),
                SpriteSeed(frameIndex, i + 2u, 29u),
                weaponRotation + sideSign * 0.65f,
                displayWidth,
                displayHeight);
        }
    }

    uint32_t pulseIndex = 0u;
    for (const RenderVfxPulse& pulse : transientVfx) {
        if (pulse.ttl <= 0.0f || pulse.duration <= 0.0001f || pulse.radius <= 0.0f || scene.spriteCount >= kMaxRenderSprites) {
            continue;
        }
        const float life = Clamp(pulse.ttl / pulse.duration, 0.0f, 1.0f);
        const float progress = 1.0f - life;
        const Vec2 direction{pulse.direction.x, pulse.direction.z};
        const float rotation = ScreenRotationForDirection(viewProj, pulse.position, direction, displayWidth, displayHeight);
        const Vec3 color = ColorForVfxKind(pulse.kind);
        const uint32_t seed = SpriteSeed(frameIndex, pulseIndex, 41u);
        AddWorldSprite(
            scene,
            viewProj,
            Vec3{pulse.position.x, pulse.position.y + progress * 0.14f, pulse.position.z},
            Clamp(pulse.radius * (0.50f + progress * 0.26f), 0.11f, 1.20f),
            color,
            Clamp((0.30f + pulse.intensity * 0.22f) * life, 0.0f, 0.72f),
            AtlasForVfxKind(pulse.kind),
            seed,
            rotation + progress * 0.85f,
            displayWidth,
            displayHeight);
        if (pulse.kind == RenderVfxKind::HitSpark) {
            const float jitter = SpriteHash01(seed) * 6.2831853f;
            AddWorldSprite(
                scene,
                viewProj,
                Vec3{
                    pulse.position.x + std::cos(jitter) * pulse.radius * 0.22f,
                    pulse.position.y + 0.12f,
                    pulse.position.z + std::sin(jitter) * pulse.radius * 0.22f
                },
                Clamp(pulse.radius * 0.18f, 0.07f, 0.28f),
                Vec3{0.92f, 0.98f, 1.0f},
                Clamp(0.28f * life * pulse.intensity, 0.0f, 0.42f),
                0u,
                seed ^ 0xb5297a4du,
                jitter,
                displayWidth,
                displayHeight);
        }
        ++pulseIndex;
    }

    uint32_t projectileIndex = 0u;
    for (const ProjectileState& projectile : combat.Projectiles()) {
        if (!projectile.active || scene.spriteCount >= kMaxRenderSprites) {
            continue;
        }
        const Vec2 dir = NormalizeOr(Vec2{projectile.velocity.x, projectile.velocity.y}, Vec2{1.0f, 0.0f});
        const Vec3 pos = WorldFromGround(projectile.position, 0.32f);
        AddWorldSprite(
            scene,
            viewProj,
            pos,
            Clamp(projectile.radius * 0.95f, 0.10f, 0.38f),
            ElementSpriteColor(projectile.element),
            0.36f,
            AtlasForElement(projectile.element),
            SpriteSeed(frameIndex, projectileIndex++, 53u),
            ScreenRotationForDirection(viewProj, pos, dir, displayWidth, displayHeight),
            displayWidth,
            displayHeight);
    }

    AddActorStatusSprites(scene, viewProj, player.statuses, player.position, 0.82f, timeSeconds, frameIndex, 67u, displayWidth, displayHeight);

    uint32_t enemyIndex = 0u;
    for (const EnemyState& enemy : combat.Enemies()) {
        if (!enemy.active || enemy.hp <= 0.0f || enemy.roomIndex != player.roomIndex || scene.spriteCount >= kMaxRenderSprites) {
            ++enemyIndex;
            continue;
        }
        if (enemy.actionTimer > 0.0f && enemy.actionDuration > 0.0f) {
            const Vec2 dir = NormalizeOr(enemy.activeActionDirection, NormalizeOr(player.position - enemy.position, Vec2{1.0f, 0.0f}));
            const Vec3 pos = WorldFromGround(enemy.position + dir * 0.54f, enemy.kind == EnemyKind::Boss ? 0.94f : 0.66f);
            const Element element = enemy.activeActionElement;
            AddWorldSprite(
                scene,
                viewProj,
                pos,
                enemy.kind == EnemyKind::Boss ? 0.40f : 0.25f,
                ElementSpriteColor(element),
                enemy.kind == EnemyKind::Boss ? 0.46f : 0.30f,
                AtlasForElement(element),
                SpriteSeed(frameIndex, enemyIndex, 79u),
                ScreenRotationForDirection(viewProj, pos, dir, displayWidth, displayHeight),
                displayWidth,
                displayHeight);
        }
        AddActorStatusSprites(
            scene,
            viewProj,
            enemy.statuses,
            enemy.position,
            enemy.kind == EnemyKind::Boss ? 1.05f : 0.72f,
            timeSeconds,
            frameIndex,
            97u + enemyIndex * 11u,
            displayWidth,
            displayHeight);
        ++enemyIndex;
    }
}

}

RenderScene BuildRenderScene(
    const RoomGraph& world,
    const CombatSim& combat,
    uint32_t outputWidth,
    uint32_t outputHeight,
    float timeSeconds,
    uint32_t frameIndex,
    std::span<const RenderVfxPulse> transientVfx,
    uint32_t runStatus,
    uint32_t displayWidth,
    uint32_t displayHeight,
    uint32_t renderQuality) {
    RenderScene scene{};
    scene.world = world;
    const PlayerState& player = combat.Player();
    const int activeSlot = ActiveWeaponSlotIndex(player);

    scene.visualStyle = BuildVisualStyle(world, player.roomIndex);
    scene.visualStylePacked = PackVisualStyle(scene.visualStyle);

    const DirectX::XMMATRIX viewProj = UpdateCamera(scene, world, combat, outputWidth, outputHeight);
    scene.frame.cameraPosition[0] = scene.camera.position.x;
    scene.frame.cameraPosition[1] = scene.camera.position.y;
    scene.frame.cameraPosition[2] = scene.camera.position.z;
    scene.frame.timeSeconds = timeSeconds;
    scene.frame.frameIndex = frameIndex;
    scene.frame.materialCount = kMaxDxrMaterials;
    scene.frame.outputWidth = outputWidth;
    scene.frame.outputHeight = outputHeight;
    scene.frame.displayWidth = displayWidth != 0u ? displayWidth : outputWidth;
    scene.frame.displayHeight = displayHeight != 0u ? displayHeight : outputHeight;
    scene.frame.visualStyleIdentity = scene.visualStylePacked.identity;
    scene.frame.visualStyleSurface = scene.visualStylePacked.surface;
    scene.frame.visualStyleAtmosphere = scene.visualStylePacked.atmosphere;
    scene.frame.visualStyleVariant = scene.visualStylePacked.variant;
    scene.frame.renderQuality = std::min(renderQuality, 2u);
    scene.overlay.overlayWeaponId = static_cast<uint32_t>(player.weaponSlots[activeSlot].weapon);
    scene.overlay.overlayElementId = static_cast<uint32_t>(player.weaponSlots[activeSlot].element);
    scene.overlay.overlayActiveSlot = static_cast<uint32_t>(activeSlot + 1);
    scene.overlay.overlayQReadyPercent = PercentReady(player, WeaponActionIndex::Action1);
    scene.overlay.overlayEReadyPercent = PercentReady(player, WeaponActionIndex::Action2);
    const WeaponSpec& activeWeapon = GetWeaponSpec(player.weaponSlots[activeSlot].weapon);
    scene.overlay.overlayQActionShape = static_cast<uint32_t>(activeWeapon.actions[0].shape);
    scene.overlay.overlayEActionShape = static_cast<uint32_t>(activeWeapon.actions[1].shape);
    scene.overlay.overlayLoadoutSlot0 = PackPlayerLoadoutSlot(player, 0, activeSlot);
    scene.overlay.overlayLoadoutSlot1 = PackPlayerLoadoutSlot(player, 1, activeSlot);
    scene.overlay.overlayLoadoutSlot2 = PackPlayerLoadoutSlot(player, 2, activeSlot);
    scene.overlay.overlayHp = static_cast<uint32_t>(std::lround(Clamp(player.hp, 0.0f, 999.0f)));
    scene.overlay.overlayCurrentRoom = static_cast<uint32_t>(player.roomIndex + 1);
    scene.overlay.overlayRoomCount = static_cast<uint32_t>(world.roomCount);
    scene.overlay.overlayActiveEnemies = static_cast<uint32_t>(combat.ActiveEnemiesInRoom(player.roomIndex));
    const BossOverlayInfo boss = BossOverlayForRoom(combat, player.roomIndex);
    scene.overlay.overlayBossHpPercent = boss.hpPercent;
    scene.overlay.overlayBossPhase = boss.phase;
    scene.overlay.overlayPlayerStatusMask = PlayerStatusMask(player);
    if (player.roomIndex >= 0 && player.roomIndex < world.roomCount) {
        scene.overlay.overlayObjectiveKind = static_cast<uint32_t>(world.rooms[player.roomIndex].objective.kind);
        scene.overlay.overlayObjectiveProgressPercent = ObjectiveProgressPercent(world, combat, player.roomIndex);
    }
    scene.overlay.overlayRunStatus = runStatus;
    scene.overlay.overlayFloorIndex = static_cast<uint32_t>(std::max(0, world.floorIndex));
    scene.overlay.overlayDescentPercent = static_cast<uint32_t>(std::lround(Clamp(world.descent, 0.0f, 1.0f) * 100.0f));
    BuildRenderSprites(
        scene,
        combat,
        transientVfx,
        viewProj,
        timeSeconds,
        frameIndex,
        scene.frame.displayWidth,
        scene.frame.displayHeight);

    scene.proxies = BuildEntityProxies(combat);
    std::vector<EntityRTProxy> vfxProxies = BuildVfxProxies(transientVfx);
    scene.proxies.insert(scene.proxies.end(), vfxProxies.begin(), vfxProxies.end());
    scene.generatedGeometry = GenerateWorldGeometry(world, &scene.visualStyle, player.roomIndex);
    GeneratedRTGeometry entityGeometry = GenerateRTGeometry(scene.proxies);
    scene.generatedGeometry.triangles.insert(
        scene.generatedGeometry.triangles.end(),
        entityGeometry.triangles.begin(),
        entityGeometry.triangles.end());
    scene.packedGeometry = PackRTGeometry(scene.generatedGeometry);
    scene.geometryHash = HashPackedRTGeometry(scene.packedGeometry);
    scene.materials = DefaultMaterials(scene.visualStyle);
    scene.materialCount = scene.frame.materialCount;
    return scene;
}

}

