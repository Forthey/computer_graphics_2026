#include "framework.h"
#include "app.h"
#include "dx11_renderer.h"

namespace
{
constexpr UINT kWindowWidth = 1280;
constexpr UINT kWindowHeight = 720;
constexpr wchar_t kWindowClassName[] = L"CG2026Task1Window";
constexpr wchar_t kWindowTitle[] = L"Task1 DirectX11";

Dx11Renderer g_renderer;

ATOM RegisterWindowClass(HINSTANCE hInstance);
BOOL InitMainWindow(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
} // namespace

int RunApp(HINSTANCE hInstance, int nCmdShow)
{
    RegisterWindowClass(hInstance);

    if (!InitMainWindow(hInstance, nCmdShow))
    {
        return FALSE;
    }

    MSG msg{};
    bool exitRequested = false;

    while (!exitRequested)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                exitRequested = true;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!exitRequested)
        {
            g_renderer.RenderFrame();
        }
    }

    return static_cast<int>(msg.wParam);
}

namespace
{
ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = hInstance;
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    return RegisterClassExW(&windowClass);
}

BOOL InitMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    RECT rc{ 0, 0, static_cast<LONG>(kWindowWidth), static_cast<LONG>(kWindowHeight) };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND windowHandle = CreateWindowW(
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (windowHandle == nullptr)
    {
        return FALSE;
    }

    if (!g_renderer.Initialize(windowHandle))
    {
        DestroyWindow(windowHandle);
        return FALSE;
    }

    ShowWindow(windowHandle, nCmdShow);
    UpdateWindow(windowHandle);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            const UINT width = static_cast<UINT>(LOWORD(lParam));
            const UINT height = static_cast<UINT>(HIWORD(lParam));
            g_renderer.Resize(width, height);
        }
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        g_renderer.Shutdown();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}
} // namespace
