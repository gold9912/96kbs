#pragma once

#include "render/gpu_upload_buffer.h"

#include <cstdint>

#include <d3d12.h>

namespace rogue {

class DxrPipeline;

class DxrShaderTable {
public:
    bool Initialize(ID3D12Device5* device, const DxrPipeline& pipeline);
    void Reset();
    D3D12_DISPATCH_RAYS_DESC DispatchDesc(uint32_t width, uint32_t height) const;

private:
    GpuUploadBuffer table_;
    uint32_t recordSize_ = 0;
};

}

