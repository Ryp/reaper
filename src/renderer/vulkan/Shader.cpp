////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Shader.h"

#include "Debug.h"

#include "core/fs/FileLoading.h"

namespace Reaper
{
using namespace vk;

void vulkan_create_shader_module(VkShaderModule& shaderModule, VkDevice device, const std::string& fileName)
{
    std::vector<char> fileContents = readWholeFile(fileName);

    VkShaderModuleCreateInfo shader_module_create_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
                                                          fileContents.size(),
                                                          reinterpret_cast<const uint32_t*>(&fileContents[0])};

    Assert(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shaderModule) == VK_SUCCESS);

    VulkanSetDebugName(device, shaderModule, fileName.c_str());
}
} // namespace Reaper
