#include "app.h"

#include <chrono>
#include <cstdint>

#include "AppController.h"
#include "framework.h"

namespace {
constexpr std::int32_t kWindowWidth = 1280;
constexpr std::int32_t kWindowHeight = 720;
constexpr wchar_t kWindowClassName[] = L"CG2026task5Window";
constexpr wchar_t kWindowTitle[] = L"task5 DirectX11";

AppController appController;

bool initMainWindow(HINSTANCE hInstance, int commandShowMode) {
    RECT windowBounds{0, 0, kWindowWidth, kWindowHeight};
    AdjustWindowRect(&windowBounds, WS_OVERLAPPEDWINDOW, false);

    HWND window = CreateWindowW(kWindowClassName, kWindowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                windowBounds.right - windowBounds.left, windowBounds.bottom - windowBounds.top, nullptr,
                                nullptr, hInstance, nullptr);

    if (window == nullptr) {
        return false;
    }

    if (!appController.initializeWindowResources(window)) {
        DestroyWindow(window);
        return false;
    }

    ShowWindow(window, commandShowMode);
    UpdateWindow(window);
    return true;
}

LRESULT CALLBACK handleWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return appController.handleWindowMessage(window, message, wParam, lParam);
}

bool registerWindowClass(HINSTANCE hInstance) {
    WNDCLASSEXW windowDefinition{};
    windowDefinition.cbSize = sizeof(WNDCLASSEX);
    windowDefinition.style = CS_HREDRAW | CS_VREDRAW;
    windowDefinition.lpfnWndProc = handleWindowMessage;
    windowDefinition.hInstance = hInstance;
    windowDefinition.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    windowDefinition.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowDefinition.hbrBackground = nullptr;
    windowDefinition.lpszClassName = kWindowClassName;
    windowDefinition.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    return RegisterClassExW(&windowDefinition) != 0;
}
}  // namespace

int runApp(HINSTANCE hInstance, int commandShowMode) {
    if (!registerWindowClass(hInstance)) {
        return 0;
    }

    if (!initMainWindow(hInstance, commandShowMode)) {
        return 0;
    }

    MSG messageData{};
    bool exitRequested = false;
    auto lastTick = std::chrono::steady_clock::now();

    while (!exitRequested) {
        while (PeekMessage(&messageData, nullptr, 0, 0, PM_REMOVE)) {
            if (messageData.message == WM_QUIT) {
                exitRequested = true;
                break;
            }

            TranslateMessage(&messageData);
            DispatchMessage(&messageData);
        }

        if (!exitRequested) {
            const auto currentTick = std::chrono::steady_clock::now();
            const std::chrono::duration<float> deltaTime = currentTick - lastTick;
            lastTick = currentTick;
            appController.updateAndRender(deltaTime.count());
        }
    }

    return static_cast<int>(messageData.wParam);
}

