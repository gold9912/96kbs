#pragma once

#include <cstdint>
#include <string>

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace rogue {

struct D3D12InitResult {
    bool ok = false;
    bool dxrSupported = false;
    D3D12_RAYTRACING_TIER raytracingTier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
};

struct D3D12FrameCallbackResult {
    bool ok = true;
    ID3D12DescriptorHeap* descriptorHeap = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE compositeSrv{};
};

using D3D12FrameCallback = D3D12FrameCallbackResult (*)(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, void* userData);

class D3D12Context {
public:
    bool Initialize(HWND hwnd, uint32_t width, uint32_t height, bool requireDxr);
    void Shutdown();
    bool RenderFrame(float clearPulse, D3D12FrameCallback callback = nullptr, void* userData = nullptr);

    ID3D12Device5* DxrDevice() const { return device5_.Get(); }
    D3D12InitResult InitResult() const { return initResult_; }
    const std::string& AdapterName() const { return adapterName_; }
    const std::string& LastError() const { return lastError_; }

private:
    bool CreateDevice(bool requireDxr);
    bool CreateSwapchain(HWND hwnd, uint32_t width, uint32_t height);
    bool CreateFrameResources(bool requireDxr);
    bool CreateWorldPipeline();
    bool CreateCompositePipeline();
    bool CreateFence();
    void WaitForGpu();
    void DrawWorld(float clearPulse);
    void DrawComposite(ID3D12DescriptorHeap* descriptorHeap, D3D12_GPU_DESCRIPTOR_HANDLE srv);
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const;

    static constexpr uint32_t kFrameCount = 2;

    D3D12InitResult initResult_{};
    std::string adapterName_{};
    std::string lastError_{};
    uint32_t rtvDescriptorSize_ = 0;
    uint32_t frameIndex_ = 0;
    uint64_t fenceValue_ = 0;
    void* fenceEvent_ = nullptr;

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12Device5> device5_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> worldRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> worldPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> compositeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> compositePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets_[kFrameCount];
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators_[kFrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> dxrCommandList_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
};

}
