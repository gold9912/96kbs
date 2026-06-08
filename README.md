# Rogue96DXR

Tiny procedural Windows roguelike prototype inspired by the technical discipline
of `.kkrieger`: code-generated world, materials, gameplay entities, and shaders,
with D3D12/DXR required from the first engine milestone.

## Current milestone

This repository contains the first implementation layer:

- MSVC/CMake project structure.
- Offline HLSL compilation through `dxc.exe`.
- Embedded DXIL blobs in the executable.
- Win32 + D3D12 bootstrap with mandatory DXR feature check.
- Minimal DXR state object scaffold for procedural entities.
- DXR data-flow contract for `RenderScene`, packed geometry, BLAS/TLAS resources,
  shader table layout, and `DispatchRays` bindings.
- First frame graph connection: world pass, DXR resource update, `DispatchRays`,
  and fullscreen composite of the DXR output.
- Deterministic room/portal world generator.
- Deterministic combat simulation for top-down shooter/slasher/spell gameplay.
- Procedural RT proxy geometry generation for future BLAS/TLAS builds.
- Fixed-capacity screen-space VFX sprite overlay with one embedded micro mask
  atlas for weapon-local glow and elemental hit pulses.
- Core simulation tests that do not require a GPU.

The renderer currently draws the procedural world pass through D3D12, updates
generated DXR scene resources, dispatches the ray generation shader, and
composites the DXR output texture over the world pass. The current gameplay
slice includes procedural rooms, portals, combat HUD, reward choice cards,
distinct enemy silhouettes, and a final-room boss proxy.

## Requirements

- Windows 10/11.
- Visual Studio 2022 Build Tools or newer.
- Windows SDK with D3D12 headers/libs.
- `dxc.exe` in `PATH`.
- D3D12 GPU with DXR support. There is intentionally no fallback path in v1.

## Build

Open a Developer PowerShell for Visual Studio:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\check_toolchain.ps1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --clean-first
ctest --test-dir build --output-on-failure
cmake --build build --target size_report
```

Or let the helper locate `VsDevCmd.bat` and run the Debug pass:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build_msvc_debug.ps1 -Clean
```

Use a clean rebuild after changing shared gameplay/render headers. This keeps
Ninja/MSVC object files in sync with structs such as `GameEvent`,
`GameSessionTickResult`, and `ActorStatusSet`.

Run:

```powershell
.\build\rogue96.exe
```

For hardware without DXR support, there is an explicit diagnostic mode:

```powershell
.\build\rogue96.exe --allow-no-dxr
```

This mode is only for local smoke testing. It keeps the Win32/D3D12/swapchain
and procedural world pass active, but skips DXR pipeline creation,
BLAS/TLAS updates, `DispatchRays`, and DXR composite. A normal run still
requires a DXR-capable GPU.

For automated smoke runs, append a frame budget and the app will close itself
after rendering that many frames:

```powershell
.\build\rogue96.exe --smoke-frames=60
.\build\rogue96.exe --smoke-combat-room=1 --smoke-frames=180
.\build\rogue96.exe --start-floor=5 --smoke-combat-room=1 --smoke-frames=180
.\build\rogue96.exe --allow-no-dxr --smoke-frames=60
```

The default window is `1920x1080`. Display and render scaling are separate. The window size controls input and HUD
scale, while `--render-scale=` controls the DXR output resolution before the
fullscreen composite:

```powershell
.\build\rogue96.exe --width=1920 --height=1080 --smoke-combat-room=1 --smoke-frames=180
.\build\rogue96.exe --width=2560 --height=1440 --render-scale=75 --fps-limit=120
.\build\rogue96.exe --width=3840 --height=2160 --render-scale=50 --fps-limit=0
```

`--fps-limit=0` disables the app-side frame cap. The default cap is `120`, and
the default render scale is `100`.

DXR quality is exposed as a small runtime tier so visual iteration can stay
inside the no-external-runtime-assets size budget while still offering a rich
default frame:

```powershell
.\build\rogue96.exe --rt-quality=0  # performance: direct DXR lighting
.\build\rogue96.exe --rt-quality=1  # reflections enabled
.\build\rogue96.exe --rt-quality=2  # default: reflections plus diffuse GI
```

## Controls

- `WASD`: move
- Mouse: aim
- `Q` or left mouse: primary weapon action
- `E` or right mouse: weapon ability
- `1`/`2`/`3`: choose reward or switch weapon slot
- `Space`: dash
- `Esc`: quit

## Size discipline

The prototype accepts `<15 MB` as the hard technology ceiling, with `<3 MB` as
the main optimization target. Release builds embed shader bytecode and one tiny
generated `R8_UNORM` VFX mask atlas, and do not load external art assets or
runtime shader compilers.

Visual references may live under `docs/visual_references/`, but they are
documentation only and are never loaded or embedded by the runtime.

## Renderer contract

See [docs/dxr_data_flow.md](docs/dxr_data_flow.md) for the fixed CPU-to-DXR
resource path and root signature binding layout.
