////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PresentationSurface.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    #include "renderer/window/Win32Window.h"
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    #include "renderer/window/XCBWindow.h"
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    #include "renderer/window/XLibWindow.h"
#else
    #error You are doing something wrong!
#endif

using namespace vk;

void create_presentation_surface(VkInstance instance, VkSurfaceKHR& vkPresentationSurface, IWindow* window)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    Win32Window* win32Window = dynamic_cast<Win32Window*>(window);
    Assert(win32Window != nullptr);

    VkWin32SurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,  // VkStructureType                  sType
        nullptr,                                          // const void                      *pNext
        0,                                                // VkWin32SurfaceCreateFlagsKHR     flags
        win32Window->Instance,                            // HINSTANCE                        hinstance
        win32Window->Handle                               // HWND                             hwnd
    };

    Assert(vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &vkPresentationSurface) == VK_SUCCESS);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    XCBWindow* xcbWindow = dynamic_cast<XCBWindow*>(window);
    Assert(xcbWindow != nullptr);

    VkXcbSurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,    // VkStructureType                  sType
        nullptr,                                          // const void                      *pNext
        0,                                                // VkXcbSurfaceCreateFlagsKHR       flags
        xcbWindow->Connection,                            // xcb_connection_t*                connection
        xcbWindow->Handle                                 // xcb_window_t                     window
    };

    Assert(vkCreateXcbSurfaceKHR(instance, &surface_create_info, nullptr, &vkPresentationSurface) == VK_SUCCESS);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    XLibWindow* xlibWindow = dynamic_cast<XLibWindow*>(window);
    Assert(xlibWindow != nullptr);

    VkXlibSurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,   // VkStructureType                sType
        nullptr,                                          // const void                    *pNext
        0,                                                // VkXlibSurfaceCreateFlagsKHR    flags
        xlibWindow->DisplayPtr,                           // Display                       *dpy
        xlibWindow->Handle                                // Window                         window
    };

    Assert(vkCreateXlibSurfaceKHR(instance, &surface_create_info, nullptr, &vkPresentationSurface) == VK_SUCCESS);
#endif
    Assert(vkPresentationSurface != VK_NULL_HANDLE);
}

