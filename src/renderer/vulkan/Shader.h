////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

#include "api/Vulkan.h"

namespace Reaper
{
void vulkan_create_shader_module(VkShaderModule& shaderModule, VkDevice device, const std::string& fileName);
} // namespace Reaper
