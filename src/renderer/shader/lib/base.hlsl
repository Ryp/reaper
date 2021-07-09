////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// ALL shaders MUST include this header before anything else
#ifndef LIB_BASE_INCLUDED
#define LIB_BASE_INCLUDED

// This define is useful when sharing code with the c++ side.
#define REAPER_SHADER_CODE

// Available platforms
#define REAPER_PLATFORM_PC      0
#define REAPER_PLATFORM_ORBIS   1

// Detect platform
#ifdef __ORBIS__
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
    #define VK_CONSTANT(_index)         [[vk::constant_id(_index)]]
    #define VK_BINDING(_binding, _set)  [[vk::binding(_binding, _set)]]
    #define VK_LOCATION(_location)      [[vk::location(_location)]]
#else
    // Not supported
    #define VK_PUSH_CONSTANT()
    #define VK_CONSTANT(_index)
    #define VK_BINDING(_binding, _set)
    #define VK_LOCATION(_location)
#endif

#include "common.hlsl"

#endif
