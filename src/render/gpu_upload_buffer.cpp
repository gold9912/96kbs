#include "render/gpu_upload_buffer.h"

#include <cstring>

namespace rogue {

namespace {

D3D12_RESOURCE_DESC BufferDesc(std::size_t size) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = static_cast<UINT64>(size);
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    return desc;
}

}

bool GpuUploadBuffer::Create(ID3D12Device* device, const void* data, std::size_t size) {
    Reset();
    if (!device || !data || size == 0) {
        return false;
    }

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC desc = BufferDesc(size);
    const HRESULT hr = device->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource_));
    if (FAILED(hr)) {
        Reset();
        return false;
    }

    void* mapped = nullptr;
    D3D12_RANGE readRange{0, 0};
    if (FAILED(resource_->Map(0, &readRange, &mapped))) {
        Reset();
        return false;
    }
    std::memcpy(mapped, data, size);
    resource_->Unmap(0, nullptr);
    size_ = size;
    return true;
}

void GpuUploadBuffer::Reset() {
    resource_.Reset();
    size_ = 0;
}

D3D12_GPU_VIRTUAL_ADDRESS GpuUploadBuffer::GpuAddress() const {
    return resource_ ? resource_->GetGPUVirtualAddress() : 0;
}

}

