# DXR Data Flow Contract

This file records the renderer contract before the first full MSVC/DXC compile
pass. It is intentionally small: the goal is to prevent architecture drift while
BLAS/TLAS and `DispatchRays` are wired into the frame.

## CPU frame path

`CombatSim` owns gameplay state. Every frame, `BuildRenderScene` converts it into
renderer data:

```text
CombatSim + RoomGraph
  -> EntityRTProxy[]
  -> GeneratedRTGeometry
  -> PackedRTGeometry vertices/indices/triangle metadata
  -> RenderScene frame constants/materials
```

The packed geometry format is the only accepted input for DXR upload. Room
floors, borders, portal markers, entity silhouettes, and short-lived VFX are all
generated procedurally. There are no mesh files, texture files, animation files,
or asset database entries.

## GPU resource path

`DxrSceneResources::Update(device, commandList, scene)` is the intended upload
entry point:

```text
PackedRTGeometry
  -> upload vertex/index buffers
  -> default vertex/index buffers
  -> BLAS build input
  -> one BLAS
  -> one TLAS instance
  -> TLAS
  -> descriptor heap
  -> DispatchRays output texture
  -> fullscreen composite over world pass
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
```

The raster world pass remains separate. The current frame order is:

```text
transition backbuffer present -> render target
draw procedural world fullscreen pass
update DXR scene resources
DispatchRays into DXR output UAV
transition DXR output UAV -> pixel shader resource
draw fullscreen composite pass over world
transition backbuffer render target -> present
```
