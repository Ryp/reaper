////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// ALL shaders MUST include this header before anything else
#ifndef LIB_BASE_INCLUDED
#define LIB_BASE_INCLUDED

// Available platforms
#define REAPER_PLATFORM_PC      0
#define REAPER_PLATFORM_ORBIS   1

// Detect platform
#ifdef __PSSL__
    #include "platform_orbis.hlsl"
    #define REAPER_PLATFORM REAPER_PLATFORM_ORBIS
    #define REAPER_USE_VULKAN_BINDING 0
#else
    #define REAPER_PLATFORM REAPER_PLATFORM_PC
    #define REAPER_USE_VULKAN_BINDING 1
#endif

#ifndef REAPER_PLATFORM
    #error platform must be defined
#endif

// Vulkan compat
#if REAPER_USE_VULKAN_BINDING
    #define VK_PUSH_CONSTANT()          [[vk::push_constant]]
    #define VK_BINDING(_binding, _set)  [[vk::binding(_binding, _set)]]
#else
    // Not supported
    #define VK_PUSH_CONSTANT()
    #define VK_BINDING(_binding, _set)
#endif

#endif
