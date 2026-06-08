#include "app/app.h"

#include <windows.h>

#include <cstdlib>
#include <cstring>
#include <memory>

namespace {

rogue::Application* g_app = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            DestroyWindow(hwnd);
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
    rogue::ApplyMovementInputBindings(input, KeyDown('W'), KeyDown('A'), KeyDown('S'), KeyDown('D'));
    rogue::ApplyInputActionBindings(
        input,
        KeyDown('Q'),
        KeyDown('E'),
        KeyDown(VK_LBUTTON),
        KeyDown(VK_RBUTTON));
    input.dash = KeyDown(VK_SPACE);
    input.advanceFloor = KeyDown(VK_RETURN);
    if (KeyDown('1')) {
        rogue::ApplyNumberInputBinding(input, 0);
    } else if (KeyDown('2')) {
        rogue::ApplyNumberInputBinding(input, 1);
    } else if (KeyDown('3')) {
        rogue::ApplyNumberInputBinding(input, 2);
    }

    POINT cursor{};
    GetCursorPos(&cursor);
    ScreenToClient(hwnd, &cursor);
    rogue::ApplyScreenAimBinding(
        input,
        static_cast<float>(cursor.x),
        static_cast<float>(cursor.y),
        static_cast<float>(config.width),
        static_cast<float>(config.height));
    return input;
}

bool HasFlag(const char* commandLine, const char* flag) {
    return commandLine && std::strstr(commandLine, flag) != nullptr;
}

int ParseIntFlag(const char* commandLine, const char* flag, int fallback) {
    if (!commandLine || !flag) {
        return fallback;
    }
    const char* match = std::strstr(commandLine, flag);
    if (!match) {
        return fallback;
    }
    return std::atoi(match + std::strlen(flag));
}

int ClampInt(int value, int low, int high) {
    return value < low ? low : (value > high ? high : value);
}

void ApplyFrameLimit(uint32_t fpsLimit, LARGE_INTEGER frameStart, LARGE_INTEGER freq) {
    if (fpsLimit == 0 || freq.QuadPart <= 0) {
        return;
    }

    const double targetSeconds = 1.0 / static_cast<double>(fpsLimit);
    for (;;) {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        const double elapsedSeconds =
            static_cast<double>(now.QuadPart - frameStart.QuadPart) /
            static_cast<double>(freq.QuadPart);
        const double remainingSeconds = targetSeconds - elapsedSeconds;
        if (remainingSeconds <= 0.0002) {
            break;
        }

        const DWORD sleepMs = remainingSeconds > 0.002
            ? static_cast<DWORD>(remainingSeconds * 1000.0) - 1u
            : 0u;
        Sleep(sleepMs);
    }
}

}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR commandLine, int showCmd) {
    rogue::EngineConfig config{};
    if (HasFlag(commandLine, "--allow-no-dxr") || HasFlag(commandLine, "--raster-test")) {
        config.requireDxr = false;
    }
    config.width = static_cast<uint32_t>(ClampInt(ParseIntFlag(commandLine, "--width=", static_cast<int>(config.width)), 320, 7680));
    config.height = static_cast<uint32_t>(ClampInt(ParseIntFlag(commandLine, "--height=", static_cast<int>(config.height)), 240, 4320));
    config.renderScalePercent = static_cast<uint32_t>(ClampInt(
        ParseIntFlag(commandLine, "--render-scale=", static_cast<int>(config.renderScalePercent)),
        25,
        100));
    config.fpsLimit = static_cast<uint32_t>(ClampInt(ParseIntFlag(commandLine, "--fps-limit=", static_cast<int>(config.fpsLimit)), 0, 1000));
    config.renderQuality = static_cast<uint32_t>(ClampInt(
        ParseIntFlag(
            commandLine,
            "--rt-quality=",
            ParseIntFlag(commandLine, "--render-quality=", static_cast<int>(config.renderQuality))),
        0,
        2));
    config.startFloorIndex = ClampInt(ParseIntFlag(commandLine, "--start-floor=", config.startFloorIndex), 0, 5);
    config.smokeCombatRoom = ParseIntFlag(commandLine, "--smoke-combat-room=", config.smokeCombatRoom);
    const int smokeFrames = ParseIntFlag(commandLine, "--smoke-frames=", 0);

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

    auto app = std::make_unique<rogue::Application>();
    g_app = app.get();
    if (!app->Initialize(hwnd, config)) {
        g_app = nullptr;
        return 2;
    }

    ShowWindow(hwnd, showCmd);
    UpdateWindow(hwnd);

    LARGE_INTEGER freq{};
    LARGE_INTEGER prev{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    MSG msg{};
    int frameCount = 0;
    while (msg.message != WM_QUIT) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (msg.message == WM_QUIT) {
            break;
        }

        LARGE_INTEGER frameStart{};
        QueryPerformanceCounter(&frameStart);
        const float dt = static_cast<float>(frameStart.QuadPart - prev.QuadPart) / static_cast<float>(freq.QuadPart);
        prev = frameStart;

        const rogue::InputState input = ReadInput(hwnd, config, rogue::PlayerState{});
        app->Tick(input, dt);
        app->UpdateWindowTitle(hwnd, dt);
        app->Render();
        if (smokeFrames > 0 && ++frameCount >= smokeFrames) {
            DestroyWindow(hwnd);
        } else {
            ApplyFrameLimit(config.fpsLimit, frameStart, freq);
        }
    }

    app->Shutdown();
    g_app = nullptr;
    app.reset();
    return 0;
}
