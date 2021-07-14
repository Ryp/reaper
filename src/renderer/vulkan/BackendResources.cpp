////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "BackendResources.h"

#include "Backend.h"

#include "common/Log.h"

namespace Reaper
{
void create_backend_resources(ReaperRoot& root, VulkanBackend& backend)
{
    backend.resources = new BackendResources;

    BackendResources& resources = *backend.resources;

    // Create command buffer
    VkCommandPoolCreateInfo poolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                              backend.physicalDeviceInfo.graphicsQueueFamilyIndex};

    Assert(vkCreateCommandPool(backend.device, &poolCreateInfo, nullptr, &resources.graphicsCommandPool) == VK_SUCCESS);
    log_debug(root, "vulkan: created command pool with handle: {}", static_cast<void*>(resources.graphicsCommandPool));

    VkCommandBufferAllocateInfo cmdBufferAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
                                                      resources.graphicsCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                      1};

    Assert(vkAllocateCommandBuffers(backend.device, &cmdBufferAllocInfo, &resources.gfxCmdBuffer.handle) == VK_SUCCESS);
    log_debug(
        root, "vulkan: created command buffer with handle: {}", static_cast<void*>(resources.gfxCmdBuffer.handle));

    const glm::uvec2 swapchain_extent(backend.presentInfo.surfaceExtent.width,
                                      backend.presentInfo.surfaceExtent.height);

    resources.mesh_cache = create_mesh_cache(root, backend);
    resources.material_resources = create_material_resources(root, backend);
    resources.cull_resources = create_culling_resources(root, backend);
    resources.shadow_map_resources = create_shadow_map_resources(root, backend);
    resources.main_pass_resources = create_main_pass_resources(root, backend, swapchain_extent);
    resources.histogram_pass_resources = create_histogram_pass_resources(root, backend);
    resources.swapchain_pass_resources = create_swapchain_pass_resources(root, backend, swapchain_extent);
    resources.frame_sync_resources = create_frame_sync_resources(root, backend);
}

void destroy_backend_resources(VulkanBackend& backend)
{
    BackendResources& resources = *backend.resources;

    destroy_swapchain_pass_resources(backend, resources.swapchain_pass_resources);
    destroy_histogram_pass_resources(backend, resources.histogram_pass_resources);
    destroy_main_pass_resources(backend, resources.main_pass_resources);
    destroy_shadow_map_resources(backend, resources.shadow_map_resources);
    destroy_culling_resources(backend, resources.cull_resources);
    destroy_material_resources(backend, resources.material_resources);
    destroy_mesh_cache(backend, resources.mesh_cache);
    destroy_frame_sync_resources(backend, resources.frame_sync_resources);

    vkFreeCommandBuffers(backend.device, resources.graphicsCommandPool, 1, &resources.gfxCmdBuffer.handle);
    vkDestroyCommandPool(backend.device, resources.graphicsCommandPool, nullptr);

    delete backend.resources;
    backend.resources = nullptr;
}
} // namespace Reaper
