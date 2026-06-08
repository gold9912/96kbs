#include "platform/d3d12_context.h"

#include "embedded_shaders.h"

#include <windows.h>

#include <climits>
#include <cstdio>
#include <string>

namespace rogue {

namespace {

bool Failed(HRESULT hr, const char* label) {
    if (SUCCEEDED(hr)) {
        return false;
    }
    char buffer[256]{};
    std::snprintf(buffer, sizeof(buffer), "%s failed: 0x%08x\n", label, static_cast<unsigned>(hr));
    OutputDebugStringA(buffer);
    return true;
}

}

bool D3D12Context::Initialize(HWND hwnd, uint32_t width, uint32_t height, bool requireDxr) {
    lastError_.clear();
#if defined(_DEBUG)
    Microsoft::WRL::ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
        debug->EnableDebugLayer();
    }
#endif

    if (!CreateDevice(requireDxr)) {
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    if (Failed(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_)), "CreateCommandQueue")) {
        lastError_ = "CreateCommandQueue failed.";
        return false;
    }

    if (!CreateSwapchain(hwnd, width, height)) {
        if (lastError_.empty()) {
            lastError_ = "Swapchain creation failed.";
        }
        return false;
    }
    if (!CreateFrameResources(requireDxr)) {
        if (lastError_.empty()) {
            lastError_ = "Frame resource creation failed.";
        }
        return false;
    }
    if (!CreateWorldPipeline()) {
        if (lastError_.empty()) {
            lastError_ = "World fullscreen pipeline creation failed.";
        }
        return false;
    }
    if (requireDxr && !CreateCompositePipeline()) {
        if (lastError_.empty()) {
            lastError_ = "DXR composite pipeline creation failed.";
        }
        return false;
    }
    if (!CreateFence()) {
        if (lastError_.empty()) {
            lastError_ = "Fence creation failed.";
        }
        return false;
    }

    initResult_.ok = true;
    return true;
}

void D3D12Context::Shutdown() {
    if (commandQueue_ && fence_) {
        WaitForGpu(1000);
    }
    if (fenceEvent_) {
        CloseHandle(static_cast<HANDLE>(fenceEvent_));
        fenceEvent_ = nullptr;
    }
}

bool D3D12Context::RenderFrame(float clearPulse, D3D12FrameCallback callback, void* userData, const D3D12OverlayConstants* overlay) {
    ID3D12CommandAllocator* allocator = commandAllocators_[frameIndex_].Get();
    if (Failed(allocator->Reset(), "CommandAllocator::Reset")) {
        return false;
    }
    if (Failed(commandList_->Reset(allocator, nullptr), "CommandList::Reset")) {
        return false;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderTargets_[frameIndex_].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    DrawWorld(clearPulse, overlay);

    D3D12FrameCallbackResult callbackResult{};
    if (callback) {
        if (!dxrCommandList_) {
            callbackResult.ok = false;
            OutputDebugStringA("Frame callback requested without an ID3D12GraphicsCommandList4.\n");
        } else {
            callbackResult = callback(device5_.Get(), dxrCommandList_.Get(), userData);
        }
        if (!callbackResult.ok) {
            OutputDebugStringA("Frame callback reported failure.\n");
        }
    }
    if (callbackResult.ok && callbackResult.descriptorHeap && callbackResult.compositeSrv.ptr != 0) {
        DrawComposite(callbackResult.descriptorHeap, callbackResult.compositeSrv, overlay);
    }

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &barrier);

    if (Failed(commandList_->Close(), "CommandList::Close")) {
        return false;
    }

    ID3D12CommandList* lists[] = {commandList_.Get()};
    commandQueue_->ExecuteCommandLists(1, lists);
    swapChain_->Present(0, 0);
    if (!WaitForGpu()) {
        return false;
    }
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
    return callbackResult.ok;
}

void D3D12Context::DrawWorld(float clearPulse, const D3D12OverlayConstants* overlay) {
    const float clearColor[4] = {
        0.015f + clearPulse * 0.025f,
        0.010f,
        0.018f + clearPulse * 0.018f,
        1.0f
    };
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = CurrentRtv();
    commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    commandList_->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    SetViewportAndScissor();
    commandList_->SetPipelineState(worldPipelineState_.Get());
    commandList_->SetGraphicsRootSignature(worldRootSignature_.Get());
    D3D12OverlayConstants constants = overlay ? *overlay : D3D12OverlayConstants{};
    constants.outputWidth = constants.outputWidth != 0 ? constants.outputWidth : width_;
    constants.outputHeight = constants.outputHeight != 0 ? constants.outputHeight : height_;
    constants.renderWidth = constants.renderWidth != 0 ? constants.renderWidth : constants.outputWidth;
    constants.renderHeight = constants.renderHeight != 0 ? constants.renderHeight : constants.outputHeight;
    commandList_->SetGraphicsRoot32BitConstants(
        0,
        static_cast<UINT>(sizeof(D3D12OverlayConstants) / sizeof(uint32_t)),
        &constants,
        0);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->DrawInstanced(3, 1, 0, 0);
}

void D3D12Context::DrawComposite(ID3D12DescriptorHeap* descriptorHeap, D3D12_GPU_DESCRIPTOR_HANDLE srv, const D3D12OverlayConstants* overlay) {
    if (!descriptorHeap || srv.ptr == 0) {
        return;
    }
    ID3D12DescriptorHeap* heaps[] = {descriptorHeap};
    commandList_->SetDescriptorHeaps(1, heaps);
    SetViewportAndScissor();
    commandList_->SetPipelineState(compositePipelineState_.Get());
    commandList_->SetGraphicsRootSignature(compositeRootSignature_.Get());
    commandList_->SetGraphicsRootDescriptorTable(0, srv);
    D3D12OverlayConstants constants = overlay ? *overlay : D3D12OverlayConstants{};
    constants.outputWidth = constants.outputWidth != 0 ? constants.outputWidth : width_;
    constants.outputHeight = constants.outputHeight != 0 ? constants.outputHeight : height_;
    commandList_->SetGraphicsRoot32BitConstants(
        1,
        static_cast<UINT>(sizeof(D3D12OverlayConstants) / sizeof(uint32_t)),
        &constants,
        0);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->DrawInstanced(3, 1, 0, 0);
}

bool D3D12Context::CreateDevice(bool requireDxr) {
    UINT factoryFlags = 0;
#if defined(_DEBUG)
    factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    HRESULT factoryHr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory_));
#if defined(_DEBUG)
    if (FAILED(factoryHr) && factoryFlags != 0) {
        OutputDebugStringA("Debug DXGI factory creation failed; retrying without DXGI_CREATE_FACTORY_DEBUG.\n");
        factoryHr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory_));
    }
#endif
    if (Failed(factoryHr, "CreateDXGIFactory2")) {
        char message[192]{};
        std::snprintf(message, sizeof(message), "CreateDXGIFactory2 failed: 0x%08x.", static_cast<unsigned>(factoryHr));
        lastError_ = message;
        return false;
    }

    const D3D_FEATURE_LEVEL featureLevel = requireDxr ? D3D_FEATURE_LEVEL_12_1 : D3D_FEATURE_LEVEL_11_0;
    for (UINT index = 0; factory_->EnumAdapters1(index, &adapter_) != DXGI_ERROR_NOT_FOUND; ++index) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter_->GetDesc1(&desc);
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(adapter_.Get(), featureLevel, IID_PPV_ARGS(&device_)))) {
            char name[128]{};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, static_cast<int>(sizeof(name)), nullptr, nullptr);
            adapterName_ = name;
            break;
        }
        adapter_.Reset();
    }

    if (!device_) {
        if (Failed(D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&device_)), "D3D12CreateDevice")) {
            lastError_ = requireDxr
                ? "No D3D12 feature level 12_1 capable adapter was found."
                : "No D3D12 feature level 11_0 capable adapter was found.";
            return false;
        }
        adapterName_ = "default adapter";
    }

    if (Failed(device_.As(&device5_), "Query ID3D12Device5")) {
        initResult_.dxrSupported = false;
        if (requireDxr) {
            lastError_ = "The selected adapter exposes D3D12 but not ID3D12Device5.";
            return false;
        }
        lastError_ = "The selected adapter exposes D3D12 but not ID3D12Device5; explicit raster test mode is active.";
        return true;
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
    if (Failed(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)), "CheckFeatureSupport OPTIONS5")) {
        initResult_.dxrSupported = false;
        lastError_ = "D3D12_OPTIONS5 feature query failed.";
        if (!requireDxr) {
            OutputDebugStringA("D3D12_OPTIONS5 query failed; continuing in explicit raster test mode.\n");
            return true;
        }
        return false;
    }

    initResult_.raytracingTier = options5.RaytracingTier;
    initResult_.dxrSupported = options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
    if (!initResult_.dxrSupported) {
        if (requireDxr) {
            OutputDebugStringA("DXR is required, but this adapter reports no raytracing tier.\n");
            lastError_ = "DXR is required, but the selected adapter reports D3D12_RAYTRACING_TIER_NOT_SUPPORTED.";
            return false;
        }
        OutputDebugStringA("DXR is unavailable; continuing in explicit raster test mode.\n");
    }

    return true;
}

bool D3D12Context::CreateSwapchain(HWND hwnd, uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.BufferCount = kFrameCount;
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.SampleDesc.Count = 1;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    if (Failed(factory_->CreateSwapChainForHwnd(commandQueue_.Get(), hwnd, &desc, nullptr, nullptr, &swapChain1), "CreateSwapChainForHwnd")) {
        lastError_ = "CreateSwapChainForHwnd failed.";
        return false;
    }
    if (Failed(factory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER), "MakeWindowAssociation")) {
        lastError_ = "MakeWindowAssociation failed.";
        return false;
    }
    if (Failed(swapChain1.As(&swapChain_), "Query IDXGISwapChain3")) {
        lastError_ = "The swapchain does not expose IDXGISwapChain3.";
        return false;
    }
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
    return true;
}

void D3D12Context::SetViewportAndScissor() {
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissor{};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = static_cast<LONG>(width_);
    scissor.bottom = static_cast<LONG>(height_);

    commandList_->RSSetViewports(1, &viewport);
    commandList_->RSSetScissorRects(1, &scissor);
}

bool D3D12Context::CreateFrameResources(bool requireDxr) {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = kFrameCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (Failed(device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap_)), "CreateDescriptorHeap RTV")) {
        lastError_ = "CreateDescriptorHeap RTV failed.";
        return false;
    }
    rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < kFrameCount; ++i) {
        if (Failed(swapChain_->GetBuffer(i, IID_PPV_ARGS(&renderTargets_[i])), "SwapChain::GetBuffer")) {
            lastError_ = "SwapChain::GetBuffer failed.";
            return false;
        }
        device_->CreateRenderTargetView(renderTargets_[i].Get(), nullptr, handle);
        handle.ptr += rtvDescriptorSize_;

        if (Failed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators_[i])), "CreateCommandAllocator")) {
            lastError_ = "CreateCommandAllocator failed.";
            return false;
        }
    }

    if (Failed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators_[frameIndex_].Get(), nullptr, IID_PPV_ARGS(&commandList_)), "CreateCommandList")) {
        lastError_ = "CreateCommandList failed.";
        return false;
    }
    HRESULT commandList4Hr = commandList_.As(&dxrCommandList_);
    if (FAILED(commandList4Hr)) {
        if (requireDxr) {
            Failed(commandList4Hr, "Query ID3D12GraphicsCommandList4");
            lastError_ = "DXR requires ID3D12GraphicsCommandList4, but the selected D3D12 runtime does not expose it.";
            return false;
        }
        OutputDebugStringA("ID3D12GraphicsCommandList4 is unavailable; continuing in explicit raster test mode.\n");
    }
    commandList_->Close();
    return true;
}

bool D3D12Context::CreateWorldPipeline() {
    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    D3D12_ROOT_PARAMETER rootParam{};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParam.Constants.ShaderRegister = 0;
    rootParam.Constants.RegisterSpace = 0;
    rootParam.Constants.Num32BitValues = sizeof(D3D12OverlayConstants) / sizeof(uint32_t);
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootDesc.NumParameters = 1;
    rootDesc.pParameters = &rootParam;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> rootBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    if (Failed(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errorBlob), "Serialize world root signature")) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return false;
    }
    if (Failed(device_->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(), IID_PPV_ARGS(&worldRootSignature_)), "Create world root signature")) {
        return false;
    }

    const shaders::Bytecode vs = shaders::FullscreenVS();
    const shaders::Bytecode ps = shaders::WorldFullscreenPS();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = worldRootSignature_.Get();
    pso.VS = D3D12_SHADER_BYTECODE{vs.data, vs.size};
    pso.PS = D3D12_SHADER_BYTECODE{ps.data, ps.size};
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;

    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;

    pso.BlendState.RenderTarget[0].BlendEnable = FALSE;
    pso.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
    pso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    pso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    pso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    pso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    pso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    pso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    pso.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;

    const HRESULT hr = device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&worldPipelineState_));
    if (Failed(hr, "Create world PSO")) {
        char message[192]{};
        std::snprintf(message, sizeof(message), "Create world PSO failed: 0x%08x.", static_cast<unsigned>(hr));
        lastError_ = message;
        return false;
    }
    return true;
}

bool D3D12Context::CreateCompositePipeline() {
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 3;
    range.BaseShaderRegister = 0;
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].DescriptorTable.NumDescriptorRanges = 1;
    params[0].DescriptorTable.pDescriptorRanges = &range;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[1].Constants.ShaderRegister = 0;
    params[1].Constants.RegisterSpace = 0;
    params[1].Constants.Num32BitValues = sizeof(D3D12OverlayConstants) / sizeof(uint32_t);
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0.0f;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = 2;
    rootDesc.pParameters = params;
    rootDesc.NumStaticSamplers = 1;
    rootDesc.pStaticSamplers = &sampler;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> rootBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    if (Failed(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errorBlob), "Serialize composite root signature")) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return false;
    }
    if (Failed(device_->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(), IID_PPV_ARGS(&compositeRootSignature_)), "Create composite root signature")) {
        return false;
    }

    const shaders::Bytecode vs = shaders::FullscreenVS();
    const shaders::Bytecode ps = shaders::DxrCompositePS();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = compositeRootSignature_.Get();
    pso.VS = D3D12_SHADER_BYTECODE{vs.data, vs.size};
    pso.PS = D3D12_SHADER_BYTECODE{ps.data, ps.size};
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;

    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;

    D3D12_RENDER_TARGET_BLEND_DESC& blend = pso.BlendState.RenderTarget[0];
    blend.BlendEnable = FALSE;
    blend.LogicOpEnable = FALSE;
    blend.SrcBlend = D3D12_BLEND_ONE;
    blend.DestBlend = D3D12_BLEND_ZERO;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;

    const HRESULT hr = device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&compositePipelineState_));
    if (Failed(hr, "Create composite PSO")) {
        char message[192]{};
        std::snprintf(message, sizeof(message), "Create composite PSO failed: 0x%08x.", static_cast<unsigned>(hr));
        lastError_ = message;
        return false;
    }
    return true;
}

bool D3D12Context::CreateFence() {
    if (Failed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence")) {
        return false;
    }
    fenceValue_ = 1;
    fenceEvent_ = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    return fenceEvent_ != nullptr;
}

bool D3D12Context::WaitForGpu(uint32_t timeoutMs) {
    if (!commandQueue_ || !fence_ || !fenceEvent_) {
        lastError_ = "GPU fence wait requested before synchronization objects were ready.";
        return false;
    }

    const uint64_t signalValue = fenceValue_;
    if (Failed(commandQueue_->Signal(fence_.Get(), signalValue), "CommandQueue::Signal")) {
        lastError_ = "CommandQueue::Signal failed while waiting for GPU.";
        return false;
    }
    ++fenceValue_;

    if (fence_->GetCompletedValue() < signalValue) {
        if (Failed(fence_->SetEventOnCompletion(signalValue, static_cast<HANDLE>(fenceEvent_)), "Fence::SetEventOnCompletion")) {
            lastError_ = "Fence::SetEventOnCompletion failed while waiting for GPU.";
            return false;
        }
        const DWORD waitResult = WaitForSingleObject(static_cast<HANDLE>(fenceEvent_), timeoutMs);
        if (waitResult != WAIT_OBJECT_0) {
            lastError_ = waitResult == WAIT_TIMEOUT
                ? "GPU fence wait timed out."
                : "GPU fence wait failed.";
            OutputDebugStringA(lastError_.c_str());
            OutputDebugStringA("\n");
            return false;
        }
    }
    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Context::CurrentRtv() const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(frameIndex_) * rtvDescriptorSize_;
    return handle;
}

}
