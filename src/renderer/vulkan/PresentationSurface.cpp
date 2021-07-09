////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PresentationSurface.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#    include "renderer/window/Win32Window.h"
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#    include "renderer/window/XCBWindow.h"
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#    include "renderer/window/XLibWindow.h"
#else
#    error "Unsupported WSI!"
#endif

namespace Reaper
{
using namespace vk;

void vulkan_create_presentation_surface(VkInstance instance, VkSurfaceKHR& vkPresentationSurface, IWindow* window)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    Win32Window* win32Window = dynamic_cast<Win32Window*>(window);
    Assert(win32Window != nullptr);

    VkWin32SurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, // VkStructureType                  sType
        nullptr,                                         // const void                      *pNext
        0,                                               // VkWin32SurfaceCreateFlagsKHR     flags
        win32Window->m_instance,                         // HINSTANCE                        hinstance
        win32Window->m_handle                            // HWND                             hwnd
    };

    Assert(vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &vkPresentationSurface) == VK_SUCCESS);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    XCBWindow* xcbWindow = dynamic_cast<XCBWindow*>(window);
    Assert(xcbWindow != nullptr);

    VkXcbSurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, // VkStructureType                  sType
        nullptr,                                       // const void                      *pNext
        0,                                             // VkXcbSurfaceCreateFlagsKHR       flags
        xcbWindow->m_connection,                       // xcb_connection_t*                connection
        xcbWindow->m_handle                            // xcb_window_t                     window
    };

    Assert(vkCreateXcbSurfaceKHR(instance, &surface_create_info, nullptr, &vkPresentationSurface) == VK_SUCCESS);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    XLibWindow* xlibWindow = dynamic_cast<XLibWindow*>(window);
    Assert(xlibWindow != nullptr);

    VkXlibSurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR, // VkStructureType                sType
        nullptr,                                        // const void                    *pNext
        0,                                              // VkXlibSurfaceCreateFlagsKHR    flags
        xlibWindow->DisplayPtr,                         // Display                       *dpy
        xlibWindow->Handle                              // Window                         window
    };

    Assert(vkCreateXlibSurfaceKHR(instance, &surface_create_info, nullptr, &vkPresentationSurface) == VK_SUCCESS);
#else
#    error "Unsupported WSI!"
#endif
    Assert(vkPresentationSurface != VK_NULL_HANDLE);
}

bool vulkan_queue_family_has_presentation_support(VkPhysicalDevice device, uint32_t queueFamilyIndex, IWindow* window)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    static_cast<void>(window);
    return vkGetPhysicalDeviceWin32PresentationSupportKHR(device, queueFamilyIndex) == VK_TRUE;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    XCBWindow* xcbWindow = dynamic_cast<XCBWindow*>(window);
    Assert(xcbWindow != nullptr);

    xcb_screen_t* screen = xcbWindow->m_screen;
    Assert(screen != nullptr);

    xcb_visualid_t visualId = screen->root_visual;
    return vkGetPhysicalDeviceXcbPresentationSupportKHR(device, queueFamilyIndex, xcbWindow->m_connection, visualId)
           == VK_TRUE;
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    XLibWindow* xlibWindow = dynamic_cast<XLibWindow*>(window);
    Assert(xlibWindow != nullptr);

    XWindowAttributes* attributes = nullptr;
    Assert(XGetWindowAttributes(xlibWindow->DisplayPtr, xlibWindow->Handle, attributes)
           != 0); // Non-zero equals success in X.
    Assert(attributes != nullptr);

    Visual* visual = attributes->visual;
    Assert(visual != nullptr);

    VisualID visualId = XVisualIDFromVisual(visual);
    return vkGetPhysicalDeviceXlibPresentationSupportKHR(device, queueFamilyIndex, xlibWindow->DisplayPtr, visualId)
           == VK_TRUE;
#else
#    error "Unsupported WSI!"
#endif
}
} // namespace Reaper
