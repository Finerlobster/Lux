#include "Core/Types.h"
#include "Core/Assert.h"
#include "Core/WindowHandle.h"
#include "Vulkan/VulkanBackend.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static bool s_Running = true;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CLOSE:
            s_Running = false;
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

int main()
{
    HINSTANCE hinstance = GetModuleHandleA(nullptr);

    WNDCLASSEXA wc{};
    wc.cbSize        = sizeof(WNDCLASSEXA);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hinstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "LuxWindow";

    LX_ASSERT(RegisterClassExA(&wc), "Failed to register window class");

    HWND hwnd = CreateWindowExA(
        0, "LuxWindow", "Lux Renderer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 720,
        nullptr, nullptr, hinstance, nullptr);

    LX_ASSERT(hwnd != nullptr, "Failed to create Win32 window");

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    LX::WindowHandle windowHandle{};
    windowHandle.hwnd      = hwnd;
    windowHandle.hinstance = hinstance;

    LX::VulkanBackend backend;
    bool initialized = backend.Init(windowHandle);
    LX_ASSERT(initialized, "Failed to initialize Vulkan backend");

    MSG msg{};
    while (s_Running)
    {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        backend.BeginFrame();
        // draw calls come from engine
        backend.EndFrame();
    }

    backend.Shutdown();
    return 0;
}