////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

#include <vulkan_loader/Vulkan.h>

namespace Reaper
{
VkShaderModule vulkan_create_shader_module(VkDevice device, const std::string& fileName);
} // namespace Reaper
