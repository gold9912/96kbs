#pragma once

#include "render/dxr_shader_table.h"

#include <cstdint>

#include <d3d12.h>
#include <wrl/client.h>

namespace rogue {

class DxrSceneResources;

class DxrPipeline {
public:
    bool Initialize(ID3D12Device5* device);
    void Dispatch(ID3D12GraphicsCommandList4* commandList, DxrSceneResources& sceneResources, uint32_t width, uint32_t height) const;
    ID3D12StateObject* StateObject() const { return stateObject_.Get(); }
    ID3D12RootSignature* GlobalRootSignature() const { return globalRootSignature_.Get(); }
    const void* ShaderIdentifier(const wchar_t* exportName) const;

private:
    bool CreateGlobalRootSignature(ID3D12Device5* device);
    bool CreateStateObject(ID3D12Device5* device);

    Microsoft::WRL::ComPtr<ID3D12RootSignature> globalRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12StateObject> stateObject_;
    Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> stateObjectProperties_;
    DxrShaderTable shaderTable_;
};

}
