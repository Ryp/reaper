////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
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

    Assert(vkCreateCommandPool(backend.device, &poolCreateInfo, nullptr, &resources.gfxCommandPool) == VK_SUCCESS);
    log_debug(root, "vulkan: created command pool with handle: {}", static_cast<void*>(resources.gfxCommandPool));

    VkCommandBufferAllocateInfo cmdBufferAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
                                                      resources.gfxCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};

    Assert(vkAllocateCommandBuffers(backend.device, &cmdBufferAllocInfo, &resources.gfxCmdBuffer.handle) == VK_SUCCESS);
    log_debug(root, "vulkan: created command buffer with handle: {}",
              static_cast<void*>(resources.gfxCmdBuffer.handle));

    const VkEventCreateInfo event_info = {
        VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
        nullptr,
        VK_EVENT_CREATE_DEVICE_ONLY_BIT,
    };
    Assert(vkCreateEvent(backend.device, &event_info, nullptr, &resources.event) == VK_SUCCESS);

    resources.shader_modules = create_shader_modules(root, backend);
    resources.samplers_resources = create_sampler_resources(root, backend);
    resources.framegraph_resources = create_framegraph_resources(root, backend);
    resources.audio_resources = create_audio_resources(root, backend, resources.shader_modules);
    resources.cull_resources = create_culling_resources(root, backend, resources.shader_modules);
    resources.frame_sync_resources = create_frame_sync_resources(root, backend);
    resources.gui_pass_resources = create_gui_pass_resources(root, backend, resources.shader_modules);
    resources.histogram_pass_resources = create_histogram_pass_resources(root, backend, resources.shader_modules);
    resources.lighting_resources = create_lighting_pass_resources(root, backend);
    resources.tiled_lighting_resources = create_tiled_lighting_pass_resources(root, backend, resources.shader_modules);
    resources.forward_pass_resources = create_forward_pass_resources(root, backend, resources.shader_modules);
    resources.material_resources = create_material_resources(root, backend);
    resources.mesh_cache = create_mesh_cache(root, backend);
    resources.shadow_map_resources = create_shadow_map_resources(root, backend, resources.shader_modules);
    resources.swapchain_pass_resources = create_swapchain_pass_resources(root, backend, resources.shader_modules);
}

void destroy_backend_resources(VulkanBackend& backend)
{
    BackendResources& resources = *backend.resources;

    destroy_shader_modules(backend, resources.shader_modules);
    destroy_sampler_resources(backend, resources.samplers_resources);
    destroy_framegraph_resources(backend, resources.framegraph_resources);
    destroy_audio_resources(backend, resources.audio_resources);
    destroy_culling_resources(backend, resources.cull_resources);
    destroy_frame_sync_resources(backend, resources.frame_sync_resources);
    destroy_gui_pass_resources(backend, resources.gui_pass_resources);
    destroy_histogram_pass_resources(backend, resources.histogram_pass_resources);
    destroy_lighting_pass_resources(backend, resources.lighting_resources);
    destroy_tiled_lighting_pass_resources(backend, resources.tiled_lighting_resources);
    destroy_forward_pass_resources(backend, resources.forward_pass_resources);
    destroy_material_resources(backend, resources.material_resources);
    destroy_mesh_cache(backend, resources.mesh_cache);
    destroy_shadow_map_resources(backend, resources.shadow_map_resources);
    destroy_swapchain_pass_resources(backend, resources.swapchain_pass_resources);

    vkDestroyEvent(backend.device, resources.event, nullptr);
    vkFreeCommandBuffers(backend.device, resources.gfxCommandPool, 1, &resources.gfxCmdBuffer.handle);
    vkDestroyCommandPool(backend.device, resources.gfxCommandPool, nullptr);

    delete backend.resources;
    backend.resources = nullptr;
}
} // namespace Reaper
