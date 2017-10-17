////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "VulkanRenderer.h"

#include "common/Log.h"

#include <cmath>
#include <cstring>

#include "Memory.h"
#include "Shader.h"

#include "mesh/ModelLoader.h"

#include "renderer/texture/TextureLoader.h"

#include "allocator/GPUStackAllocator.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Reaper
{
using namespace vk;

void test_vulkan_renderer(ReaperRoot& root, VulkanBackend& backend)
{
    log_info(root, "vulkan: running test function");

    VkCommandPool gfxCmdPool = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo cmd_pool_create_info = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,   // VkStructureType              sType
        nullptr,                                      // const void*                  pNext
        0,                                            // VkCommandPoolCreateFlags     flags
        backend.physicalDeviceInfo.graphicsQueueIndex // uint32_t                     queueFamilyIndex
    };
    Assert(vkCreateCommandPool(backend.device, &cmd_pool_create_info, nullptr, &gfxCmdPool) == VK_SUCCESS);

    vkDestroyCommandPool(backend.device, gfxCmdPool, nullptr);
}
}
