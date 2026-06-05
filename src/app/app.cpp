#include "app/app.h"

#include <windows.h>

#include <cmath>
#include <cstdio>

namespace rogue {

bool Application::Initialize(HWND hwnd, const EngineConfig& config) {
    config_ = config;
    session_.Start(config.seed);

    if (!d3d_.Initialize(hwnd, config.width, config.height, config.requireDxr)) {
        char message[512]{};
        std::snprintf(message, sizeof(message),
            config.requireDxr
                ? "D3D12/DXR initialization failed.\n\nAdapter: %s\nReason: %s\n\nThis prototype currently requires a DXR-capable GPU."
                : "D3D12 initialization failed.\n\nAdapter: %s\nReason: %s\n\nRaster test mode still requires a working D3D12 device.",
            d3d_.AdapterName().empty() ? "<not selected>" : d3d_.AdapterName().c_str(),
            d3d_.LastError().empty() ? "unknown initialization failure" : d3d_.LastError().c_str());
        MessageBoxA(hwnd,
            message,
            "Rogue96DXR",
            MB_ICONERROR | MB_OK);
        return false;
    }

    if (config.requireDxr && d3d_.InitResult().dxrSupported && !dxr_.Initialize(d3d_.DxrDevice())) {
        MessageBoxA(hwnd,
            "DXR state object creation failed. Offline embedded DXIL or driver support is invalid.",
            "Rogue96DXR",
            MB_ICONERROR | MB_OK);
        return false;
    }

    return true;
}

void Application::Shutdown() {
    d3d_.Shutdown();
}

void Application::Tick(const InputState& input, float dt) {
    timeSeconds_ += dt;
    titleAccumulator_ += dt;
    session_.Tick(input, dt);
    renderScene_ = BuildRenderScene(session_.World(), session_.Combat(), config_.width, config_.height, timeSeconds_, frameIndex_++);
}

void Application::Render() {
    const float pulse = 0.5f + 0.5f * std::sin(timeSeconds_ * 1.7f);
    if (config_.requireDxr && d3d_.InitResult().dxrSupported) {
        d3d_.RenderFrame(pulse, &Application::RenderDxrFrame, this);
        return;
    }
    d3d_.RenderFrame(pulse);
}

D3D12FrameCallbackResult Application::RenderDxrFrame(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, void* userData) {
    Application* app = static_cast<Application*>(userData);
    if (!app || !device || !commandList) {
        return D3D12FrameCallbackResult{false, nullptr, D3D12_GPU_DESCRIPTOR_HANDLE{}};
    }

    if (!app->dxrSceneResources_.Update(device, commandList, app->renderScene_)) {
        return D3D12FrameCallbackResult{false, nullptr, D3D12_GPU_DESCRIPTOR_HANDLE{}};
    }

    app->dxr_.Dispatch(commandList, app->dxrSceneResources_, app->config_.width, app->config_.height);
    app->dxrSceneResources_.InsertOutputUavBarrier(commandList);
    app->dxrSceneResources_.TransitionOutput(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    return D3D12FrameCallbackResult{
        true,
        app->dxrSceneResources_.DescriptorHeap(),
        app->dxrSceneResources_.OutputSrvGpu()
    };
}

void Application::UpdateWindowTitle(HWND hwnd, float elapsedSeconds) {
    (void)elapsedSeconds;
    if (titleAccumulator_ < 0.25f) {
        return;
    }
    titleAccumulator_ = 0.0f;

    const CombatSnapshot snapshot = session_.Snapshot();
    const char* mode = (config_.requireDxr && d3d_.InitResult().dxrSupported) ? "DXR" : "raster-test";
    char title[256]{};
    std::snprintf(title, sizeof(title),
        "Rogue96DXR | %s | room %d/%d | enemies %d | hp %.0f | seed 0x%08x",
        mode,
        snapshot.currentRoom + 1,
        session_.World().roomCount,
        snapshot.activeEnemies,
        snapshot.player.hp,
        session_.World().seed);
    SetWindowTextA(hwnd, title);
}

}
