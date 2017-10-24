////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Win32Window.h"

#include "renderer/Renderer.h"

namespace
{
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
    case WM_EXITSIZEMOVE:
        PostMessage(hWnd, WM_USER + 1, wParam, lParam);
        break;
    case WM_KEYDOWN:
    case WM_CLOSE:
        PostMessage(hWnd, WM_USER + 2, wParam, lParam);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
} // namespace

#define REAPER_WINDOW_INFO "Reaper"

namespace Reaper
{
Win32Window::Win32Window(const WindowCreationDescriptor& creationInfo)
    : Instance()
    , Handle()
{
    Instance = GetModuleHandle(nullptr);
    Assert(Instance != nullptr);

    // Register window class
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = Instance;
    wcex.hIcon = nullptr;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = REAPER_WINDOW_INFO;
    wcex.hIconSm = nullptr;

    if (!RegisterClassEx(&wcex))
    {
        AssertUnreachable();
        return;
    }

    constexpr u32 DefaultX = 40;
    constexpr u32 DefaultY = DefaultX;

    RECT rect;
    rect.left = DefaultX;
    rect.top = DefaultY;
    rect.right = creationInfo.width + DefaultX;
    rect.bottom = creationInfo.height + DefaultY;

    const DWORD style = WS_OVERLAPPEDWINDOW;
    const HWND  parent = nullptr;

    const HMENU menu = nullptr;
    const BOOL  hasMenu = (menu == nullptr ? FALSE : TRUE);

    // Adjust rect to account for window decorations so we get the desired resolution
    Assert(AdjustWindowRect(&rect, style, hasMenu) != FALSE);

    const LPVOID param = nullptr;

    // Create window
    Handle = CreateWindow(REAPER_WINDOW_INFO, creationInfo.title, style, rect.left, rect.top, rect.right - rect.left,
                          rect.bottom - rect.top, parent, menu, Instance, param);
    Assert(Handle != nullptr);
}

Win32Window::~Win32Window()
{
    if (Handle)
        Assert(DestroyWindow(Handle) != FALSE);

    if (Instance)
        Assert(UnregisterClass(REAPER_WINDOW_INFO, Instance) != FALSE);
}

bool Win32Window::renderLoop(AbstractRenderer* renderer)
{
    // Display window
    ShowWindow(Handle, SW_SHOWNORMAL);
    UpdateWindow(Handle);

    // Main message loop
    MSG  message;
    bool loop = true;
    bool resize = false;

    while (loop)
    {
        if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            // Process events
            switch (message.message)
            {
            // Resize
            case WM_USER + 1:
                resize = true;
                break;
            // Close
            case WM_USER + 2:
                loop = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
        else
        {
            // Draw
            if (resize)
            {
                resize = false;
                //                 if( !renderer.OnWindowSizeChanged() ) {
                //                     loop = false;
                //                 }
            }
            renderer->render();
        }
    }
    return true;
}
} // namespace Reaper