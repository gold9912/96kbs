#include "render/dxr_scene_resources.h"

#include <algorithm>
#include <cstring>

namespace rogue {

namespace {

constexpr UINT64 AlignUp(UINT64 value, UINT64 alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

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

D3D12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = type;
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    return heap;
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
    if (!EnsureDescriptorHeap(device) || !EnsureOutput(device, scene.frame.outputWidth, scene.frame.outputHeight)) {
        return false;
    }
    TransitionOutput(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    if (!UploadFrameData(device, scene)) {
        return false;
    }
    return Update(device, commandList, scene.packedGeometry);
}

bool DxrSceneResources::Update(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, const PackedRTGeometry& geometry) {
    if (!device || !commandList || geometry.vertices.empty() || geometry.indices.empty()) {
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
    vertexUpload_.Reset();
    indexUpload_.Reset();
    instanceUpload_.Reset();
    frameConstants_.Reset();
    materialUpload_.Reset();
    geometryDesc_ = {};
    descriptorSize_ = 0;
    outputWidth_ = 0;
    outputHeight_ = 0;
    materialCount_ = 0;
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
    handle.ptr += descriptorSize_ * 3u;
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
    desc.NumDescriptors = 4;
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
    if (!descriptorHeap_ || !output_ || !tlasResult_ || !materialUpload_.Resource()) {
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
    D3D12_SHADER_RESOURCE_VIEW_DESC outputSrv{};
    outputSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    outputSrv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    outputSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    outputSrv.Texture2D.MipLevels = 1;
    outputSrv.Texture2D.MostDetailedMip = 0;
    outputSrv.Texture2D.PlaneSlice = 0;
    outputSrv.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(output_.Get(), &outputSrv, handle);
}

}
