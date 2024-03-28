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

namespace Reaper::Window
{
namespace
{
    KeyCode::type convert_win32_keycode(u32 key_code)
    {
        switch (key_code)
        {
        case VK_ESCAPE:
            return KeyCode::ESCAPE;
        case VK_RETURN:
            return KeyCode::ENTER;
        case VK_SPACE:
            return KeyCode::SPACE;
        case VK_RIGHT:
            return KeyCode::ARROW_RIGHT;
        case VK_LEFT:
            return KeyCode::ARROW_LEFT;
        case VK_DOWN:
            return KeyCode::ARROW_DOWN;
        case VK_UP:
            return KeyCode::ARROW_UP;
        case 'W':
            return KeyCode::W;
        case 'A':
            return KeyCode::A;
        case 'S':
            return KeyCode::S;
        case 'D':
            return KeyCode::D;
        case '1':
            return KeyCode::NUM_1;
        case '2':
            return KeyCode::NUM_2;
        case '3':
            return KeyCode::NUM_3;
        case '4':
            return KeyCode::NUM_4;
        case '5':
            return KeyCode::NUM_5;
        case '6':
            return KeyCode::NUM_6;
        case '7':
            return KeyCode::NUM_7;
        case '8':
            return KeyCode::NUM_8;
        case '9':
            return KeyCode::NUM_9;
        case '0':
            return KeyCode::NUM_0;
        default:
            return KeyCode::Invalid;
        }
    }
} // namespace
} // namespace Reaper::Window

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
    // There's a delay between when we're showing the window for the first time and actually presenting to it.
    // Windows paints the full background in white when that happens, resulting in very annoying flicker.
    // As a workaround, we override the WM_ERASEBKGND message and pretend that it's taken care of.
    case WM_ERASEBKGND:
        return (LRESULT)1; // Say we handled it.
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
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP: {
            bool is_pressed = message.message != WM_LBUTTONUP;
            eventOutput.emplace_back(Window::createButtonEvent(Window::MouseButton::Left, is_pressed));
            break;
        }
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_RBUTTONUP: {
            bool is_pressed = message.message != WM_RBUTTONUP;
            eventOutput.emplace_back(Window::createButtonEvent(Window::MouseButton::Right, is_pressed));
            break;
        }
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_MBUTTONUP: {
            bool is_pressed = message.message != WM_MBUTTONUP;
            eventOutput.emplace_back(Window::createButtonEvent(Window::MouseButton::Middle, is_pressed));
            break;
        }
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            bool horizontal = message.message == WM_MOUSEHWHEEL;

            // https://devblogs.microsoft.com/oldnewthing/20130123-00/?p=5473
            i32 delta = GET_WHEEL_DELTA_WPARAM(message.wParam) / WHEEL_DELTA;
            u32 key_state = GET_KEYSTATE_WPARAM(message.wParam);

            // On windows you need to emulate horizontal scroll manually when shift is pressed
            if (key_state & MK_SHIFT)
            {
                horizontal = true;
            }

            eventOutput.emplace_back(Window::createMouseWheelEvent(horizontal ? delta : 0, horizontal ? 0 : delta));
            break;
        }
        /*
        case WM_MOUSEMOVE:
        {
            constexpr LONG_PTR SIGNATURE_MASK = 0xFFFFFF00;
            constexpr LONG_PTR MOUSEEVENTF_FROMTOUCH = 0xFF515700;

            LONG_PTR extraInfo = GetMessageExtraInfo(); // NOTE: Careful, this call is context sensitive

            if ((extraInfo & SIGNATURE_MASK) == MOUSEEVENTF_FROMTOUCH)
                break;

            i16 x = (i16)LOWORD(message.lParam);
            i16 y = (i16)HIWORD(message.lParam);

            break;
        }
        */
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            u32 key_code = static_cast<u32>(message.wParam);
            eventOutput.emplace_back(Window::createKeyEvent(Window::convert_win32_keycode(key_code), true, key_code));
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            u32 key_code = static_cast<u32>(message.wParam);
            eventOutput.emplace_back(Window::createKeyEvent(Window::convert_win32_keycode(key_code), false, key_code));
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
    POINT point;
    Assert(GetCursorPos(&point) == TRUE);

    return MouseState{
        .pos_x = point.x,
        .pos_y = point.y,
    };
}
} // namespace Reaper
