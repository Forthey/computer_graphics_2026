#include "app.h"

#include <cstdint>

#include "dx11_renderer.h"
#include "framework.h"

namespace {
constexpr std::int32_t kWindowWidth = 1280;
constexpr std::int32_t kWindowHeight = 720;
constexpr wchar_t kWindowClassName[] = L"CG2026Task2Window";
constexpr wchar_t kWindowTitle[] = L"Task2 DirectX11";

Dx11Renderer renderer;

bool initMainWindow(HINSTANCE hInstance, int commandShowMode) {
    RECT windowBounds{0, 0, kWindowWidth, kWindowHeight};
    AdjustWindowRect(&windowBounds, WS_OVERLAPPEDWINDOW, false);

    HWND window = CreateWindowW(kWindowClassName, kWindowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                windowBounds.right - windowBounds.left, windowBounds.bottom - windowBounds.top, nullptr,
                                nullptr, hInstance, nullptr);

    if (window == nullptr) {
        return false;
    }

    if (!renderer.initialize(window)) {
        DestroyWindow(window);
        return false;
    }

    ShowWindow(window, commandShowMode);
    UpdateWindow(window);
    return true;
}

LRESULT CALLBACK wndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                const std::uint32_t width = static_cast<std::uint32_t>(LOWORD(lParam));
                const std::uint32_t height = static_cast<std::uint32_t>(HIWORD(lParam));
                renderer.resize(width, height);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            BeginPaint(window, &paint);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_DESTROY:
            renderer.shutdown();
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(window, message, wParam, lParam);
    }
}

bool registerWindowClass(HINSTANCE hInstance) {
    WNDCLASSEXW windowDefinition{};
    windowDefinition.cbSize = sizeof(WNDCLASSEX);
    windowDefinition.style = CS_HREDRAW | CS_VREDRAW;
    windowDefinition.lpfnWndProc = wndProc;
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
            renderer.renderFrame();
        }
    }

    return static_cast<int>(messageData.wParam);
}
