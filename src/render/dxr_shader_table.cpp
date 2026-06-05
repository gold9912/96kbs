#include "render/dxr_shader_table.h"

#include "render/dxr_pipeline.h"

#include <array>
#include <cstring>
#include <vector>

namespace rogue {

namespace {

constexpr uint32_t AlignUp(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

}

bool DxrShaderTable::Initialize(ID3D12Device5* device, const DxrPipeline& pipeline) {
    Reset();
    if (!device) {
        return false;
    }

    recordSize_ = AlignUp(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    const uint32_t tableSize = recordSize_ * 3u;
    std::vector<unsigned char> data(tableSize);

    const void* rayGen = pipeline.ShaderIdentifier(L"RayGen");
    const void* miss = pipeline.ShaderIdentifier(L"Miss");
    const void* hit = pipeline.ShaderIdentifier(L"EntityHitGroup");
    if (!rayGen || !miss || !hit) {
        return false;
    }

    std::memcpy(data.data() + recordSize_ * 0u, rayGen, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    std::memcpy(data.data() + recordSize_ * 1u, miss, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    std::memcpy(data.data() + recordSize_ * 2u, hit, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    return table_.Create(device, data.data(), data.size());
}

void DxrShaderTable::Reset() {
    table_.Reset();
    recordSize_ = 0;
}

D3D12_DISPATCH_RAYS_DESC DxrShaderTable::DispatchDesc(uint32_t width, uint32_t height) const {
    D3D12_DISPATCH_RAYS_DESC desc{};
    const D3D12_GPU_VIRTUAL_ADDRESS base = table_.GpuAddress();
    desc.RayGenerationShaderRecord.StartAddress = base;
    desc.RayGenerationShaderRecord.SizeInBytes = recordSize_;
    desc.MissShaderTable.StartAddress = base + recordSize_;
    desc.MissShaderTable.SizeInBytes = recordSize_;
    desc.MissShaderTable.StrideInBytes = recordSize_;
    desc.HitGroupTable.StartAddress = base + recordSize_ * 2u;
    desc.HitGroupTable.SizeInBytes = recordSize_;
    desc.HitGroupTable.StrideInBytes = recordSize_;
    desc.Width = width;
    desc.Height = height;
    desc.Depth = 1;
    return desc;
}

}
