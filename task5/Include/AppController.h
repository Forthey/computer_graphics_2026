#pragma once

#include <windows.h>

#include "Dx11Renderer.h"

class AppController {
public:
    AppController() = default;
    AppController(const AppController&) = delete;
    AppController& operator=(const AppController&) = delete;
    AppController(AppController&&) = delete;
    AppController& operator=(AppController&&) = delete;

    bool initializeWindowResources(HWND window);
    void updateAndRender(float deltaTimeSeconds);
    LRESULT handleWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

private:
    static float movementAxis(bool positiveDirectionPressed, bool negativeDirectionPressed);

    void resetMovementState();
    LRESULT handleResize(WPARAM wParam, LPARAM lParam);
    LRESULT handlePaint(HWND window) const;
    LRESULT handleKeyDown(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleKeyUp(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void beginMouseDrag(HWND window, LPARAM lParam);
    void handleMouseMove(WPARAM wParam, LPARAM lParam);
    void endMouseDrag();

    Dx11Renderer m_renderer;
    bool m_isDragging = false;
    POINT m_lastMousePos{};
    bool m_isMoveForwardPressed = false;
    bool m_isMoveBackwardPressed = false;
    bool m_isMoveLeftPressed = false;
    bool m_isMoveRightPressed = false;
};

