# Procedural Visual Design System

This document fixes the visual target for the no-external-runtime-assets
renderer. The game may keep reference images in this folder for direction, but
runtime builds must not load images, loose textures, mesh files, font files,
JSON art databases, or other external visual assets. The shipped visual
language is generated from code, shaders, seeds, compact numeric style tokens,
and one embedded micro `R8_UNORM` VFX mask atlas.

## References

Reference images are stored as documentation only:

- `docs/visual_references/ref_01_sunlit_ruins.png`: warm ruined sanctuary,
  broken stone, plants, bright window light, cyan katana VFX.
- `docs/visual_references/ref_02_abyss_throne.png`: red/violet gothic arena,
  wet stone, ritual throne, heavy emissive corruption.
- `docs/visual_references/ref_03_overgrown_sanctuary.png`: readable hybrid
  combat room with water channels, dense overgrowth, warm sunlight, blue lamps.
- `docs/visual_references/ref_04_arcane_library.png`: darker library/chapel
  space, candles, bookshelves, statues, circular floor ornament.
- `docs/visual_references/ref_05_blue_crypt.png`: cold blue crypt, fog, wet
  floor, cyan/purple magic, compact enemy readability.

These references are a hard visual target for camera angle, arena framing,
environment density, material contrast, lighting language, reflections, VFX
readability, and final-frame postprocessing. The acceptance target is at least
80% visual match to the references, with 100% as the ideal direction. Runtime
code must not copy pixels or ship the reference assets; it must reproduce the
same design parameters procedurally from compact rules.

## Reference Target Preset

The canonical local capture for this visual target is:

```powershell
.\build\rogue96.exe --reference-target=1
```

The preset means floor `0`, combat room `1`, `1920x1080`, render scale `100`,
DXR quality `5`, `180` smoke frames, and
`artifacts\reference_target.bmp` as the default capture path. Acceptance runs
should pair it with:

```powershell
ctest --test-dir build --output-on-failure
cmake --build build --target size_report
```

The capture is documentation output, not an asset source. The runtime remains
fully procedural.

## Visual Pillars

- Isometric clarity first: silhouettes, attack arcs, enemy tells, HP/status
  marks, and reward choices must remain readable before decoration.
- Reference-grade arena framing: the camera targets the active room as a staged
  combat arena, with visible floor pattern, walls, props, and edge lighting
  around a clean combat center.
- Hybrid sacred-arcane mood: sunlit ruin rooms and dark crypt rooms share one
  material grammar, so the run feels cohesive while rooms vary strongly.
- Procedural richness from rules: stone patterns, cracks, wetness, moss,
  corruption, glow, fog, and UI tint are generated from seed-driven tokens.
- Small data, large variation: one packed style record should drive CPU
  materials, DXR shading, fullscreen world color, and HUD tint.
- Final-frame polish is procedural too: bloom, tone mapping, vignette, color
  grade, soft fog, light shafts, and tiny atlas-masked VFX sprites are shader
  code plus embedded data, not loose runtime assets.

## Biomes

`SunlitRuins`
: Warm stone, low corruption, moderate cracks, visible moss, golden light and
cyan action contrast.

`OvergrownSanctuary`
: Strong green overgrowth, water/wetness hints, warm window light, blue lamps,
and softer danger contrast.

`ArcaneLibrary`
: Dark wood/stone impression through color and floor marks, candle gold,
violet magic, circular/rune floor motifs.

`AbyssCrypt`
: Black stone, red cracks, violet/cyan emissive accents, heavier fog, high
corruption, wet reflective floor cues.

## Runtime Style Tokens

Each active room receives a deterministic `RoomVisualStyle`:

- `biome`: one of the four visual biomes.
- `paletteId`: small variation index for colors.
- `floorPatternId`: grid, cracked slab, circular/rune, or wet crypt pattern.
- `propGrammarId`: active grammar selector for columns, vines, candles,
  crystals, bookshelves, and abyss spires.
- `lightRigId`: warm window, blue lamp, candle/violet, or red crypt lighting.
- `moss`, `wetness`, `cracks`, `decay`, `corruption`, `glow`, `fog`: weights in
  `[0, 1]`, packed to nibbles before reaching shaders.

`ShotLayout` stages the active room on top of `RoomVisualStyle`:

- `keyLightSide`, `windowFrame`, `foregroundMask`: compact identity nibbles
  for top/right light language and foreground corner treatment.
- `combatClearRadius`: keeps the middle of the arena open for player movement,
  enemies, tells, and slash VFX.
- `edgeDensity`, `foliageDensity`: push prop and foliage richness to perimeter
  and corners.
- `heroVfxBias`, `warmCoolContrast`: reserve brightness and bloom response for
  the player slash while keeping warm window light and cool shadows coherent.

`ShotLayoutPacked` travels with frame constants and overlay constants so world
geometry, DXR shading, and fullscreen composite share one staging decision.

## Material Grammar

- Stone is the base surface everywhere. Variation comes from generated grids,
  slab masks, cracks, edge lines, and biome palette shifts.
- `RtTriangleMetadata.styleTag` stores compact surface role, element, surface
  angle, and emissive hints without increasing `kMaxDxrMaterials`.
- Moss and overgrowth tint floor edges and quiet corners; this must never cover
  enemy tells or player attack VFX.
- Wetness adds cool highlights and darker floor patches rather than a texture.
- Corruption adds red/violet cracks and emissive pulse, strongest in final and
  crypt-like rooms.
- Glow is reserved for readable gameplay and small environmental anchors:
  portals, lamps, runes, reward pips, and active objectives.

## Procedural Props

- Props are generated as compact RT geometry from biome style tokens, never
  loaded from meshes or textures.
- `SunlitRuins`: broken square columns, low moss/vine strips, warm rail/light
  streaks, and small sanctuary anchors near room edges.
- `OvergrownSanctuary`: heavier vine cascades, green patches, low columns, and
  blue water/crystal accents.
- `ArcaneLibrary`: bookshelf wall runs, candle clusters, ritual crystals, and
  circular/rune floor ornaments.
- `AbyssCrypt`: tall black/red spires, corrupted rings, violet/blue crystals,
  and stronger danger rails.
- Props stay along room edges and corners so the arena center remains readable
  for movement, attack shapes, enemy tells, and loot choices.
- Every active visual style adds a procedural arena scaffold: slab grid,
  inset floor frames, raised wall panels, columns, and biome-specific light
  anchors. This is the baseline that moves the scene toward the reference
  screenshots without shipping textures or meshes.

## Silhouettes And VFX

- Player: katana-first cyan core, blade/readability accent, mask, shoulders,
  cloak/armor hints, and sharp compact silhouette.
- Brute: broad heavy body and weapon; Caster: floating magic silhouette;
  Skirmisher/skeleton: small fast bony form with blade accent; Bulwark: blocky
  shielded armor; Boss: stacked ritual armor and larger emissive silhouette.
- Player action VFX keeps cyan as the primary readability color, with element
  tint only as a secondary accent.
- Character poses use a procedural `windup -> impact -> recovery` timeline,
  with idle bob, locomotion lean, weapon sway, squash/stretch, hit reaction,
  caster floating, and death collapse/dissolve layered through generated
  geometry and transient VFX.
- Katana attacks should read as a backward windup, hard impact crescent,
  recovery overshoot, afterimage/streak, floor glow, and small particles.
- Enemy threat tells use element color but stay thinner and more restrained than
  player attack feedback.
- Death and kill beats should emit collapse, burst motes, and floor ripples
  without competing with the active player slash.
- Weapon-local glow and elemental hit pulses use a fixed-capacity `RenderSprite`
  list and the embedded micro atlas for snowflakes, droplets, flame, lightning,
  sparks, slashes, rings, and wisps. These sprites must stay small and local to
  weapons, statuses, projectiles, or short-lived combat events.

The fixed visual budgets are `kMaxRenderSprites = 128` and
`kMaxRenderVfxPulses = 48`. Overflow is a test failure; effects must degrade by
priority or compactness, not by unbounded allocation or runtime asset loads.

## HUD

- HUD remains shader-only: no font assets, icons, or UI textures.
- The current 5x7 procedural type and compact cards stay as the base language.
- The panel tint follows the active biome, but HP, Q, E, slot selection, status,
  reaction, and reward colors remain stable enough for muscle memory.
- Combat HUD is split across the screen instead of one debug block: HP/status
  lives top-left, objective/floor/depth top-center, boss pressure top-right,
  loadout bottom-left, and Q/E action/cooldown cards bottom-right.
- HUD coordinates scale from display size, not DXR render size, so render scale
  can change without shrinking UI on 2K/4K displays.

## Floor Descent

- `RoomGraph.floorIndex` and `RoomGraph.descent` drive deeper mood over a run:
  early floors prefer friendly sunlit/overgrown rooms, while lower floors shift
  toward arcane libraries and abyss crypts.
- Early floors avoid `AbyssCrypt` except for explicit final-room pressure, so
  the first layer reads as brighter ruins/sanctuary and the descent earns its
  darker hell-like tone.
- Descent increases corruption, fog, cracks, wetness, and vignette strength
  while reducing friendly moss so the game grows darker without loading assets.
- Floor transitions preserve player build progress but reset short combat
  transients. The next floor is a fresh procedural space with the same compact
  runtime data contract.

## Size Contract

The release executable must stay under 15 MB. Reference PNG files can live in
the repository for art direction, but they are not runtime assets and must not
be embedded, copied to release output, or loaded by the game. The embedded VFX
atlas budget is under 100 KB raw data, with the current generated atlas at
`256x256` single-channel bytes before executable/link compression.
