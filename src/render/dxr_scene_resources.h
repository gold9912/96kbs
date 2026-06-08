#pragma once

#include "render/gpu_upload_buffer.h"
#include "render/render_scene.h"

#include <cstddef>

#include <d3d12.h>
#include <wrl/client.h>

namespace rogue {

struct DxrBuildStats {
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t triangleCount = 0;
    uint32_t geometryHash = 0;
};

class DxrSceneResources {
public:
    bool Update(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, const RenderScene& scene);
    bool Update(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, const PackedRTGeometry& geometry);
    void Reset();

    ID3D12DescriptorHeap* DescriptorHeap() const { return descriptorHeap_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE OutputUavGpu() const;
    D3D12_GPU_DESCRIPTOR_HANDLE SrvTableGpu() const;
    D3D12_GPU_DESCRIPTOR_HANDLE OutputSrvGpu() const;
    D3D12_GPU_VIRTUAL_ADDRESS FrameConstantsGpuAddress() const { return frameConstants_.GpuAddress(); }
    D3D12_GPU_VIRTUAL_ADDRESS TlasGpuAddress() const;
    DxrBuildStats Stats() const { return stats_; }
    void TransitionOutput(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES targetState);
    void InsertOutputUavBarrier(ID3D12GraphicsCommandList* commandList) const;

private:
    bool EnsureDescriptorHeap(ID3D12Device5* device);
    bool EnsureOutput(ID3D12Device5* device, uint32_t width, uint32_t height);
    bool EnsureVfxAtlas(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList);
    bool UploadGeometry(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, const PackedRTGeometry& geometry);
    bool UploadFrameData(ID3D12Device5* device, const RenderScene& scene);
    bool UploadSprites(ID3D12Device5* device, const RenderScene& scene);
    bool BuildAccelerationStructures(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList);
    void WriteDescriptors(ID3D12Device5* device);

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> output_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> blasScratch_;
    Microsoft::WRL::ComPtr<ID3D12Resource> blasResult_;
    Microsoft::WRL::ComPtr<ID3D12Resource> tlasScratch_;
    Microsoft::WRL::ComPtr<ID3D12Resource> tlasResult_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vfxAtlas_;

    GpuUploadBuffer vertexUpload_;
    GpuUploadBuffer indexUpload_;
    GpuUploadBuffer instanceUpload_;
    GpuUploadBuffer frameConstants_;
    GpuUploadBuffer materialUpload_;
    GpuUploadBuffer triangleMetadataUpload_;
    GpuUploadBuffer spriteUpload_;
    GpuUploadBuffer vfxAtlasUpload_;

    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc_{};
    uint32_t descriptorSize_ = 0;
    uint32_t outputWidth_ = 0;
    uint32_t outputHeight_ = 0;
    uint32_t materialCount_ = 0;
    uint32_t spriteCount_ = 0;
    D3D12_RESOURCE_STATES outputState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    DxrBuildStats stats_{};
};

}
