#include "app.h"

#include <chrono>
#include <cstdint>

#include "Dx11Renderer.h"
#include "framework.h"

namespace {
constexpr std::int32_t kWindowWidth = 1280;
constexpr std::int32_t kWindowHeight = 720;
constexpr wchar_t kWindowClassName[] = L"CG2026task3Window";
constexpr wchar_t kWindowTitle[] = L"task3 DirectX11";
constexpr float kKeyboardMoveSpeed = 3.0f;
constexpr float kMouseSensitivity = 0.01f;

Dx11Renderer renderer;
bool isDragging = false;
POINT lastMousePos{};
bool isMoveForwardPressed = false;
bool isMoveBackwardPressed = false;
bool isMoveLeftPressed = false;
bool isMoveRightPressed = false;

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
        case WM_KEYDOWN:
            switch (wParam) {
                case 'A':
                    isMoveLeftPressed = true;
                    return 0;
                case 'D':
                    isMoveRightPressed = true;
                    return 0;
                case 'W':
                    isMoveForwardPressed = true;
                    return 0;
                case 'S':
                    isMoveBackwardPressed = true;
                    return 0;
                case VK_SPACE:
                    if ((lParam & (1 << 30)) == 0) {
                        renderer.toggleSceneAutoRotation();
                    }
                    return 0;
                default:
                    break;
            }
            return DefWindowProc(window, message, wParam, lParam);
        case WM_KEYUP:
            switch (wParam) {
                case 'A':
                    isMoveLeftPressed = false;
                    return 0;
                case 'D':
                    isMoveRightPressed = false;
                    return 0;
                case 'W':
                    isMoveForwardPressed = false;
                    return 0;
                case 'S':
                    isMoveBackwardPressed = false;
                    return 0;
                default:
                    break;
            }
            return DefWindowProc(window, message, wParam, lParam);
        case WM_KILLFOCUS:
            isMoveForwardPressed = false;
            isMoveBackwardPressed = false;
            isMoveLeftPressed = false;
            isMoveRightPressed = false;
            return 0;
        case WM_LBUTTONDOWN:
            isDragging = true;
            lastMousePos.x = static_cast<short>(LOWORD(lParam));
            lastMousePos.y = static_cast<short>(HIWORD(lParam));
            SetCapture(window);
            return 0;
        case WM_MOUSEMOVE:
            if (isDragging && (wParam & MK_LBUTTON) != 0) {
                const int mouseX = static_cast<short>(LOWORD(lParam));
                const int mouseY = static_cast<short>(HIWORD(lParam));
                const int dx = mouseX - lastMousePos.x;
                const int dy = mouseY - lastMousePos.y;
                renderer.adjustCamera(static_cast<float>(dx) * kMouseSensitivity,
                                      static_cast<float>(-dy) * kMouseSensitivity);
                lastMousePos.x = mouseX;
                lastMousePos.y = mouseY;
            }
            return 0;
        case WM_LBUTTONUP:
            isDragging = false;
            ReleaseCapture();
            return 0;
        case WM_DESTROY:
            isMoveForwardPressed = false;
            isMoveBackwardPressed = false;
            isMoveLeftPressed = false;
            isMoveRightPressed = false;
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

            float forwardAxis = 0.0f;
            float rightAxis = 0.0f;
            if (isMoveForwardPressed) {
                forwardAxis += 1.0f;
            }
            if (isMoveBackwardPressed) {
                forwardAxis -= 1.0f;
            }
            if (isMoveRightPressed) {
                rightAxis += 1.0f;
            }
            if (isMoveLeftPressed) {
                rightAxis -= 1.0f;
            }

            renderer.moveCamera(forwardAxis * kKeyboardMoveSpeed * deltaTime.count(),
                                rightAxis * kKeyboardMoveSpeed * deltaTime.count());
            renderer.renderFrame();
        }
    }

    return static_cast<int>(messageData.wParam);
}
