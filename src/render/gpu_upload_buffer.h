#pragma once

#include <cstddef>

#include <d3d12.h>
#include <wrl/client.h>

namespace rogue {

class GpuUploadBuffer {
public:
    bool Create(ID3D12Device* device, const void* data, std::size_t size);
    void Reset();

    ID3D12Resource* Resource() const { return resource_.Get(); }
    D3D12_GPU_VIRTUAL_ADDRESS GpuAddress() const;
    std::size_t Size() const { return size_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    std::size_t size_ = 0;
};

}

