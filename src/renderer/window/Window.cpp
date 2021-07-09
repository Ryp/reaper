////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Window.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#    include "Win32Window.h"
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#    include "XCBWindow.h"
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#    include "XLibWindow.h"
#endif

namespace Reaper
{
IWindow* createWindow(const WindowCreationDescriptor& creationInfo)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    return new Win32Window(creationInfo);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    return new XCBWindow(creationInfo);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    return new XLibWindow(creationInfo);
#else
    static_cast<void>(creationInfo);
    return nullptr;
#endif
}
} // namespace Reaper
