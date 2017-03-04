////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_VULKANFUNCTIONS_INCLUDED
#define REAPER_VULKANFUNCTIONS_INCLUDED

#include "Vulkan.h"

namespace vk {

#define REAPER_VK_EXPORTED_FUNCTION(func)       extern PFN_##func func;
#define REAPER_VK_GLOBAL_LEVEL_FUNCTION(func)   extern PFN_##func func;
#define REAPER_VK_INSTANCE_LEVEL_FUNCTION(func) extern PFN_##func func;
#define REAPER_VK_DEVICE_LEVEL_FUNCTION(func)   extern PFN_##func func;

#include "VulkanSymbolHelper.inl"

}

#endif // REAPER_VULKANFUNCTIONS_INCLUDED
