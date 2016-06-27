////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_SHADER_INCLUDED
#define REAPER_SHADER_INCLUDED

#include <string>

#include "api/Vulkan.h"

void createShaderModule(VkShaderModule& shaderModule, VkDevice device, const std::string& fileName);

#endif // REAPER_SHADER_INCLUDED
