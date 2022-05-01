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

#include "renderer/Mesh2.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/texture/GPUTextureView.h"
#include "renderer/window/Event.h"
#include "renderer/window/Window.h"

#include "renderer/graph/FrameGraphBuilder.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/BitTricks.h"
#include "core/Profile.h"
#include "core/memory/Allocator.h"

#include <array>

namespace Reaper
{
namespace
{
    void log_barriers(ReaperRoot& root, const FrameGraph::FrameGraph& frame_graph,
                      const FrameGraph::FrameGraphSchedule& schedule)
    {
        using namespace FrameGraph;

        log_debug(root, "framegraph: scheduling {} barrier events", schedule.barrier_events.size());

        log_debug(root, "framegraph: schedule has {} barriers", schedule.barriers.size());
        for (auto event : schedule.barrier_events)
        {
            const char* barrier_type_str =
                event.type == BarrierType::SingleBefore
                    ? "SingleBefore"
                    : (event.type == BarrierType::SingleAfter
                           ? "SingleAfter"
                           : (event.type == BarrierType::SplitBegin ? "SplitBegin" : "SplitEnd"));

            const char* render_pass_name = frame_graph.RenderPasses[event.render_pass_handle].Identifier;

            const Barrier&       barrier = schedule.barriers[event.barrier_handle];
            const ResourceUsage& src_usage = frame_graph.ResourceUsages[barrier.src.usage_handle];

            const char* resource_name = frame_graph.Resources[src_usage.Resource].Identifier;
            log_warning(root, "framegraph: pass '{}', resource '{}', barrier type = '{}'", render_pass_name,
                        resource_name, barrier_type_str);
            log_warning(root, "    - src layout = '{}', dst layout = '{}'",
                        GetImageLayoutToString(barrier.src.access.layout),
                        GetImageLayoutToString(barrier.dst.access.layout));
        }
    }

    // FIXME We have no way of merging events for now (dependencies struct)
    void record_framegraph_barriers(CommandBuffer& cmdBuffer, const FrameGraph::FrameGraphSchedule& schedule,
                                    const FrameGraph::FrameGraph& frame_graph, FrameGraphResources& resources,
                                    nonstd::span<const FrameGraph::BarrierEvent> barriers, bool before)
    {
        if (barriers.empty())
            return;

        using namespace FrameGraph;
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "FrameGraphBarrier", MP_RED);

        for (const auto& barrier_event : barriers)
        {
            const u32     barrier_handle = barrier_event.barrier_handle;
            const Barrier barrier = schedule.barriers[barrier_handle];

            std::vector<VkImageMemoryBarrier2> imageBarriers;

            const ResourceUsage& dst_usage = GetResourceUsage(frame_graph, barrier.dst.usage_handle);

            ImageInfo& texture = get_frame_graph_texture(resources, dst_usage.Resource);

            imageBarriers.emplace_back(
                get_vk_image_barrier(texture.handle, dst_usage.Usage.view, barrier.src.access, barrier.dst.access));

            const VkDependencyInfo dependencies =
                get_vk_image_barrier_depency_info(static_cast<u32>(imageBarriers.size()), imageBarriers.data());

            if (barrier_event.type == BarrierType::SingleBefore && before)
            {
                vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
            }
            else if (barrier_event.type == BarrierType::SingleAfter && !before)
            {
                vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
            }
            else if (barrier_event.type == BarrierType::SplitBegin && !before)
            {
                vkCmdSetEvent2(cmdBuffer.handle, resources.events[barrier_handle], &dependencies);
            }
            else if (barrier_event.type == BarrierType::SplitEnd && before)
            {
                vkCmdWaitEvents2(cmdBuffer.handle, 1, &resources.events[barrier_handle], &dependencies);
            }
        }
    }
} // namespace

bool vulkan_process_window_events(ReaperRoot& root, VulkanBackend& backend, IWindow* window)
{
    log_info(root, "window: pump events");

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

    prepare_shadow_map_objects(root, backend, prepared, resources.shadow_map_resources);

    upload_culling_resources(backend, prepared, resources.cull_resources);
    upload_shadow_map_resources(backend, prepared, resources.shadow_map_resources);
    upload_main_pass_frame_resources(backend, prepared, resources.main_pass_resources);
    upload_histogram_frame_resources(backend, resources.histogram_pass_resources, backbufferExtent);
    upload_swapchain_frame_resources(backend, prepared, resources.swapchain_pass_resources);
    upload_audio_frame_resources(backend, prepared, resources.audio_resources);

    FrameGraph::FrameGraph frame_graph;

    using namespace FrameGraph;

    FrameGraphBuilder builder(frame_graph);

    // Main
    const RenderPassHandle main_pass_handle = builder.create_render_pass("Forward");

    GPUTextureProperties scene_hdr_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, MainHDRColorFormat);
    scene_hdr_properties.usageFlags = GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled; // FIXME deduced?

    GPUTextureProperties scene_depth_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, MainDepthFormat);
    scene_depth_properties.usageFlags = GPUTextureUsage::DepthStencilAttachment; // FIXME deduced?

    const GPUTextureAccess scene_hdr_access_main_pass = {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                         VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_QUEUE_FAMILY_IGNORED};

    TGPUTextureUsage scene_hdr_texture_usage = {};
    scene_hdr_texture_usage.LoadOp = ELoadOp::DontCare;
    scene_hdr_texture_usage.access = scene_hdr_access_main_pass;
    scene_hdr_texture_usage.view = DefaultGPUTextureView(scene_hdr_properties);

    const ResourceUsageHandle main_hdr_usage_handle =
        builder.create_texture(main_pass_handle, "Scene HDR", scene_hdr_properties, scene_hdr_texture_usage);

    TGPUTextureUsage scene_depth_texture_usage = {};
    scene_depth_texture_usage.LoadOp = ELoadOp::DontCare;
    scene_depth_texture_usage.access =
        GPUTextureAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                         VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_QUEUE_FAMILY_IGNORED};
    scene_depth_texture_usage.view = DefaultGPUTextureView(scene_depth_properties);

    const ResourceUsageHandle main_depth_create_usage_handle =
        builder.create_texture(main_pass_handle, "Scene Depth", scene_depth_properties, scene_depth_texture_usage);

    // GUI
    const RenderPassHandle gui_pass_handle = builder.create_render_pass("GUI");

    GPUTextureProperties gui_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, GUIFormat);
    gui_properties.usageFlags = GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled;

    TGPUTextureUsage gui_texture_usage = {};
    gui_texture_usage.LoadOp = ELoadOp::Clear;
    gui_texture_usage.access =
        GPUTextureAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_QUEUE_FAMILY_IGNORED};
    gui_texture_usage.view = DefaultGPUTextureView(gui_properties);

    const ResourceUsageHandle gui_create_usage_handle =
        builder.create_texture(gui_pass_handle, "GUI SDR", gui_properties, gui_texture_usage);

    // Histogram
    const RenderPassHandle histogram_pass_handle = builder.create_render_pass(
        "Histogram", true); // FIXME Use outputs from this pass so we don't have to force is on

    TGPUTextureUsage histogram_hdr_usage = {};
    histogram_hdr_usage.LoadOp = ELoadOp::Load;
    histogram_hdr_usage.access = GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_QUEUE_FAMILY_IGNORED};
    histogram_hdr_usage.view = DefaultGPUTextureView(scene_hdr_properties);

    const ResourceUsageHandle histogram_hdr_usage_handle =
        builder.read_texture(histogram_pass_handle, main_hdr_usage_handle, histogram_hdr_usage);

    // Swapchain
    const RenderPassHandle swapchain_pass_handle = builder.create_render_pass("Swapchain", true);

    TGPUTextureUsage swapchain_hdr_usage = {};
    swapchain_hdr_usage.LoadOp = ELoadOp::Load;
    swapchain_hdr_usage.access = GPUTextureAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_QUEUE_FAMILY_IGNORED};
    swapchain_hdr_usage.view = DefaultGPUTextureView(scene_hdr_properties);

    const ResourceUsageHandle swapchain_hdr_usage_handle =
        builder.read_texture(swapchain_pass_handle, main_hdr_usage_handle, swapchain_hdr_usage);

    TGPUTextureUsage swapchain_gui_usage = {};
    swapchain_gui_usage.LoadOp = ELoadOp::Load;
    swapchain_gui_usage.access = GPUTextureAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_QUEUE_FAMILY_IGNORED};
    swapchain_gui_usage.view = DefaultGPUTextureView(gui_properties);

    const ResourceUsageHandle swapchain_gui_usage_handle =
        builder.read_texture(swapchain_pass_handle, gui_create_usage_handle, swapchain_gui_usage);

    builder.build();

    const FrameGraphSchedule schedule = compute_schedule(frame_graph);

    log_barriers(root, frame_graph, schedule);

    allocate_framegraph_volatile_resources(root, backend, resources.framegraph_resources, frame_graph);

    const GPUTextureAccess src_undefined = {VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
                                            VK_IMAGE_LAYOUT_UNDEFINED, VK_QUEUE_FAMILY_IGNORED};

    const GPUTextureAccess shadow_access_main_pass = {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                      VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                                                      VK_QUEUE_FAMILY_IGNORED};

    const GPUTextureAccess shadow_access_shadow_pass = {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_QUEUE_FAMILY_IGNORED};

    update_culling_pass_descriptor_sets(backend, prepared, resources.cull_resources, resources.mesh_cache);
    update_shadow_map_pass_descriptor_sets(backend, prepared, resources.shadow_map_resources);
    update_main_pass_descriptor_sets(backend, resources.main_pass_resources, resources.material_resources,
                                     resources.shadow_map_resources.shadowMapView);

    update_histogram_pass_descriptor_set(
        backend, resources.histogram_pass_resources,
        get_frame_graph_texture_view(resources.framegraph_resources, histogram_hdr_usage_handle));

    update_swapchain_pass_descriptor_set(
        backend, resources.swapchain_pass_resources,
        get_frame_graph_texture_view(resources.framegraph_resources, swapchain_hdr_usage_handle),
        get_frame_graph_texture_view(resources.framegraph_resources, swapchain_gui_usage_handle));
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

    record_material_upload_command_buffer(resources.material_resources.staging, cmdBuffer);

    {
        // FIXME Right now we recreate shadow maps every frame
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        std::vector<VkImageMemoryBarrier2> imageBarriers;

        for (ImageInfo& shadow_map : resources.shadow_map_resources.shadowMap)
        {
            imageBarriers.emplace_back(get_vk_image_barrier(shadow_map.handle,
                                                            DefaultGPUTextureView(shadow_map.properties), src_undefined,
                                                            shadow_access_main_pass));
        }

        const VkDependencyInfo dependencies =
            get_vk_image_barrier_depency_info(static_cast<u32>(imageBarriers.size()), imageBarriers.data());

        vkCmdSetEvent2(cmdBuffer.handle, resources.event, &dependencies);
        vkCmdWaitEvents2(cmdBuffer.handle, 1, &resources.event, &dependencies);
    }

    if (backend.mustTransitionSwapchain)
    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        std::vector<VkImageMemoryBarrier2> imageBarriers;

        for (u32 swapchainImageIndex = 0; swapchainImageIndex < static_cast<u32>(backend.presentInfo.images.size());
             swapchainImageIndex++)
        {
            const GPUTextureAccess dst = {VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_MEMORY_READ_BIT,
                                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_QUEUE_FAMILY_IGNORED};

            GPUTextureView view = {};
            // view.format;
            view.aspect = ViewAspect::Color;
            view.mipCount = 1;
            view.layerCount = 1;

            imageBarriers.emplace_back(
                get_vk_image_barrier(backend.presentInfo.images[swapchainImageIndex], view, src_undefined, dst));
        }

        const VkDependencyInfo dependencies =
            get_vk_image_barrier_depency_info(static_cast<u32>(imageBarriers.size()), imageBarriers.data());

        vkCmdSetEvent2(cmdBuffer.handle, resources.event, &dependencies);
        vkCmdWaitEvents2(cmdBuffer.handle, 1, &resources.event, &dependencies);

        backend.mustTransitionSwapchain = false;
    }

    record_culling_command_buffer(backend.options.freeze_culling, cmdBuffer, prepared, resources.cull_resources);

    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        std::vector<VkImageMemoryBarrier2> imageBarriers;

        for (ImageInfo& shadow_map : resources.shadow_map_resources.shadowMap)
        {
            imageBarriers.emplace_back(get_vk_image_barrier(shadow_map.handle,
                                                            DefaultGPUTextureView(shadow_map.properties),
                                                            shadow_access_main_pass, shadow_access_shadow_pass));
        }

        const VkDependencyInfo dependencies =
            get_vk_image_barrier_depency_info(static_cast<u32>(imageBarriers.size()), imageBarriers.data());

        vkCmdSetEvent2(cmdBuffer.handle, resources.event, &dependencies);
        vkCmdWaitEvents2(cmdBuffer.handle, 1, &resources.event, &dependencies);
    }

    record_shadow_map_command_buffer(cmdBuffer, backend, prepared, resources.shadow_map_resources,
                                     resources.cull_resources, resources.mesh_cache.vertexBufferPosition.buffer);

    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        std::vector<VkImageMemoryBarrier2> imageBarriers;

        for (ImageInfo& shadow_map : resources.shadow_map_resources.shadowMap)
        {
            imageBarriers.emplace_back(get_vk_image_barrier(shadow_map.handle,
                                                            DefaultGPUTextureView(shadow_map.properties),
                                                            shadow_access_shadow_pass, shadow_access_main_pass));
        }

        const VkDependencyInfo dependencies =
            get_vk_image_barrier_depency_info(static_cast<u32>(imageBarriers.size()), imageBarriers.data());

        vkCmdSetEvent2(cmdBuffer.handle, resources.event, &dependencies);
        vkCmdWaitEvents2(cmdBuffer.handle, 1, &resources.event, &dependencies);
    }

    {
        auto barriers = get_barriers_to_execute(schedule, main_pass_handle);
        log_debug(root, "framegraph: pass '{}', scheduling {} barrier events", main_pass_handle, barriers.size());

        record_framegraph_barriers(cmdBuffer, schedule, frame_graph, resources.framegraph_resources, barriers, true);

        record_main_pass_command_buffer(
            cmdBuffer, backend, prepared, resources.main_pass_resources, resources.cull_resources, resources.mesh_cache,
            backbufferExtent, get_frame_graph_texture_view(resources.framegraph_resources, main_hdr_usage_handle),
            get_frame_graph_texture_view(resources.framegraph_resources, main_depth_create_usage_handle));

        record_framegraph_barriers(cmdBuffer, schedule, frame_graph, resources.framegraph_resources, barriers, false);
    }

    {
        auto barriers = get_barriers_to_execute(schedule, gui_pass_handle);
        log_debug(root, "framegraph: pass '{}', scheduling {} barrier events", gui_pass_handle, barriers.size());

        record_framegraph_barriers(cmdBuffer, schedule, frame_graph, resources.framegraph_resources, barriers, true);

        record_gui_command_buffer(
            cmdBuffer, resources.gui_pass_resources, backbufferExtent,
            get_frame_graph_texture_view(resources.framegraph_resources, gui_create_usage_handle));

        record_framegraph_barriers(cmdBuffer, schedule, frame_graph, resources.framegraph_resources, barriers, false);
    }

    {
        auto barriers = get_barriers_to_execute(schedule, histogram_pass_handle);
        log_debug(root, "framegraph: pass '{}', scheduling {} barrier events", histogram_pass_handle, barriers.size());

        record_framegraph_barriers(cmdBuffer, schedule, frame_graph, resources.framegraph_resources, barriers, true);
        record_histogram_command_buffer(cmdBuffer, frame_data, resources.histogram_pass_resources);
        record_framegraph_barriers(cmdBuffer, schedule, frame_graph, resources.framegraph_resources, barriers, false);
    }

    {
        auto barriers = get_barriers_to_execute(schedule, swapchain_pass_handle);
        log_debug(root, "framegraph: pass '{}', scheduling {} barrier events", swapchain_pass_handle, barriers.size());

        record_framegraph_barriers(cmdBuffer, schedule, frame_graph, resources.framegraph_resources, barriers, true);

        record_swapchain_command_buffer(cmdBuffer, frame_data, resources.swapchain_pass_resources,
                                        backend.presentInfo.imageViews[current_swapchain_index]);

        record_framegraph_barriers(cmdBuffer, schedule, frame_graph, resources.framegraph_resources, barriers, false);
    }

    record_audio_command_buffer(cmdBuffer, prepared, resources.audio_resources);

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
