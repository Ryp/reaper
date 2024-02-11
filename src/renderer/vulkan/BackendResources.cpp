////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "BackendResources.h"

#include "Backend.h"
#include "api/AssertHelper.h"
#include "profiling/Scope.h"

#include "common/Log.h"

#include <core/Literals.h>

namespace Reaper
{
void create_backend_resources(ReaperRoot& root, VulkanBackend& backend)
{
    backend.resources = new BackendResources;

    BackendResources& resources = *backend.resources;

    // Create command buffer
    const VkCommandPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .queueFamilyIndex = backend.physical_device.graphics_queue_family_index,
    };

    AssertVk(vkCreateCommandPool(backend.device, &poolCreateInfo, nullptr, &resources.gfxCommandPool));
    log_debug(root, "vulkan: created command pool with handle: {}", static_cast<void*>(resources.gfxCommandPool));

    const VkCommandBufferAllocateInfo cmdBufferAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = resources.gfxCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    AssertVk(vkAllocateCommandBuffers(backend.device, &cmdBufferAllocInfo, &resources.gfxCmdBuffer.handle));
    log_debug(root, "vulkan: created command buffer with handle: {}",
              static_cast<void*>(resources.gfxCmdBuffer.handle));

#if defined(REAPER_USE_TRACY)
    resources.gfxCmdBuffer.tracy_ctx = TracyVkContextCalibrated(
        backend.physical_device.handle, backend.device, backend.graphics_queue, resources.gfxCmdBuffer.handle,
        vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, vkGetCalibratedTimestampsEXT);
    // TracyVkContextName(resources.gfxCmdBuffer.tracy_ctx, name, size);
#endif

    resources.pipeline_factory = create_pipeline_factory(backend);
    resources.shader_modules = create_shader_modules(backend);
    resources.samplers_resources = create_sampler_resources(backend);
    resources.frame_storage_allocator =
        create_storage_buffer_allocator(backend, "Frame Storage Buffer Allocator", 1_MiB);
    resources.debug_geometry_resources = create_debug_geometry_pass_resources(backend, resources.pipeline_factory);
    resources.framegraph_resources = create_framegraph_resources(backend);
    resources.audio_resources = create_audio_resources(backend, resources.pipeline_factory);
    resources.meshlet_culling_resources = create_meshlet_culling_resources(backend, resources.pipeline_factory);
    resources.vis_buffer_pass_resources = create_vis_buffer_pass_resources(backend, resources.pipeline_factory);
    resources.frame_sync_resources = create_frame_sync_resources(backend);
    resources.gui_pass_resources = create_gui_pass_resources(backend, resources.pipeline_factory);
    resources.histogram_pass_resources = create_histogram_pass_resources(backend, resources.pipeline_factory);
    resources.exposure_pass_resources = create_exposure_pass_resources(backend, resources.pipeline_factory);
    resources.hzb_pass_resources = create_hzb_pass_resources(backend, resources.pipeline_factory);
    resources.lighting_resources = create_lighting_pass_resources(backend);
    resources.tiled_raster_resources = create_tiled_raster_pass_resources(backend, resources.pipeline_factory);
    resources.tiled_lighting_resources = create_tiled_lighting_pass_resources(backend, resources.pipeline_factory);
    resources.forward_pass_resources = create_forward_pass_resources(backend, resources.pipeline_factory);
    resources.material_resources = create_material_resources(backend);
    resources.mesh_cache = create_mesh_cache(backend);
    resources.shadow_map_resources = create_shadow_map_resources(backend, resources.pipeline_factory);
    resources.swapchain_pass_resources = create_swapchain_pass_resources(backend, resources.shader_modules);
}

void destroy_backend_resources(VulkanBackend& backend)
{
    BackendResources& resources = *backend.resources;

    destroy_pipeline_factory(backend, resources.pipeline_factory);
    destroy_shader_modules(backend, resources.shader_modules);
    destroy_sampler_resources(backend, resources.samplers_resources);
    destroy_storage_buffer_allocator(backend, resources.frame_storage_allocator);
    destroy_debug_geometry_pass_resources(backend, resources.debug_geometry_resources);
    destroy_framegraph_resources(backend, resources.framegraph_resources);
    destroy_audio_resources(backend, resources.audio_resources);
    destroy_meshlet_culling_resources(backend, resources.meshlet_culling_resources);
    destroy_vis_buffer_pass_resources(backend, resources.vis_buffer_pass_resources);
    destroy_frame_sync_resources(backend, resources.frame_sync_resources);
    destroy_gui_pass_resources(backend, resources.gui_pass_resources);
    destroy_histogram_pass_resources(backend, resources.histogram_pass_resources);
    destroy_exposure_pass_resources(backend, resources.exposure_pass_resources);
    destroy_hzb_pass_resources(backend, resources.hzb_pass_resources);
    destroy_lighting_pass_resources(backend, resources.lighting_resources);
    destroy_tiled_raster_pass_resources(backend, resources.tiled_raster_resources);
    destroy_tiled_lighting_pass_resources(backend, resources.tiled_lighting_resources);
    destroy_forward_pass_resources(backend, resources.forward_pass_resources);
    destroy_material_resources(backend, resources.material_resources);
    destroy_mesh_cache(backend, resources.mesh_cache);
    destroy_shadow_map_resources(backend, resources.shadow_map_resources);
    destroy_swapchain_pass_resources(backend, resources.swapchain_pass_resources);

#if defined(REAPER_USE_TRACY)
    TracyVkDestroy(resources.gfxCmdBuffer.tracy_ctx);
#endif

    vkFreeCommandBuffers(backend.device, resources.gfxCommandPool, 1, &resources.gfxCmdBuffer.handle);
    vkDestroyCommandPool(backend.device, resources.gfxCommandPool, nullptr);

    delete backend.resources;
    backend.resources = nullptr;
}
} // namespace Reaper
