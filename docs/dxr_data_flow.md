# DXR Data Flow Contract

This file records the renderer contract before the first full MSVC/DXC compile
pass. It is intentionally small: the goal is to prevent architecture drift while
BLAS/TLAS and `DispatchRays` are wired into the frame.

## CPU frame path

`GameSession` owns progression events and `CombatSim` owns moment-to-moment
gameplay state. Every frame, `Application` ages a small transient VFX queue,
adds pulses from `GameSessionTickResult`, computes a deterministic room visual
style, and `BuildRenderScene` converts the current state into renderer data:

```text
GameSessionTickResult -> transient RenderVfxPulse[]
CombatSim + RoomGraph + transient RenderVfxPulse[]
  -> RoomVisualStyle + VisualStylePacked
  -> EntityRTProxy[]
  -> GeneratedRTGeometry
  -> PackedRTGeometry vertices/indices/triangle metadata
  -> RenderScene frame constants/materials/screen-space VFX sprites
```

The packed geometry format is the only accepted input for DXR upload. Room
floors, borders, portal markers, entity silhouettes, and short-lived VFX are all
generated procedurally. There are no external mesh files, texture files,
animation files, or asset database entries. The only runtime visual bitmap is a
single embedded generated `256x256 R8_UNORM` VFX mask atlas used by the
fullscreen composite for tiny snow/spark/droplet/flame/lightning/slash masks.

Enemy silhouettes are part of this contract: Brute, Caster, Skirmisher,
Bulwark, and Boss each map to a distinct `EntityProxyKind` and material id. The
default material table currently exposes 15 DXR materials, including VFX pulses
and the three additional enemy archetype materials.
Enemy combat readability markers also live in this same proxy stream:
`EnemyHealthBack`, `EnemyHealthFill`, `EnemyShieldFill`, and `EnemyStatusPip`
are generated procedurally from `EnemyState` and reuse the fixed material table
instead of adding textures, fonts, or mesh assets.
Enemy attack tells are generated from the same data-driven `EnemyAttackIntent`
used by combat pressure: ready in-range attacks emit procedural cone, line, or
ring proxies, while enemies outside attack range or early in cooldown emit no
persistent floor warning.

Player action readability is split between world geometry and a procedural
in-frame UI overlay. The world keeps only a small player-facing indicator plus
short-lived action/event VFX. Control objectives add a procedural floor marker
from `RoomObjective.controlPoint/controlRadius`, with no mesh or texture asset.
The active room also generates a compact visual style record: biome, palette,
floor pattern, future prop grammar, light rig, and packed weights for moss,
wetness, cracks, decay, corruption, glow, and fog. That style is deterministic
from `RoomGraph.seed` and current room state, then drives CPU material colors,
extra procedural floor detail, DXR lighting/material shading, the fullscreen
world pass, and HUD tint. `GenerateWorldGeometry` also consumes the active
style to add edge/corner prop grammars as RT triangles: columns, vine cascades,
bookshelf wall runs, candle clusters, crystals, and abyss spires. These props
are generated from code and style tokens only; no mesh or texture assets are
loaded.
When called from `BuildRenderScene`, the DXR world geometry focuses on the
active room as a single staged arena. The gameplay `RoomGraph` still contains
the full multi-room floor; this focus only controls what the current camera
renders so neighboring rooms do not dilute the reference-like combat framing.
Persistent Q/E footprints and cooldown rings are hidden so they do not compete
with combat readability. `RenderScene.overlay` publishes HUD values for the DXR
composite pass: `HP`, room/enemy counts, active weapon/element/slot, current
objective kind/progress, real-time `Q`/`E` readiness percentages/bars, and the
active `AttackShape` for each button. It also carries the current run status
(`RUN`, `DOWN`, or `CLEAR`) so terminal states remain visible in the same
shader-only HUD path as combat and rewards. In a final boss room it additionally
publishes compact boss phase and HP percent fields, so boss pressure is readable
without adding a separate UI system. The composite shader renders those shapes
as compact procedural glyphs next to the action names, so weapon behavior remains
readable without persistent world-space guides. It also publishes three packed
loadout-slot records for weapon keys `1`/`2`/`3`; each record carries weapon id,
element id, active state, and compact Q/E readiness so the HUD can show the whole
current loadout after reward swaps. It also publishes a compact player status mask
(`Wet`, `Burning`, `Charged`, `Chilled`) and a short-lived reaction flash id so
the composite shader can explain elemental combat without adding world-space
tutorial text. It also publishes a short-lived incoming damage flash percent
and source element id from `PlayerDamaged`, so enemy pressure is readable in
the HUD without persistent world-space rings. The UI is shader-only and split
into stable combat regions: vitals/status top-left, objective/floor/depth
top-center, boss phase/HP top-right, loadout bottom-left, and Q/E action cards
bottom-right. Glass panels, corner rails, element swatches, bitmap glyphs,
objective/status/reaction lines, action-shape badges, loadout slot cards, and
segmented cooldown bars are generated in `dxr_composite.hlsl`. This remains a
no-external-runtime-assets path: no loose project textures, font files, meshes,
or UI images are loaded, and the embedded VFX mask atlas is not used for HUD
art.
HUD coordinates use display dimensions, not DXR render dimensions, so
`--render-scale` can lower the raytraced output while the overlay remains
readable at 720p, 2K, and 4K.

Floor depth is also part of the render contract. `RoomGraph.floorIndex` and
`RoomGraph.descent` are published through `RenderScene.overlay` and packed style
constants. The world shader, entity material shader, and composite shader all use
those values to darken lower floors, increase fog/corruption, and keep the HUD
aware of the current floor without loading any floor-specific art.

Transient VFX are data-only pulses derived from gameplay events such as
`EnemyDamaged`, `RoomCompleted`, `RewardOffered`, `RewardSelected`,
`ReactionTriggered`, `PortalOpened`, `PlayerDamaged`, `PlayerActionUsed`, and
`PlayerAbilityUsed`. Player action events include the data-driven `AttackShape`,
so `Application` can map combat actions into short-lived procedural cone, line,
ring, or burst pulses. `PlayerDamaged` includes the source weapon/element payload
used by both the transient player hit spark and the composite damage flash. In
addition, `BuildRenderScene` derives a fixed-capacity `RenderSprite` list from
active weapon state, projectiles, statuses, and transient pulses. The composite
shader samples the embedded atlas for local glow masks; gameplay must not depend
on those presentation-only sprites.

While `GameSession` is in `RunPhase::RewardChoice`, `Application` appends three
procedural reward marker pulses to the frame VFX span and packs the three active
`RewardOption` records into composite root constants via
`PackRewardOverlayOption`. The packed option stores `kind`, `targetSlot`,
`element`, `weapon`, `upgrade`, and a compact `synergyElement` hint; `iconSeed`
remains for world/VFX placement.
The composite shader draws three reward cards for keyboard choices `1`/`2`/`3`
using the same procedural glyph and shape system as the combat HUD. Weapon-swap
cards include the offered weapon, element, and Q/E action names; infusion cards
name the target weapon plus the new element. Elemental reward cards also draw a
tiny `RX` link between the existing loadout element and the offered element, so
the player sees why a synergy-aware choice matters without adding text-heavy
tooltips. No reward icon, card, font, or texture asset exists on disk. After selection,
`RewardSelected` carries the selected option's weapon/element payload plus the
same packed reward option that fed the card. `Application` holds that packed
record briefly as a shader-only `GAIN` toast, so reward application remains
readable after the cards clear and without re-reading stale reward-card state.

## GPU resource path

`DxrSceneResources::Update(device, commandList, scene)` is the intended upload
entry point:

```text
PackedRTGeometry
  -> upload vertex/index buffers
  -> default vertex/index buffers
  -> upload sprite structured buffer
  -> ensure embedded VFX atlas texture
  -> BLAS build input
  -> one BLAS
  -> one TLAS instance
  -> TLAS
  -> descriptor heap
  -> DispatchRays output texture
  -> fullscreen composite over world pass + procedural UI overlay
```

The current first-pass TLAS uses one instance that points at the generated BLAS.
Per-entity instances can be added later if material indexing or transform reuse
becomes more important than keeping the implementation compact.

## Shader table

The shader table has exactly three records and no local root data:

```text
RayGen
Miss
EntityHitGroup
```

All records contain only the shader identifier. This keeps the table small and
makes the first `DispatchRays` pass deterministic.

## Global root signature

The global DXR root signature is fixed to:

```text
root 0: descriptor table UAV u0        RWTexture2D<float4> gOutput
root 1: descriptor table SRV t0-t2     TLAS + material buffer + triangle metadata
root 2: root CBV b0                    RayFrameConstants
```

HLSL declarations must stay aligned with this layout:

```hlsl
RWTexture2D<float4> gOutput : register(u0);
RaytracingAccelerationStructure gScene : register(t0);
StructuredBuffer<EntityMaterial> gMaterials : register(t1);
StructuredBuffer<TriangleMetadata> gTriangles : register(t2);
ConstantBuffer<RayFrameConstants> gFrame : register(b0);
```

`RayFrameConstants` also carries `displayWidth/displayHeight` plus the four
packed visual style fields. `outputWidth/outputHeight` describe the DXR dispatch
resolution; display dimensions describe the swapchain/HUD resolution.

## Dispatch contract

`DxrPipeline::Dispatch(commandList, sceneResources, width, height)` binds the
descriptor heap, root signature, root parameters, state object, shader table,
and then calls `DispatchRays`.

## Descriptor heap layout

`DxrSceneResources` owns one shader-visible `CBV_SRV_UAV` heap:

```text
0: UAV u0    DXR output texture
1: SRV t0    TLAS
2: SRV t1    material buffer
3: SRV t2    triangle metadata buffer
4: SRV t0    DXR output texture for raster composite
5: SRV t1    RenderSprite structured buffer for raster composite
6: SRV t2    embedded R8 VFX atlas texture for raster composite
```

The DXR root signature still sees descriptors 0-3. The raster composite root
signature starts at descriptor 4 and exposes `gDxrOutput`, `gSprites`, and
`gVfxAtlas` as SRVs `t0-t2`.

The raster world pass remains separate. The current frame order is:

```text
transition backbuffer present -> render target
draw procedural world fullscreen pass
update DXR scene resources
DispatchRays into DXR output UAV
transition DXR output UAV -> pixel shader resource
draw fullscreen composite pass as the final frame
transition backbuffer render target -> present
```

## Reflection/PT Foundation

The DXR pipeline allows two ray recursion levels. The primary ray shades
procedural room and entity geometry; the closest-hit shader may launch one
secondary reflection ray for wet floors, portals, and emissive anchors. This is
the compact foundation for path-traced-feeling reflections without texture,
mesh, or material asset files. Future PT work should extend this through
temporal accumulation and deterministic shader sampling while preserving the
15 MB shipped build target.

## Final-Frame Postprocess

`dxr_composite.hlsl` owns the final procedural image treatment before drawing
the HUD: bloom from bright DXR samples, biome color grading, filmic tone map,
soft fog, vignette, and lightweight light shafts. The composite pass writes the
full backbuffer instead of alpha-blending over the raster world pass, so the DXR
scene is the visual source of truth and the raster world remains a fallback and
diagnostic layer.
