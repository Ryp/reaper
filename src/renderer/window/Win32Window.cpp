////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Win32Window.h"

#include "Event.h"

#include "renderer/Renderer.h"

#define REAPER_WM_UPDATE_SIZE (WM_USER + 1)
#define REAPER_WM_CLOSE (WM_USER + 2)

namespace
{
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
    case WM_EXITSIZEMOVE:
        PostMessage(hWnd, REAPER_WM_UPDATE_SIZE, wParam, lParam);
        break;
    case WM_KEYDOWN:
    case WM_CLOSE:
        PostMessage(hWnd, REAPER_WM_CLOSE, wParam, lParam);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

struct WindowSize
{
    int width;
    int height;
};

WindowSize getWindowSize(HWND hWnd)
{
    RECT rect = {};

    Assert(GetClientRect(hWnd, &rect) == TRUE);

    return {
        rect.right - rect.left,
        rect.bottom - rect.top,
    };
}
} // namespace

// FIXME
#define REAPER_WINDOW_INFO "Reaper"

namespace Reaper
{
Win32Window::Win32Window(const WindowCreationDescriptor& creationInfo)
    : m_instance()
    , m_handle()
{
    // Make sure Windows doesn't do any rescaling behind our backs
    Assert(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE) != FALSE);

    m_instance = GetModuleHandle(nullptr);
    Assert(m_instance != nullptr);

    // Register window class
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = m_instance;
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

    constexpr i32 DefaultX = 0;
    constexpr i32 DefaultY = 0;

    RECT rect;
    rect.left = DefaultX;
    rect.top = DefaultY;
    rect.right = DefaultX + creationInfo.width;
    rect.bottom = DefaultY + creationInfo.height;

    const DWORD style = WS_POPUP;
    const HWND  parent = nullptr;

    const HMENU menu = nullptr;
    const BOOL  hasMenu = (menu == nullptr ? FALSE : TRUE);

    // Adjust rect to account for window decorations and dpi so we get the desired resolution
    Assert(AdjustWindowRectExForDpi(&rect, style, hasMenu, 0, 96) != FALSE);

    const LPVOID param = nullptr;

    // Create window
    m_handle = CreateWindow(REAPER_WINDOW_INFO, creationInfo.title, style, rect.left, rect.top, rect.right - rect.left,
                            rect.bottom - rect.top, parent, menu, m_instance, param);
    Assert(m_handle != nullptr);
}

Win32Window::~Win32Window()
{
    if (m_handle)
        Assert(DestroyWindow(m_handle) != FALSE);

    if (m_instance)
        Assert(UnregisterClass(REAPER_WINDOW_INFO, m_instance) != FALSE);
}

void Win32Window::map()
{
    LONG exStyle = GetWindowLong(m_handle, GWL_EXSTYLE);
    LONG style = GetWindowLong(m_handle, GWL_STYLE);

    SetWindowLong(m_handle, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUPWINDOW);
    SetWindowLong(m_handle, GWL_EXSTYLE, exStyle | WS_EX_TOPMOST);

    ShowWindow(m_handle, SW_SHOWNORMAL);

    UpdateWindow(m_handle);
}

void Win32Window::unmap()
{
    // FIXME
}

void Win32Window::pumpEvents(std::vector<Window::Event>& eventOutput)
{
    // Main message loop
    MSG message;

    // Process events
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
    {
        switch (message.message)
        {
        case REAPER_WM_UPDATE_SIZE: {
            const WindowSize window_size = getWindowSize(m_handle);
            eventOutput.emplace_back(Window::createResizeEvent(window_size.width, window_size.height));
            break;
        }
        case REAPER_WM_CLOSE:
            // eventOutput.emplace_back(convertXcbEvent(event));
            break;
        }
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}

MouseState Win32Window::get_mouse_state()
{
    return MouseState{};
}
} // namespace Reaper
