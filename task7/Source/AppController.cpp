#include "AppController.h"

#include <cstdint>
#include <cwchar>

namespace {
constexpr float kKeyboardMoveSpeed = 3.0f;
constexpr float kMouseSensitivity = 0.01f;
constexpr float kFpsUpdateIntervalSeconds = 0.25f;
constexpr wchar_t kBaseWindowTitle[] = L"task7 DirectX11";
}

bool AppController::initializeWindowResources(HWND window) {
    m_window = window;
    m_fpsAccumulatedSeconds = 0.0f;
    m_fpsFrameCount = 0u;
    m_lastPublishedFps = 0u;
    updateWindowTitle();
    return m_renderer.initialize(window);
}

void AppController::updateAndRender(float deltaTimeSeconds) {
    const float forwardAxis = movementAxis(m_isMoveForwardPressed, m_isMoveBackwardPressed);
    const float rightAxis = movementAxis(m_isMoveRightPressed, m_isMoveLeftPressed);

    m_renderer.moveCamera(forwardAxis * kKeyboardMoveSpeed * deltaTimeSeconds,
                          rightAxis * kKeyboardMoveSpeed * deltaTimeSeconds);
    m_renderer.renderFrame();

    m_fpsAccumulatedSeconds += deltaTimeSeconds;
    ++m_fpsFrameCount;
    if (m_fpsAccumulatedSeconds >= kFpsUpdateIntervalSeconds) {
        m_lastPublishedFps =
            static_cast<std::uint32_t>(static_cast<float>(m_fpsFrameCount) / m_fpsAccumulatedSeconds + 0.5f);
        m_fpsAccumulatedSeconds = 0.0f;
        m_fpsFrameCount = 0u;
        updateWindowTitle();
    }
}

LRESULT AppController::handleWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_SIZE:
            return handleResize(wParam, lParam);
        case WM_PAINT:
            return handlePaint(window);
        case WM_KEYDOWN:
            return handleKeyDown(window, message, wParam, lParam);
        case WM_KEYUP:
            return handleKeyUp(window, message, wParam, lParam);
        case WM_KILLFOCUS:
            resetMovementState();
            return 0;
        case WM_LBUTTONDOWN:
            beginMouseDrag(window, lParam);
            return 0;
        case WM_MOUSEMOVE:
            handleMouseMove(wParam, lParam);
            return 0;
        case WM_LBUTTONUP:
            endMouseDrag();
            return 0;
        case WM_DESTROY:
            resetMovementState();
            m_isDragging = false;
            m_renderer.shutdown();
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(window, message, wParam, lParam);
    }
}

float AppController::movementAxis(bool positiveDirectionPressed, bool negativeDirectionPressed) {
    float axis = 0.0f;
    if (positiveDirectionPressed) {
        axis += 1.0f;
    }
    if (negativeDirectionPressed) {
        axis -= 1.0f;
    }
    return axis;
}

void AppController::resetMovementState() {
    m_isMoveForwardPressed = false;
    m_isMoveBackwardPressed = false;
    m_isMoveLeftPressed = false;
    m_isMoveRightPressed = false;
}

void AppController::updateWindowTitle() {
    if (m_window == nullptr) {
        return;
    }

    wchar_t title[160]{};
    swprintf_s(title, L"%s | FPS: %u | Mode: %s", kBaseWindowTitle, m_lastPublishedFps,
               m_renderer.postProcessModeName());
    SetWindowTextW(m_window, title);
}

LRESULT AppController::handleResize(WPARAM wParam, LPARAM lParam) {
    if (wParam != SIZE_MINIMIZED) {
        const std::uint32_t width = static_cast<std::uint32_t>(LOWORD(lParam));
        const std::uint32_t height = static_cast<std::uint32_t>(HIWORD(lParam));
        m_renderer.resize(width, height);
    }
    return 0;
}

LRESULT AppController::handlePaint(HWND window) const {
    PAINTSTRUCT paint{};
    BeginPaint(window, &paint);
    EndPaint(window, &paint);
    return 0;
}

LRESULT AppController::handleKeyDown(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (wParam) {
        case 'A':
            m_isMoveLeftPressed = true;
            return 0;
        case 'D':
            m_isMoveRightPressed = true;
            return 0;
        case 'W':
            m_isMoveForwardPressed = true;
            return 0;
        case 'S':
            m_isMoveBackwardPressed = true;
            return 0;
        case VK_SPACE:
            if ((lParam & (1 << 30)) == 0) {
                m_renderer.toggleSceneAutoRotation();
            }
            return 0;
        case 'P':
            if ((lParam & (1 << 30)) == 0) {
                m_renderer.cyclePostProcessMode();
                updateWindowTitle();
            }
            return 0;
        default:
            return DefWindowProc(window, message, wParam, lParam);
    }
}

LRESULT AppController::handleKeyUp(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (wParam) {
        case 'A':
            m_isMoveLeftPressed = false;
            return 0;
        case 'D':
            m_isMoveRightPressed = false;
            return 0;
        case 'W':
            m_isMoveForwardPressed = false;
            return 0;
        case 'S':
            m_isMoveBackwardPressed = false;
            return 0;
        default:
            return DefWindowProc(window, message, wParam, lParam);
    }
}

void AppController::beginMouseDrag(HWND window, LPARAM lParam) {
    m_isDragging = true;
    m_lastMousePos.x = static_cast<short>(LOWORD(lParam));
    m_lastMousePos.y = static_cast<short>(HIWORD(lParam));
    SetCapture(window);
}

void AppController::handleMouseMove(WPARAM wParam, LPARAM lParam) {
    if (!m_isDragging || (wParam & MK_LBUTTON) == 0) {
        return;
    }

    const int mouseX = static_cast<short>(LOWORD(lParam));
    const int mouseY = static_cast<short>(HIWORD(lParam));
    const int dx = mouseX - m_lastMousePos.x;
    const int dy = mouseY - m_lastMousePos.y;
    m_renderer.adjustCamera(static_cast<float>(dx) * kMouseSensitivity,
                            static_cast<float>(-dy) * kMouseSensitivity);
    m_lastMousePos.x = mouseX;
    m_lastMousePos.y = mouseY;
}

void AppController::endMouseDrag() {
    if (m_isDragging) {
        m_isDragging = false;
        ReleaseCapture();
    }
}
