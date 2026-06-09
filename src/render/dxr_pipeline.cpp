#include "render/dxr_pipeline.h"

#include "embedded_shaders.h"
#include "render/dxr_scene_resources.h"

#include <windows.h>

#include <cstdio>
#include <iterator>

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

bool DxrPipeline::Initialize(ID3D12Device5* device) {
    if (!device) {
        return false;
    }
    return CreateGlobalRootSignature(device) && CreateStateObject(device) && shaderTable_.Initialize(device, *this);
}

void DxrPipeline::Dispatch(ID3D12GraphicsCommandList4* commandList, DxrSceneResources& sceneResources, uint32_t width, uint32_t height) const {
    if (!commandList || !stateObject_ || !globalRootSignature_) {
        return;
    }

    ID3D12DescriptorHeap* heaps[] = {sceneResources.DescriptorHeap()};
    commandList->SetDescriptorHeaps(1, heaps);
    commandList->SetComputeRootSignature(globalRootSignature_.Get());
    commandList->SetComputeRootDescriptorTable(0, sceneResources.OutputUavGpu());
    commandList->SetComputeRootDescriptorTable(1, sceneResources.SrvTableGpu());
    commandList->SetComputeRootConstantBufferView(2, sceneResources.FrameConstantsGpuAddress());
    commandList->SetPipelineState1(stateObject_.Get());

    D3D12_DISPATCH_RAYS_DESC desc = shaderTable_.DispatchDesc(width, height);
    commandList->DispatchRays(&desc);
}

const void* DxrPipeline::ShaderIdentifier(const wchar_t* exportName) const {
    return stateObjectProperties_ && exportName ? stateObjectProperties_->GetShaderIdentifier(exportName) : nullptr;
}

bool DxrPipeline::CreateGlobalRootSignature(ID3D12Device5* device) {
    D3D12_DESCRIPTOR_RANGE ranges[2]{};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[0].NumDescriptors = 1;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[1].NumDescriptors = 3;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER params[3]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].DescriptorTable.NumDescriptorRanges = 1;
    params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[2].Descriptor.ShaderRegister = 0;
    params[2].Descriptor.RegisterSpace = 0;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = static_cast<UINT>(std::size(params));
    desc.pParameters = params;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    if (Failed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), "D3D12SerializeRootSignature")) {
        if (error) {
            OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        }
        return false;
    }

    return !Failed(device->CreateRootSignature(
        0,
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        IID_PPV_ARGS(&globalRootSignature_)), "CreateRootSignature");
}

bool DxrPipeline::CreateStateObject(ID3D12Device5* device) {
    const shaders::Bytecode dxr = shaders::DxrEntities();

    D3D12_EXPORT_DESC exports[3]{};
    exports[0].Name = L"RayGen";
    exports[1].Name = L"Miss";
    exports[2].Name = L"ClosestHit";

    D3D12_DXIL_LIBRARY_DESC libraryDesc{};
    libraryDesc.DXILLibrary.pShaderBytecode = dxr.data;
    libraryDesc.DXILLibrary.BytecodeLength = dxr.size;
    libraryDesc.NumExports = 3;
    libraryDesc.pExports = exports;

    D3D12_HIT_GROUP_DESC hitGroupDesc{};
    hitGroupDesc.HitGroupExport = L"EntityHitGroup";
    hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroupDesc.ClosestHitShaderImport = L"ClosestHit";

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig{};
    shaderConfig.MaxPayloadSizeInBytes = 20;
    shaderConfig.MaxAttributeSizeInBytes = 8;

    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig{};
    pipelineConfig.MaxTraceRecursionDepth = 4;

    D3D12_GLOBAL_ROOT_SIGNATURE globalRoot{};
    globalRoot.pGlobalRootSignature = globalRootSignature_.Get();

    D3D12_STATE_SUBOBJECT subobjects[5]{};
    subobjects[0].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    subobjects[0].pDesc = &libraryDesc;
    subobjects[1].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    subobjects[1].pDesc = &hitGroupDesc;
    subobjects[2].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    subobjects[2].pDesc = &shaderConfig;
    subobjects[3].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    subobjects[3].pDesc = &pipelineConfig;
    subobjects[4].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    subobjects[4].pDesc = &globalRoot;

    D3D12_STATE_OBJECT_DESC desc{};
    desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    desc.NumSubobjects = static_cast<UINT>(std::size(subobjects));
    desc.pSubobjects = subobjects;

    if (Failed(device->CreateStateObject(&desc, IID_PPV_ARGS(&stateObject_)), "CreateStateObject DXR")) {
        return false;
    }

    if (Failed(stateObject_.As(&stateObjectProperties_), "Query ID3D12StateObjectProperties")) {
        return false;
    }

    const void* rayGenId = stateObjectProperties_->GetShaderIdentifier(L"RayGen");
    const void* missId = stateObjectProperties_->GetShaderIdentifier(L"Miss");
    const void* hitId = stateObjectProperties_->GetShaderIdentifier(L"EntityHitGroup");
    return rayGenId && missId && hitId;
}

}
