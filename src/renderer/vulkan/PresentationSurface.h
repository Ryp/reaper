////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_PRESENTATIONSURFACE_INCLUDED
#define REAPER_PRESENTATIONSURFACE_INCLUDED

#include "renderer/Renderer.h"

#include "api/Vulkan.h"

struct WindowParameters
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    HINSTANCE           Instance;
    HWND                Handle;
    WindowParameters() : Instance(), Handle() {}
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    xcb_connection_t*   Connection;
    xcb_window_t        Handle;
    WindowParameters() : Connection(), Handle() {}
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    Display*            DisplayPtr;
    Window              Handle;
    WindowParameters() : DisplayPtr(), Handle() {}
#endif
};

class Window {
public:
    Window();
    ~Window();

    bool              Create(const char *title);
    bool              renderLoop(AbstractRenderer* renderer) const;
    WindowParameters  GetParameters() const;

private:
    WindowParameters  _parameters;
};

void createPresentationSurface(VkInstance instance, const WindowParameters& params, VkSurfaceKHR& vkPresentationSurface);

#endif // REAPER_PRESENTATIONSURFACE_INCLUDED
