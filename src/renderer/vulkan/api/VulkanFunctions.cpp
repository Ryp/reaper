////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Vulkan.h"

namespace vk {

#define REAPER_VK_EXPORTED_FUNCTION(func)           PFN_##func func;
#define REAPER_VK_GLOBAL_LEVEL_FUNCTION(func)       PFN_##func func;
#define REAPER_VK_INSTANCE_LEVEL_FUNCTION(func)     PFN_##func func;
#define REAPER_VK_DEVICE_LEVEL_FUNCTION(func)       PFN_##func func;

#include "VulkanSymbolHelper.inl"

}
