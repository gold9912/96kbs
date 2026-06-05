#include "app/app.h"

#include <windows.h>

#include <cstring>

namespace {

rogue::Application* g_app = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        break;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

bool KeyDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

rogue::InputState ReadInput(HWND hwnd, const rogue::EngineConfig& config, const rogue::PlayerState& player) {
    (void)player;
    rogue::InputState input{};
    input.moveX = (KeyDown('D') ? 1.0f : 0.0f) - (KeyDown('A') ? 1.0f : 0.0f);
    input.moveY = (KeyDown('S') ? 1.0f : 0.0f) - (KeyDown('W') ? 1.0f : 0.0f);
    input.melee = KeyDown(VK_LBUTTON);
    input.ranged = KeyDown(VK_RBUTTON);
    input.control = KeyDown('Q');
    input.dash = KeyDown(VK_SPACE);

    POINT cursor{};
    GetCursorPos(&cursor);
    ScreenToClient(hwnd, &cursor);
    const float cx = static_cast<float>(config.width) * 0.5f;
    const float cy = static_cast<float>(config.height) * 0.5f;
    input.aimX = static_cast<float>(cursor.x) - cx;
    input.aimY = static_cast<float>(cursor.y) - cy;
    return input;
}

bool HasFlag(const char* commandLine, const char* flag) {
    return commandLine && std::strstr(commandLine, flag) != nullptr;
}

}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR commandLine, int showCmd) {
    rogue::EngineConfig config{};
    if (HasFlag(commandLine, "--allow-no-dxr") || HasFlag(commandLine, "--raster-test")) {
        config.requireDxr = false;
    }

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "Rogue96DXRWindow";
    RegisterClassExA(&wc);

    RECT rect{0, 0, static_cast<LONG>(config.width), static_cast<LONG>(config.height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        "Rogue96DXR",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!hwnd) {
        return 1;
    }

    rogue::Application app;
    g_app = &app;
    if (!app.Initialize(hwnd, config)) {
        return 2;
    }

    ShowWindow(hwnd, showCmd);
    UpdateWindow(hwnd);

    LARGE_INTEGER freq{};
    LARGE_INTEGER prev{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    MSG msg{};
    while (msg.message != WM_QUIT) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        const float dt = static_cast<float>(now.QuadPart - prev.QuadPart) / static_cast<float>(freq.QuadPart);
        prev = now;

        const rogue::InputState input = ReadInput(hwnd, config, rogue::PlayerState{});
        app.Tick(input, dt);
        app.Render();
        app.UpdateWindowTitle(hwnd, dt);
    }

    app.Shutdown();
    g_app = nullptr;
    return 0;
}
