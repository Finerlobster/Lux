#include "Core/Types.h"
#include "Core/Assert.h"
#include "Core/WindowHandle.h"
#include "Core/Vertex.h"
#include "Vulkan/VulkanBackend.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ── Win32 Window Procedure ────────────────────────────────────────────────────
// Receives messages from the OS. We handle close and destroy,
// pass everything else to default handling.
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

// ── Entry Point ───────────────────────────────────────────────────────────────
int main()
{
    // Step 1: Get the application instance handle
    HINSTANCE hinstance = GetModuleHandleA(nullptr);

    // Step 2: Register the window class
    WNDCLASSEXA wc{};
    wc.cbSize        = sizeof(WNDCLASSEXA);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hinstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "LuxWindow";

    LX_ASSERT(RegisterClassExA(&wc), "Failed to register window class");

    // Step 3: Create the window
    HWND hwnd = CreateWindowExA(
        0,
        "LuxWindow",        // class name — must match registration
        "Lux Renderer",     // title bar text
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,  // position — let Windows decide
        1280, 720,                      // width, height
        nullptr,            // no parent window
        nullptr,            // no menu
        hinstance,
        nullptr             // no extra data
    );

    LX_ASSERT(hwnd != nullptr, "Failed to create Win32 window");

    // Step 4: Show the window
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Step 5: Build the window handle for Lux
    LX::WindowHandle windowHandle{};
    windowHandle.hwnd      = hwnd;
    windowHandle.hinstance = hinstance;

    // Step 6: Initialize Lux
    LX::VulkanBackend backend;
    bool initialized = backend.Init(windowHandle);
    LX_ASSERT(initialized, "Failed to initialize Vulkan backend");

    LX::Vertex triangleVertices[] =
    {
        {  0.0f, -0.5f,   1.0f, 0.0f, 0.0f },
        {  0.5f,  0.5f,   0.0f, 1.0f, 0.0f },
        { -0.5f,  0.5f,   0.0f, 0.0f, 1.0f },
    };

    LX::BufferHandle triangleBuffer = backend.CreateVertexBuffer(
        triangleVertices, sizeof(triangleVertices));

    LX_ASSERT(triangleBuffer.IsValid(), "Failed to create triangle buffer");

    // Step 7: Message + render loop
    MSG msg{};
    while (s_Running)
    {
        // Drain all pending OS messages
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        // Frame work goes here
        backend.BeginFrame();
        backend.DrawVertexBuffer(triangleBuffer, 3);
        backend.EndFrame();
    }

    // Step 8: Shutdown
    backend.Shutdown();
    return 0;
}