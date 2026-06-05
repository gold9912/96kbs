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
- Core simulation tests that do not require a GPU.

The renderer currently draws the procedural world pass through D3D12, updates
generated DXR scene resources, dispatches the ray generation shader, and
composites the DXR output texture over the world pass. The next implementation
step is validating this path under MSVC/DXC and tightening any D3D12 validation
errors reported by the debug layer.

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
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target size_report
```

Or let the helper locate `VsDevCmd.bat` and run the Debug pass:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build_msvc_debug.ps1
```

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

## Controls

- `WASD`: move
- Mouse: aim
- Left mouse: melee slash
- Right mouse: ranged spell
- `Q`: area/control spell
- `Space`: dash
- `Esc`: quit

## Size discipline

The prototype accepts `<15 MB` as the hard technology ceiling, with `<3 MB` as
the main optimization target. Release builds embed shader bytecode and do not
load external art assets or runtime shader compilers.

## Renderer contract

See [docs/dxr_data_flow.md](docs/dxr_data_flow.md) for the fixed CPU-to-DXR
resource path and root signature binding layout.
