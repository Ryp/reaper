////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
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
    // FIXME https://github.com/KhronosGroup/glslang/issues/1629
    #if defined(_GLSLANG)
        #define VK_PUSH_CONSTANT_HELPER(_type) VK_PUSH_CONSTANT() ConstantBuffer<_type>
    #else
        #define VK_PUSH_CONSTANT_HELPER(_type) VK_PUSH_CONSTANT() _type
    #endif
    #define VK_CONSTANT(_index)         [[vk::constant_id(_index)]]
    #define VK_BINDING(_set, _binding) [[vk::binding(_binding, _set)]]
    #define VK_LOCATION(_location)      [[vk::location(_location)]]
#else
    // Not supported
    #define VK_PUSH_CONSTANT()
    #define VK_PUSH_CONSTANT_HELPER(_type) _type
    #define VK_CONSTANT(_index)
    #define VK_BINDING(_set, _binding)
    #define VK_LOCATION(_location)
#endif

// FIXME This is very wrong but let's wait until glslang catches up
// https://github.com/KhronosGroup/glslang/issues/1637
#if defined(_GLSLANG)
    #define NonUniformResourceIndex(x) x
#endif

#include "common.hlsl"

#endif
