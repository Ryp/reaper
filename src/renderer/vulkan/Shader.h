////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_SHADER_INCLUDED
#define REAPER_SHADER_INCLUDED

#include <string>

#include "api/Vulkan.h"

void vulkan_create_shader_module(VkShaderModule& shaderModule, VkDevice device, const std::string& fileName);

#endif // REAPER_SHADER_INCLUDED
