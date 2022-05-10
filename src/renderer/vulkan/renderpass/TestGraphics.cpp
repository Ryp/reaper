////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TestGraphics.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/BackendResources.h"
#include "renderer/vulkan/Barrier.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/Debug.h"
#include "renderer/vulkan/FrameSync.h"
#include "renderer/vulkan/MaterialResources.h"
#include "renderer/vulkan/Memory.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/Swapchain.h"
#include "renderer/vulkan/api/Vulkan.h"
#include "renderer/vulkan/api/VulkanStringConversion.h"
#include "renderer/vulkan/renderpass/FrameGraphPass.h"

#include "renderer/Mesh2.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/texture/GPUTextureView.h"
#include "renderer/window/Event.h"
#include "renderer/window/Window.h"

#include "renderer/graph/FrameGraphBuilder.h"
#include "renderer/graph/GraphDebug.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/BitTricks.h"
#include "core/Profile.h"
#include "core/memory/Allocator.h"

#include "renderer/shader/share/hdr.hlsl"

#include <array>

namespace Reaper
{
namespace
{
    void log_barriers(ReaperRoot& root, const FrameGraph::FrameGraph& framegraph,
                      const FrameGraph::FrameGraphSchedule& schedule)
    {
        using namespace FrameGraph;

        log_debug(root, "framegraph: scheduling {} barrier events", schedule.barrier_events.size());

        log_debug(root, "framegraph: schedule has {} barriers", schedule.barriers.size());
        for (auto event : schedule.barrier_events)
        {
            const char* barrier_type_str =
                event.type == BarrierType::ImmediateBefore
                    ? "ImmediateBefore"
                    : (event.type == BarrierType::ImmediateAfter
                           ? "ImmediateAfter"
                           : (event.type == BarrierType::SplitBegin ? "SplitBegin" : "SplitEnd"));

            const char* render_pass_name = framegraph.RenderPasses[event.render_pass_handle].debug_name;

            const Barrier&       barrier = schedule.barriers[event.barrier_handle];
            const ResourceUsage& src_usage = framegraph.ResourceUsages[barrier.src.usage_handle];
            const ResourceHandle resource_handle = src_usage.resource_handle;
            const Resource&      resource = GetResource(framegraph, src_usage);

            const char* resource_name = resource.debug_name;
            log_debug(root, "framegraph: pass '{}', resource '{}', barrier type = '{}'", render_pass_name,
                      resource_name, barrier_type_str);
            if (resource_handle.is_texture)
            {
                log_debug(root, "    - src layout = '{}', dst layout = '{}'",
                          GetImageLayoutToString(barrier.src.access.image_layout),
                          GetImageLayoutToString(barrier.dst.access.image_layout));
            }
        }
    }
} // namespace

bool vulkan_process_window_events(ReaperRoot& root, VulkanBackend& backend, IWindow* window)
{
    log_debug(root, "window: pump events");

    std::vector<Window::Event> events;
    window->pumpEvents(events);

    for (const auto& event : events)
    {
        if (event.type == Window::EventType::Resize)
        {
            const u32 width = event.message.resize.width;
            const u32 height = event.message.resize.height;
            log_debug(root, "window: resize event, width = {}, height = {}", width, height);

            Assert(width > 0);
            Assert(height > 0);

            // FIXME Do not set for duplicate events
            backend.new_swapchain_extent = {width, height};
            backend.mustTransitionSwapchain = true;
        }
        else if (event.type == Window::EventType::KeyPress)
        {
            log_warning(root, "window: key press detected: now exiting...");
            return true;
        }
        else
        {
            log_warning(root, "window: an unknown event has been caught and will not be handled");
        }
    }

    return false;
}

void resize_swapchain(ReaperRoot& root, VulkanBackend& backend)
{
    // Resize swapchain if necessary
    if (backend.new_swapchain_extent.width != 0)
    {
        vkQueueWaitIdle(backend.deviceInfo.presentQueue); // FIXME

        Assert(backend.new_swapchain_extent.height > 0);
        resize_vulkan_wm_swapchain(root, backend, backend.presentInfo, backend.new_swapchain_extent);

        const glm::uvec2 new_swapchain_extent(backend.presentInfo.surfaceExtent.width,
                                              backend.presentInfo.surfaceExtent.height);

        backend.new_swapchain_extent.width = 0;
        backend.new_swapchain_extent.height = 0;
    }
}

void backend_execute_frame(ReaperRoot& root, VulkanBackend& backend, CommandBuffer& cmdBuffer,
                           const PreparedData& prepared, BackendResources& resources)
{
    VkResult acquireResult;
    u64      acquireTimeoutUs = 1000000000;
    uint32_t current_swapchain_index = 0;

    constexpr u32 MaxAcquireTryCount = 10;

    for (u32 acquireTryCount = 0; acquireTryCount < MaxAcquireTryCount; acquireTryCount++)
    {
        log_debug(root, "vulkan: acquiring frame try #{}", acquireTryCount);
        acquireResult = vkAcquireNextImageKHR(backend.device, backend.presentInfo.swapchain, acquireTimeoutUs,
                                              backend.presentInfo.imageAvailableSemaphore, VK_NULL_HANDLE,
                                              &current_swapchain_index);

        if (acquireResult != VK_NOT_READY)
            break;
    }

    switch (acquireResult)
    {
    case VK_NOT_READY: // Should be good after X tries
        log_debug(root, "vulkan: vkAcquireNextImageKHR not ready");
        break;
    case VK_SUBOPTIMAL_KHR:
        log_debug(root, "vulkan: vkAcquireNextImageKHR suboptimal");
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        log_warning(root, "vulkan: vkAcquireNextImageKHR out of date");
        return;
    default:
        Assert(acquireResult == VK_SUCCESS);
        break;
    }

    log_debug(root, "vulkan: swapchain image index = {}", current_swapchain_index);

    {
        VkFence drawFence = resources.frame_sync_resources.drawFence;

        log_debug(root, "vulkan: wait for fence");
        VkResult waitResult;
        {
            REAPER_PROFILE_SCOPE("Wait", MP_BLUE);
            const u64 waitTimeoutUs = 300000000;
            waitResult = vkWaitForFences(backend.device, 1, &drawFence, VK_TRUE, waitTimeoutUs);
        }

        if (waitResult != VK_SUCCESS)
            log_debug(root, "- return result {}", GetResultToString(waitResult));

        // TODO Handle timeout correctly
        Assert(waitResult == VK_SUCCESS);

        Assert(vkGetFenceStatus(backend.device, drawFence) == VK_SUCCESS);

        log_debug(root, "vulkan: reset fence");
        Assert(vkResetFences(backend.device, 1, &drawFence) == VK_SUCCESS);
    }

    const VkExtent2D backbufferExtent = backend.presentInfo.surfaceExtent;

    FrameData frame_data = {};
    frame_data.backbufferExtent = backbufferExtent;

    // FIXME Recreate the swapchain pipeline every frame
    reload_swapchain_pipeline(root, backend, resources.swapchain_pass_resources);

    upload_culling_resources(backend, prepared, resources.cull_resources);
    upload_shadow_map_resources(backend, prepared, resources.shadow_map_resources);
    upload_forward_pass_frame_resources(backend, prepared, resources.forward_pass_resources);
    upload_histogram_frame_resources(backend, resources.histogram_pass_resources, backbufferExtent);
    upload_swapchain_frame_resources(backend, prepared, resources.swapchain_pass_resources);
    upload_audio_frame_resources(backend, prepared, resources.audio_resources);

    FrameGraph::FrameGraph framegraph;

    using namespace FrameGraph;

    Builder builder(framegraph);

    // Shadow
    const RenderPassHandle shadow_pass_handle = builder.create_render_pass("Shadow");

    std::vector<GPUTextureProperties> shadow_map_properties = fill_shadow_map_properties(prepared);
    std::vector<ResourceUsageHandle>  shadow_map_creation_usage_handles;
    for (const GPUTextureProperties& properties : shadow_map_properties)
    {
        GPUResourceUsage shadow_map_usage = {};
        shadow_map_usage.access = {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                   VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL};
        shadow_map_usage.texture_view = DefaultGPUTextureView(properties);

        const ResourceUsageHandle usage_handle =
            builder.create_texture(shadow_pass_handle, "Shadow map", properties, shadow_map_usage);

        shadow_map_creation_usage_handles.push_back(usage_handle);
    }

    // Forward
    const RenderPassHandle forward_pass_handle = builder.create_render_pass("Forward");

    GPUTextureProperties scene_hdr_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, ForwardHDRColorFormat);
    scene_hdr_properties.usage_flags = GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled;

    GPUTextureProperties scene_depth_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, ForwardDepthFormat);
    scene_depth_properties.usage_flags = GPUTextureUsage::DepthStencilAttachment;

    const GPUResourceAccess scene_hdr_access_forward_pass = {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                             VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                             VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL};

    GPUResourceUsage scene_hdr_texture_usage = {};
    scene_hdr_texture_usage.access = scene_hdr_access_forward_pass;
    scene_hdr_texture_usage.texture_view = DefaultGPUTextureView(scene_hdr_properties);

    const ResourceUsageHandle forward_hdr_usage_handle =
        builder.create_texture(forward_pass_handle, "Scene HDR", scene_hdr_properties, scene_hdr_texture_usage);

    GPUResourceUsage scene_depth_texture_usage = {};
    scene_depth_texture_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                          VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL};
    scene_depth_texture_usage.texture_view = DefaultGPUTextureView(scene_depth_properties);

    const ResourceUsageHandle forward_depth_create_usage_handle =
        builder.create_texture(forward_pass_handle, "Scene Depth", scene_depth_properties, scene_depth_texture_usage);

    std::vector<ResourceUsageHandle> shadow_map_forward_usage_handles;
    for (u32 shadow_map_index = 0; shadow_map_index < shadow_map_properties.size(); shadow_map_index++)
    {
        GPUResourceUsage shadow_map_usage = {};
        shadow_map_usage.access = {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                   VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
        shadow_map_usage.texture_view = DefaultGPUTextureView(shadow_map_properties[shadow_map_index]);

        const ResourceUsageHandle usage_handle = builder.read_texture(
            forward_pass_handle, shadow_map_creation_usage_handles[shadow_map_index], shadow_map_usage);
        shadow_map_forward_usage_handles.push_back(usage_handle);
    }

    // GUI
    const RenderPassHandle gui_pass_handle = builder.create_render_pass("GUI");

    GPUTextureProperties gui_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, GUIFormat);
    gui_properties.usage_flags = GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled;

    GPUResourceUsage gui_texture_usage = {};
    gui_texture_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL};
    gui_texture_usage.texture_view = DefaultGPUTextureView(gui_properties);

    const ResourceUsageHandle gui_create_usage_handle =
        builder.create_texture(gui_pass_handle, "GUI SDR", gui_properties, gui_texture_usage);

    // Histogram Clear
    const RenderPassHandle histogram_clear_pass_handle = builder.create_render_pass("Histogram Clear");

    const GPUBufferProperties histogram_buffer_properties = DefaultGPUBufferProperties(
        HISTOGRAM_RES, sizeof(u32), GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst);

    GPUResourceUsage histogram_clear_usage = {};
    histogram_clear_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
    histogram_clear_usage.buffer_view = default_buffer_view(histogram_buffer_properties);

    const ResourceUsageHandle histogram_clear_usage_handle = builder.create_buffer(
        histogram_clear_pass_handle, "Histogram Buffer", histogram_buffer_properties, histogram_clear_usage);

    // Histogram
    const RenderPassHandle histogram_pass_handle = builder.create_render_pass("Histogram");

    GPUResourceUsage histogram_hdr_usage = {};
    histogram_hdr_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                   VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    histogram_hdr_usage.texture_view = DefaultGPUTextureView(scene_hdr_properties);

    const ResourceUsageHandle histogram_hdr_usage_handle =
        builder.read_texture(histogram_pass_handle, forward_hdr_usage_handle, histogram_hdr_usage);

    GPUResourceUsage histogram_write_usage = {};
    histogram_write_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
    histogram_write_usage.buffer_view = default_buffer_view(histogram_buffer_properties);

    const ResourceUsageHandle histogram_write_usage_handle =
        builder.write_buffer(histogram_pass_handle, histogram_clear_usage_handle, histogram_write_usage);

    // Swapchain
    const RenderPassHandle swapchain_pass_handle = builder.create_render_pass("Swapchain", true);

    GPUResourceUsage swapchain_hdr_usage = {};
    swapchain_hdr_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                   VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    swapchain_hdr_usage.texture_view = DefaultGPUTextureView(scene_hdr_properties);

    const ResourceUsageHandle swapchain_hdr_usage_handle =
        builder.read_texture(swapchain_pass_handle, forward_hdr_usage_handle, swapchain_hdr_usage);

    GPUResourceUsage swapchain_gui_usage = {};
    swapchain_gui_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                   VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    swapchain_gui_usage.texture_view = DefaultGPUTextureView(gui_properties);

    const ResourceUsageHandle swapchain_gui_usage_handle =
        builder.read_texture(swapchain_pass_handle, gui_create_usage_handle, swapchain_gui_usage);

    GPUResourceUsage swapchain_histogram_buffer_usage = {};
    swapchain_histogram_buffer_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
    swapchain_histogram_buffer_usage.buffer_view = default_buffer_view(histogram_buffer_properties);

    const ResourceUsageHandle swapchain_histogram_buffer_usage_handle =
        builder.read_buffer(swapchain_pass_handle, histogram_write_usage_handle, swapchain_histogram_buffer_usage);

    static_cast<void>(swapchain_histogram_buffer_usage_handle); // FIXME unused for now

    builder.build();

    DumpFrameGraph(framegraph);

    const FrameGraphSchedule schedule = compute_schedule(framegraph);

    log_barriers(root, framegraph, schedule);

    allocate_framegraph_volatile_resources(root, backend, resources.framegraph_resources, framegraph);

    update_culling_pass_descriptor_sets(backend, prepared, resources.cull_resources, resources.mesh_cache);

    update_shadow_map_pass_descriptor_sets(backend, prepared, resources.shadow_map_resources,
                                           resources.mesh_cache.vertexBufferPosition);

    std::vector<VkImageView> shadow_map_views;
    for (auto handle : shadow_map_creation_usage_handles)
    {
        shadow_map_views.emplace_back(
            get_frame_graph_texture(resources.framegraph_resources, framegraph, handle).view_handle);
    }

    update_forward_pass_descriptor_sets(backend, resources.forward_pass_resources, resources.material_resources,
                                        resources.mesh_cache, shadow_map_views);

    update_histogram_pass_descriptor_set(
        backend, resources.histogram_pass_resources,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, histogram_hdr_usage_handle).view_handle,
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, histogram_write_usage_handle));

    update_swapchain_pass_descriptor_set(
        backend, resources.swapchain_pass_resources,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, swapchain_hdr_usage_handle).view_handle,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, swapchain_gui_usage_handle).view_handle);
    update_audio_pass_descriptor_set(backend, resources.audio_resources);

    log_debug(root, "vulkan: record command buffer");
    Assert(vkResetCommandBuffer(cmdBuffer.handle, 0) == VK_SUCCESS);

    VkCommandBufferBeginInfo cmdBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
        0,      // Not caring yet
        nullptr // No inheritance yet
    };

    Assert(vkBeginCommandBuffer(cmdBuffer.handle, &cmdBufferBeginInfo) == VK_SUCCESS);

#if defined(REAPER_USE_MICROPROFILE)
    cmdBuffer.mlog = MicroProfileThreadLogGpuAlloc();
    MICROPROFILE_GPU_BEGIN(cmdBuffer.handle, cmdBuffer.mlog);
#endif

    if (backend.mustTransitionSwapchain)
    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        std::vector<VkImageMemoryBarrier2> imageBarriers;

        for (u32 swapchainImageIndex = 0; swapchainImageIndex < static_cast<u32>(backend.presentInfo.images.size());
             swapchainImageIndex++)
        {
            const GPUResourceAccess src_undefined = {VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
                                                     VK_IMAGE_LAYOUT_UNDEFINED};
            const GPUResourceAccess dst_access = {VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_MEMORY_READ_BIT,
                                                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

            GPUTextureView view = {};
            // view.format;
            view.aspect = ViewAspect::Color;
            view.mipCount = 1;
            view.layerCount = 1;

            imageBarriers.emplace_back(
                get_vk_image_barrier(backend.presentInfo.images[swapchainImageIndex], view, src_undefined, dst_access));
        }

        const VkDependencyInfo dependencies =
            get_vk_image_barrier_depency_info(static_cast<u32>(imageBarriers.size()), imageBarriers.data());

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);

        backend.mustTransitionSwapchain = false;
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Material upload");
        record_material_upload_command_buffer(resources.material_resources.staging, cmdBuffer);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Culling");
        record_culling_command_buffer(backend.options.freeze_culling, cmdBuffer, prepared, resources.cull_resources);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Shadow");
        record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources, shadow_pass_handle,
                                   true);

        record_shadow_map_command_buffer(cmdBuffer, backend, prepared, resources.shadow_map_resources, shadow_map_views,
                                         resources.cull_resources);

        record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources, shadow_pass_handle,
                                   false);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Forward");
        record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources, forward_pass_handle,
                                   true);

        record_forward_pass_command_buffer(
            cmdBuffer, backend, prepared, resources.forward_pass_resources, resources.cull_resources, backbufferExtent,
            get_frame_graph_texture(resources.framegraph_resources, framegraph, forward_hdr_usage_handle).view_handle,
            get_frame_graph_texture(resources.framegraph_resources, framegraph, forward_depth_create_usage_handle)
                .view_handle);

        record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources, forward_pass_handle,
                                   false);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "GUI");
        record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources, gui_pass_handle,
                                   true);

        record_gui_command_buffer(
            cmdBuffer, resources.gui_pass_resources, backbufferExtent,
            get_frame_graph_texture(resources.framegraph_resources, framegraph, gui_create_usage_handle).view_handle);

        record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources, gui_pass_handle,
                                   false);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Histogram Clear");
        record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                   histogram_clear_pass_handle, true);

        const u32        clear_value = 0;
        FrameGraphBuffer histogram_buffer =
            get_frame_graph_buffer(resources.framegraph_resources, framegraph, histogram_clear_usage_handle);

        vkCmdFillBuffer(cmdBuffer.handle, histogram_buffer.handle, histogram_buffer.view.offset_bytes,
                        histogram_buffer.view.size_bytes, clear_value);

        record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                   histogram_clear_pass_handle, false);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Histogram");
        record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                   histogram_pass_handle, true);

        record_histogram_command_buffer(cmdBuffer, frame_data, resources.histogram_pass_resources);

        record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                   histogram_pass_handle, false);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Swapchain");
        record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                   swapchain_pass_handle, true);

        record_swapchain_command_buffer(cmdBuffer, frame_data, resources.swapchain_pass_resources,
                                        backend.presentInfo.imageViews[current_swapchain_index]);

        record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                   swapchain_pass_handle, false);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Audio");
        record_audio_command_buffer(cmdBuffer, prepared, resources.audio_resources);
    }

#if defined(REAPER_USE_MICROPROFILE)
    const u64 microprofile_data = MicroProfileGpuEnd(cmdBuffer.mlog);
    MicroProfileThreadLogGpuFree(cmdBuffer.mlog);

    MICROPROFILE_GPU_SUBMIT(MicroProfileGetGlobalGpuQueue(), microprofile_data);
    MicroProfileFlip(cmdBuffer.handle);
#endif

    // Stop recording
    Assert(vkEndCommandBuffer(cmdBuffer.handle) == VK_SUCCESS);

    VkPipelineStageFlags blitWaitDstMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    std::array<VkSemaphore, 1> semaphores_to_signal = {backend.presentInfo.renderingFinishedSemaphore};

    VkSubmitInfo blitSubmitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                   nullptr,
                                   1,
                                   &backend.presentInfo.imageAvailableSemaphore,
                                   &blitWaitDstMask,
                                   1,
                                   &cmdBuffer.handle,
                                   semaphores_to_signal.size(),
                                   semaphores_to_signal.data()};

    log_debug(root, "vulkan: submit drawing commands");
    Assert(vkQueueSubmit(backend.deviceInfo.graphicsQueue, 1, &blitSubmitInfo, resources.frame_sync_resources.drawFence)
           == VK_SUCCESS);

    log_debug(root, "vulkan: present");

    VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                    nullptr,
                                    1,
                                    &backend.presentInfo.renderingFinishedSemaphore,
                                    1,
                                    &backend.presentInfo.swapchain,
                                    &current_swapchain_index,
                                    nullptr};

    VkResult presentResult = vkQueuePresentKHR(backend.deviceInfo.presentQueue, &presentInfo);
    // NOTE: window can change state between event handling and presenting, so it's normal to get OOD events.
    Assert(presentResult == VK_SUCCESS || presentResult == VK_ERROR_OUT_OF_DATE_KHR);

    read_gpu_audio_data(backend, resources.audio_resources);
}
} // namespace Reaper
