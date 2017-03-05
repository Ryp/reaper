////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_PRESENTATIONSURFACE_INCLUDED
#define REAPER_PRESENTATIONSURFACE_INCLUDED

#include "api/Vulkan.h"

class IWindow;

void create_presentation_surface(VkInstance instance, VkSurfaceKHR& vkPresentationSurface, IWindow* window);

#endif // REAPER_PRESENTATIONSURFACE_INCLUDED

