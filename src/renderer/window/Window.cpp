////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Window.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    #include "Win32Window.h"
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    #include "XCBWindow.h"
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    #include "XLibWindow.h"
#endif

IWindow* createWindow()
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    return new Win32Window();
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    return new XCBWindow();
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    return new XLibWindow();
#else
    return nullptr;
#endif
}

