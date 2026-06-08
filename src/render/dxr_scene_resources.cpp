#include "render/dxr_scene_resources.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace rogue {

namespace {

constexpr UINT64 AlignUp(UINT64 value, UINT64 alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

constexpr uint32_t kVfxAtlasSize = 256;
constexpr uint32_t kVfxAtlasTileCount = 4;
constexpr uint32_t kVfxAtlasTileSize = kVfxAtlasSize / kVfxAtlasTileCount;
constexpr uint32_t kDescriptorCompositeSrvBase = 4;
constexpr uint32_t kDescriptorSpriteSrv = 5;
constexpr uint32_t kDescriptorVfxAtlasSrv = 6;

D3D12_RESOURCE_DESC BufferDesc(UINT64 size, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;
    return desc;
}

D3D12_RESOURCE_DESC Texture2DDesc(uint32_t width, uint32_t height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;
    return desc;
}

D3D12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = type;
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    return heap;
}

bool CreateDefaultTexture(
    ID3D12Device* device,
    const D3D12_RESOURCE_DESC& desc,
    D3D12_RESOURCE_STATES initialState,
    Microsoft::WRL::ComPtr<ID3D12Resource>& out) {
    const D3D12_HEAP_PROPERTIES heap = HeapProps(D3D12_HEAP_TYPE_DEFAULT);
    return SUCCEEDED(device->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&out)));
}

float Clamp01(float value) {
    return std::max(0.0f, std::min(value, 1.0f));
}

float SmoothStep(float edge0, float edge1, float value) {
    const float t = Clamp01((value - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

float AtlasHash(float x, float y, float seed) {
    const float h = std::sin(x * 12.9898f + y * 78.233f + seed * 37.719f) * 43758.5453f;
    return h - std::floor(h);
}

float TileMask(uint32_t tileIndex, float x, float y) {
    const float dx = x - 0.5f;
    const float dy = y - 0.5f;
    const float d = std::sqrt(dx * dx + dy * dy);
    const float angle = std::atan2(dy, dx);
    switch (tileIndex) {
    case 0: {
        const float core = 1.0f - SmoothStep(0.05f, 0.45f, d);
        const float star = std::pow(std::max(0.0f, std::cos(angle * 4.0f)), 8.0f) * (1.0f - SmoothStep(0.12f, 0.50f, d));
        return Clamp01(core + star * 0.70f);
    }
    case 1: {
        const float droplet = 1.0f - SmoothStep(0.12f, 0.40f, std::sqrt(dx * dx * 1.35f + (dy + 0.05f) * (dy + 0.05f) * 0.72f));
        const float tail = SmoothStep(0.42f, 0.10f, std::abs(dx)) * SmoothStep(0.54f, 0.18f, y);
        return Clamp01(droplet + tail * 0.28f);
    }
    case 2: {
        const float flameBody = 1.0f - SmoothStep(0.05f, 0.38f, std::sqrt(dx * dx * (1.3f + y) + (dy + 0.16f) * (dy + 0.16f) * 0.60f));
        const float lick = std::pow(std::max(0.0f, std::sin((x + y * 0.45f) * 17.0f)), 4.0f) * SmoothStep(0.96f, 0.12f, y);
        return Clamp01(flameBody + lick * 0.22f);
    }
    case 3: {
        const float bolt = std::abs(dx + std::sin(y * 18.0f) * 0.075f + (y - 0.5f) * 0.20f);
        return Clamp01((1.0f - SmoothStep(0.015f, 0.105f, bolt)) * (1.0f - SmoothStep(0.46f, 0.58f, std::abs(dy))));
    }
    case 4: {
        float snow = 1.0f - SmoothStep(0.015f, 0.055f, std::abs(dx));
        snow = std::max(snow, 1.0f - SmoothStep(0.015f, 0.055f, std::abs(dy)));
        snow = std::max(snow, 1.0f - SmoothStep(0.014f, 0.050f, std::abs(dx - dy)));
        snow = std::max(snow, 1.0f - SmoothStep(0.014f, 0.050f, std::abs(dx + dy)));
        return Clamp01(snow * (1.0f - SmoothStep(0.20f, 0.48f, d)));
    }
    case 5: {
        const float arc = std::abs(d - (0.36f + std::sin(angle * 1.8f) * 0.025f));
        const float front = SmoothStep(-0.40f, 0.45f, dx) * SmoothStep(0.90f, -0.34f, dx);
        return Clamp01((1.0f - SmoothStep(0.015f, 0.075f, arc)) * front * SmoothStep(0.52f, 0.02f, std::abs(dy)));
    }
    case 6: {
        const float orb = 1.0f - SmoothStep(0.10f, 0.46f, d);
        const float ring = 1.0f - SmoothStep(0.014f, 0.070f, std::abs(d - 0.32f));
        return Clamp01(orb * 0.55f + ring * 0.65f);
    }
    case 7: {
        const float smoke = 1.0f - SmoothStep(0.08f, 0.45f, d + std::sin(angle * 3.0f) * 0.035f);
        return Clamp01(smoke * (0.70f + 0.20f * std::sin((x + y) * 12.0f)));
    }
    default:
        return Clamp01(1.0f - SmoothStep(0.10f, 0.42f, d));
    }
}

std::array<uint8_t, kVfxAtlasSize * kVfxAtlasSize> BuildVfxAtlas() {
    std::array<uint8_t, kVfxAtlasSize * kVfxAtlasSize> data{};
    for (uint32_t y = 0; y < kVfxAtlasSize; ++y) {
        for (uint32_t x = 0; x < kVfxAtlasSize; ++x) {
            const uint32_t tileX = x / kVfxAtlasTileSize;
            const uint32_t tileY = y / kVfxAtlasTileSize;
            const uint32_t tile = tileY * kVfxAtlasTileCount + tileX;
            const float u = (static_cast<float>(x % kVfxAtlasTileSize) + 0.5f) / static_cast<float>(kVfxAtlasTileSize);
            const float v = (static_cast<float>(y % kVfxAtlasTileSize) + 0.5f) / static_cast<float>(kVfxAtlasTileSize);
            const float mask = Clamp01(TileMask(tile, u, v) * (0.94f + AtlasHash(static_cast<float>(x), static_cast<float>(y), static_cast<float>(tile)) * 0.06f));
            data[y * kVfxAtlasSize + x] = static_cast<uint8_t>(std::round(mask * 255.0f));
        }
    }
    return data;
}

bool CreateDefaultBuffer(
    ID3D12Device* device,
    UINT64 size,
    D3D12_RESOURCE_STATES initialState,
    D3D12_RESOURCE_FLAGS flags,
    Microsoft::WRL::ComPtr<ID3D12Resource>& out) {
    const D3D12_HEAP_PROPERTIES heap = HeapProps(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_RESOURCE_DESC desc = BufferDesc(size, flags);
    return SUCCEEDED(device->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&out)));
}

void Transition(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) {
    if (!commandList || !resource || before == after) {
        return;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
}

void UavBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource;
    commandList->ResourceBarrier(1, &barrier);
}

}

bool DxrSceneResources::Update(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, const RenderScene& scene) {
    if (!EnsureDescriptorHeap(device) ||
        !EnsureOutput(device, scene.frame.outputWidth, scene.frame.outputHeight) ||
        !EnsureVfxAtlas(device, commandList)) {
        return false;
    }
    TransitionOutput(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    if (!UploadFrameData(device, scene) || !UploadSprites(device, scene)) {
        return false;
    }
    return Update(device, commandList, scene.packedGeometry);
}

bool DxrSceneResources::Update(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, const PackedRTGeometry& geometry) {
    if (!device || !commandList || geometry.vertices.empty() || geometry.indices.empty() || geometry.triangleMetadata.empty()) {
        return false;
    }
    if (!EnsureDescriptorHeap(device)) {
        return false;
    }
    if (!UploadGeometry(device, commandList, geometry)) {
        return false;
    }
    if (!BuildAccelerationStructures(device, commandList)) {
        return false;
    }
    WriteDescriptors(device);
    return true;
}

void DxrSceneResources::Reset() {
    descriptorHeap_.Reset();
    output_.Reset();
    vertexBuffer_.Reset();
    indexBuffer_.Reset();
    blasScratch_.Reset();
    blasResult_.Reset();
    tlasScratch_.Reset();
    tlasResult_.Reset();
    vfxAtlas_.Reset();
    vertexUpload_.Reset();
    indexUpload_.Reset();
    instanceUpload_.Reset();
    frameConstants_.Reset();
    materialUpload_.Reset();
    triangleMetadataUpload_.Reset();
    spriteUpload_.Reset();
    vfxAtlasUpload_.Reset();
    geometryDesc_ = {};
    descriptorSize_ = 0;
    outputWidth_ = 0;
    outputHeight_ = 0;
    materialCount_ = 0;
    spriteCount_ = 0;
    outputState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    stats_ = {};
}

D3D12_GPU_DESCRIPTOR_HANDLE DxrSceneResources::OutputUavGpu() const {
    return descriptorHeap_ ? descriptorHeap_->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

D3D12_GPU_DESCRIPTOR_HANDLE DxrSceneResources::SrvTableGpu() const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = OutputUavGpu();
    handle.ptr += descriptorSize_;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DxrSceneResources::OutputSrvGpu() const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = OutputUavGpu();
    handle.ptr += descriptorSize_ * kDescriptorCompositeSrvBase;
    return handle;
}

D3D12_GPU_VIRTUAL_ADDRESS DxrSceneResources::TlasGpuAddress() const {
    return tlasResult_ ? tlasResult_->GetGPUVirtualAddress() : 0;
}

void DxrSceneResources::TransitionOutput(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES targetState) {
    if (!output_ || !commandList || outputState_ == targetState) {
        return;
    }
    Transition(commandList, output_.Get(), outputState_, targetState);
    outputState_ = targetState;
}

void DxrSceneResources::InsertOutputUavBarrier(ID3D12GraphicsCommandList* commandList) const {
    if (output_ && commandList) {
        UavBarrier(commandList, output_.Get());
    }
}

bool DxrSceneResources::EnsureDescriptorHeap(ID3D12Device5* device) {
    if (!device) {
        return false;
    }
    if (descriptorHeap_) {
        return true;
    }

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 7;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap_)))) {
        return false;
    }
    descriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return true;
}

bool DxrSceneResources::EnsureOutput(ID3D12Device5* device, uint32_t width, uint32_t height) {
    if (!device || width == 0 || height == 0) {
        return false;
    }
    if (output_ && outputWidth_ == width && outputHeight_ == height) {
        return true;
    }

    output_.Reset();
    D3D12_HEAP_PROPERTIES heap = HeapProps(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (FAILED(device->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&output_)))) {
        return false;
    }
    outputWidth_ = width;
    outputHeight_ = height;
    outputState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    return true;
}

bool DxrSceneResources::EnsureVfxAtlas(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList) {
    if (!device || !commandList) {
        return false;
    }
    if (vfxAtlas_) {
        return true;
    }

    const D3D12_RESOURCE_DESC textureDesc = Texture2DDesc(kVfxAtlasSize, kVfxAtlasSize, DXGI_FORMAT_R8_UNORM);
    if (!CreateDefaultTexture(device, textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, vfxAtlas_)) {
        return false;
    }

    const auto atlas = BuildVfxAtlas();
    const UINT64 rowPitch = AlignUp(kVfxAtlasSize, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    std::vector<uint8_t> padded(static_cast<std::size_t>(rowPitch) * kVfxAtlasSize);
    for (uint32_t y = 0; y < kVfxAtlasSize; ++y) {
        std::memcpy(
            padded.data() + static_cast<std::size_t>(rowPitch) * y,
            atlas.data() + static_cast<std::size_t>(kVfxAtlasSize) * y,
            kVfxAtlasSize);
    }
    if (!vfxAtlasUpload_.Create(device, padded.data(), padded.size())) {
        return false;
    }

    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = vfxAtlas_.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = vfxAtlasUpload_.Resource();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = 0;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8_UNORM;
    src.PlacedFootprint.Footprint.Width = kVfxAtlasSize;
    src.PlacedFootprint.Footprint.Height = kVfxAtlasSize;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(rowPitch);

    commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    Transition(commandList, vfxAtlas_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return true;
}

bool DxrSceneResources::UploadGeometry(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, const PackedRTGeometry& geometry) {
    stats_.vertexCount = static_cast<uint32_t>(geometry.vertices.size());
    stats_.indexCount = static_cast<uint32_t>(geometry.indices.size());
    stats_.triangleCount = stats_.indexCount / 3u;
    stats_.geometryHash = HashPackedRTGeometry(geometry);

    const UINT64 vertexBytes = static_cast<UINT64>(geometry.vertices.size() * sizeof(PackedRtVertex));
    const UINT64 indexBytes = static_cast<UINT64>(geometry.indices.size() * sizeof(uint32_t));
    if (!vertexUpload_.Create(device, geometry.vertices.data(), static_cast<std::size_t>(vertexBytes))) {
        return false;
    }
    if (!indexUpload_.Create(device, geometry.indices.data(), static_cast<std::size_t>(indexBytes))) {
        return false;
    }
    if (!triangleMetadataUpload_.Create(
            device,
            geometry.triangleMetadata.data(),
            geometry.triangleMetadata.size() * sizeof(RtTriangleMetadata))) {
        return false;
    }

    vertexBuffer_.Reset();
    indexBuffer_.Reset();
    if (!CreateDefaultBuffer(device, vertexBytes, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE, vertexBuffer_)) {
        return false;
    }
    if (!CreateDefaultBuffer(device, indexBytes, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE, indexBuffer_)) {
        return false;
    }

    commandList->CopyBufferRegion(vertexBuffer_.Get(), 0, vertexUpload_.Resource(), 0, vertexBytes);
    commandList->CopyBufferRegion(indexBuffer_.Get(), 0, indexUpload_.Resource(), 0, indexBytes);
    Transition(commandList, vertexBuffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Transition(commandList, indexBuffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    geometryDesc_ = {};
    geometryDesc_.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc_.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    geometryDesc_.Triangles.VertexBuffer.StartAddress = vertexBuffer_->GetGPUVirtualAddress();
    geometryDesc_.Triangles.VertexBuffer.StrideInBytes = sizeof(PackedRtVertex);
    geometryDesc_.Triangles.VertexCount = stats_.vertexCount;
    geometryDesc_.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc_.Triangles.IndexBuffer = indexBuffer_->GetGPUVirtualAddress();
    geometryDesc_.Triangles.IndexCount = stats_.indexCount;
    geometryDesc_.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    return true;
}

bool DxrSceneResources::UploadFrameData(ID3D12Device5* device, const RenderScene& scene) {
    materialCount_ = std::min(scene.materialCount, kMaxDxrMaterials);
    if (!frameConstants_.Create(device, &scene.frame, sizeof(scene.frame))) {
        return false;
    }
    return materialUpload_.Create(
        device,
        scene.materials.data(),
        static_cast<std::size_t>(materialCount_ * sizeof(EntityMaterial)));
}

bool DxrSceneResources::UploadSprites(ID3D12Device5* device, const RenderScene& scene) {
    spriteCount_ = std::min(scene.spriteCount, kMaxRenderSprites);
    if (spriteCount_ == 0) {
        const RenderSprite empty{};
        return spriteUpload_.Create(device, &empty, sizeof(empty));
    }
    return spriteUpload_.Create(
        device,
        scene.sprites.data(),
        static_cast<std::size_t>(spriteCount_ * sizeof(RenderSprite)));
}

bool DxrSceneResources::BuildAccelerationStructures(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList) {
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs{};
    blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    blasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    blasInputs.NumDescs = 1;
    blasInputs.pGeometryDescs = &geometryDesc_;
    blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasInfo{};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasInfo);
    if (blasInfo.ResultDataMaxSizeInBytes == 0) {
        return false;
    }

    const UINT64 blasScratchBytes = AlignUp(blasInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
    const UINT64 blasResultBytes = AlignUp(blasInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
    if (!CreateDefaultBuffer(device, blasScratchBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, blasScratch_)) {
        return false;
    }
    if (!CreateDefaultBuffer(device, blasResultBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, blasResult_)) {
        return false;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuild{};
    blasBuild.Inputs = blasInputs;
    blasBuild.ScratchAccelerationStructureData = blasScratch_->GetGPUVirtualAddress();
    blasBuild.DestAccelerationStructureData = blasResult_->GetGPUVirtualAddress();
    commandList->BuildRaytracingAccelerationStructure(&blasBuild, 0, nullptr);
    UavBarrier(commandList, blasResult_.Get());

    D3D12_RAYTRACING_INSTANCE_DESC instance{};
    instance.Transform[0][0] = 1.0f;
    instance.Transform[1][1] = 1.0f;
    instance.Transform[2][2] = 1.0f;
    instance.InstanceID = 0;
    instance.InstanceMask = 0xff;
    instance.InstanceContributionToHitGroupIndex = 0;
    instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
    instance.AccelerationStructure = blasResult_->GetGPUVirtualAddress();
    if (!instanceUpload_.Create(device, &instance, sizeof(instance))) {
        return false;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs{};
    tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    tlasInputs.NumDescs = 1;
    tlasInputs.InstanceDescs = instanceUpload_.GpuAddress();
    tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasInfo{};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasInfo);
    if (tlasInfo.ResultDataMaxSizeInBytes == 0) {
        return false;
    }

    const UINT64 tlasScratchBytes = AlignUp(tlasInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
    const UINT64 tlasResultBytes = AlignUp(tlasInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
    if (!CreateDefaultBuffer(device, tlasScratchBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, tlasScratch_)) {
        return false;
    }
    if (!CreateDefaultBuffer(device, tlasResultBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, tlasResult_)) {
        return false;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuild{};
    tlasBuild.Inputs = tlasInputs;
    tlasBuild.ScratchAccelerationStructureData = tlasScratch_->GetGPUVirtualAddress();
    tlasBuild.DestAccelerationStructureData = tlasResult_->GetGPUVirtualAddress();
    commandList->BuildRaytracingAccelerationStructure(&tlasBuild, 0, nullptr);
    UavBarrier(commandList, tlasResult_.Get());
    return true;
}

void DxrSceneResources::WriteDescriptors(ID3D12Device5* device) {
    if (!descriptorHeap_ ||
        !output_ ||
        !tlasResult_ ||
        !materialUpload_.Resource() ||
        !triangleMetadataUpload_.Resource() ||
        !spriteUpload_.Resource() ||
        !vfxAtlas_) {
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(output_.Get(), nullptr, &uav, handle);

    handle.ptr += descriptorSize_;
    D3D12_SHADER_RESOURCE_VIEW_DESC tlasSrv{};
    tlasSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    tlasSrv.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    tlasSrv.RaytracingAccelerationStructure.Location = tlasResult_->GetGPUVirtualAddress();
    device->CreateShaderResourceView(nullptr, &tlasSrv, handle);

    handle.ptr += descriptorSize_;
    D3D12_SHADER_RESOURCE_VIEW_DESC materialSrv{};
    materialSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    materialSrv.Format = DXGI_FORMAT_UNKNOWN;
    materialSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    materialSrv.Buffer.FirstElement = 0;
    materialSrv.Buffer.NumElements = materialCount_;
    materialSrv.Buffer.StructureByteStride = sizeof(EntityMaterial);
    materialSrv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    device->CreateShaderResourceView(materialUpload_.Resource(), &materialSrv, handle);

    handle.ptr += descriptorSize_;
    D3D12_SHADER_RESOURCE_VIEW_DESC triangleSrv{};
    triangleSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    triangleSrv.Format = DXGI_FORMAT_UNKNOWN;
    triangleSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    triangleSrv.Buffer.FirstElement = 0;
    triangleSrv.Buffer.NumElements = stats_.triangleCount;
    triangleSrv.Buffer.StructureByteStride = sizeof(RtTriangleMetadata);
    triangleSrv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    device->CreateShaderResourceView(triangleMetadataUpload_.Resource(), &triangleSrv, handle);

    handle.ptr += descriptorSize_;
    D3D12_SHADER_RESOURCE_VIEW_DESC outputSrv{};
    outputSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    outputSrv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    outputSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    outputSrv.Texture2D.MipLevels = 1;
    outputSrv.Texture2D.MostDetailedMip = 0;
    outputSrv.Texture2D.PlaneSlice = 0;
    outputSrv.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(output_.Get(), &outputSrv, handle);

    handle.ptr = descriptorHeap_->GetCPUDescriptorHandleForHeapStart().ptr + descriptorSize_ * kDescriptorSpriteSrv;
    D3D12_SHADER_RESOURCE_VIEW_DESC spriteSrv{};
    spriteSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    spriteSrv.Format = DXGI_FORMAT_UNKNOWN;
    spriteSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    spriteSrv.Buffer.FirstElement = 0;
    spriteSrv.Buffer.NumElements = std::max(spriteCount_, 1u);
    spriteSrv.Buffer.StructureByteStride = sizeof(RenderSprite);
    spriteSrv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    device->CreateShaderResourceView(spriteUpload_.Resource(), &spriteSrv, handle);

    handle.ptr = descriptorHeap_->GetCPUDescriptorHandleForHeapStart().ptr + descriptorSize_ * kDescriptorVfxAtlasSrv;
    D3D12_SHADER_RESOURCE_VIEW_DESC atlasSrv{};
    atlasSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    atlasSrv.Format = DXGI_FORMAT_R8_UNORM;
    atlasSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    atlasSrv.Texture2D.MipLevels = 1;
    atlasSrv.Texture2D.MostDetailedMip = 0;
    atlasSrv.Texture2D.PlaneSlice = 0;
    atlasSrv.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(vfxAtlas_.Get(), &atlasSrv, handle);
}

}
