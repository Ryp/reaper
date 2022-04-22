////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TestGraphics.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/BackendResources.h"
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
    struct Access
    {
        VkPipelineStageFlags2 stage_mask;
        VkAccessFlags2        access_mask;
        VkImageLayout         layout;
        u32                   queueFamilyIndex;
    };

    VkDependencyInfo get_vk_image_barrier_depency_info(u32 barrier_count, const VkImageMemoryBarrier2* barriers)
    {
        return VkDependencyInfo{
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, VK_FLAGS_NONE, 0, nullptr, 0, nullptr, barrier_count, barriers,
        };
    }

    VkImageMemoryBarrier2 get_vk_image_barrier(VkImage handle, GPUTextureView view, Access src, Access dst)
    {
        const VkImageSubresourceRange view_range = GetVulkanImageSubresourceRange(view);

        return VkImageMemoryBarrier2{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                     nullptr,
                                     src.stage_mask,
                                     src.access_mask,
                                     dst.stage_mask,
                                     dst.access_mask,
                                     src.layout,
                                     dst.layout,
                                     src.queueFamilyIndex,
                                     dst.queueFamilyIndex,
                                     handle,
                                     view_range};
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
    const RenderPassHandle main_pass_handle = builder.create_render_pass("Main Pass");
    const RenderPassHandle gui_pass_handle = builder.create_render_pass("GUI Pass");

    GPUTextureProperties scene_hdr_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, MainHDRColorFormat);
    scene_hdr_properties.usageFlags = GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled; // FIXME deduced?

    GPUTextureProperties scene_depth_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, MainDepthFormat);
    scene_depth_properties.usageFlags = GPUTextureUsage::DepthStencilAttachment; // FIXME deduced?

    GPUTextureProperties gui_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, GUIFormat);
    gui_properties.usageFlags = GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled;

    const GPUTextureView scene_hdr_view = DefaultGPUTextureView(scene_hdr_properties);
    TGPUTextureUsage     scene_hdr_texture_usage = {};
    scene_hdr_texture_usage.LoadOp = ELoadOp::DontCare;
    scene_hdr_texture_usage.Layout = EImageLayout::ColorAttachmentOptimal;
    scene_hdr_texture_usage.view = scene_hdr_view;

    const GPUTextureView scene_depth_view = DefaultGPUTextureView(scene_depth_properties);
    TGPUTextureUsage     scene_depth_texture_usage = {};
    scene_depth_texture_usage.LoadOp = ELoadOp::DontCare;
    scene_depth_texture_usage.Layout = EImageLayout::ColorAttachmentOptimal;
    scene_depth_texture_usage.view = scene_depth_view;

    const GPUTextureView gui_view = DefaultGPUTextureView(gui_properties);
    TGPUTextureUsage     gui_texture_usage = {};
    gui_texture_usage.LoadOp = ELoadOp::Clear;
    gui_texture_usage.Layout = EImageLayout::ColorAttachmentOptimal;
    gui_texture_usage.view = gui_view;

    const ResourceUsageHandle main_hdr_usage_handle =
        builder.create_texture(main_pass_handle, "Scene HDR", scene_hdr_properties, scene_hdr_texture_usage);

    const ResourceUsageHandle main_depth_create_usage_handle =
        builder.create_texture(main_pass_handle, "Scene Depth", scene_depth_properties, scene_depth_texture_usage);

    const ResourceUsageHandle gui_create_usage_handle =
        builder.create_texture(gui_pass_handle, "GUI SDR", gui_properties, gui_texture_usage);

    // Swapchain
    const RenderPassHandle swapchain_pass_handle = builder.create_render_pass("Swapchain Pass", true);

    TGPUTextureUsage swapchain_hdr_usage = {};
    swapchain_hdr_usage.LoadOp = ELoadOp::Load;
    swapchain_hdr_usage.Layout = EImageLayout::ShaderReadOnlyOptimal;
    swapchain_hdr_usage.view = scene_hdr_view;

    TGPUTextureUsage swapchain_gui_usage = {};
    swapchain_gui_usage.LoadOp = ELoadOp::Load;
    swapchain_gui_usage.Layout = EImageLayout::ShaderReadOnlyOptimal;
    swapchain_gui_usage.view = gui_view;

    const ResourceUsageHandle swapchain_hdr_usage_handle =
        builder.read_texture(swapchain_pass_handle, main_hdr_usage_handle, swapchain_hdr_usage);
    const ResourceUsageHandle swapchain_gui_usage_handle =
        builder.read_texture(swapchain_pass_handle, gui_create_usage_handle, swapchain_gui_usage);
    builder.build();

    allocate_framegraph_resources(root, backend, resources.framegraph_resources, frame_graph);

    const ResourceUsage& main_hdr_usage = GetResourceUsage(frame_graph, main_hdr_usage_handle);
    const ResourceHandle main_hdr_resource_handle = main_hdr_usage.Resource;

    const ResourceUsage& main_depth_usage = GetResourceUsage(frame_graph, main_depth_create_usage_handle);
    const ResourceHandle main_depth_resource_handle = main_depth_usage.Resource;

    const ResourceUsage& gui_usage = GetResourceUsage(frame_graph, gui_create_usage_handle);
    const ResourceHandle gui_resource_handle = gui_usage.Resource;

    ImageInfo& hdrBuffer = get_frame_graph_texture(resources.framegraph_resources, main_hdr_resource_handle);
    ImageInfo& depthBuffer = get_frame_graph_texture(resources.framegraph_resources, main_depth_resource_handle);
    ImageInfo& guiBuffer = get_frame_graph_texture(resources.framegraph_resources, gui_resource_handle);

    const Access src_undefined = {VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_QUEUE_FAMILY_IGNORED};

    const Access scene_hdr_access_main_pass = {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                               VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                               VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_QUEUE_FAMILY_IGNORED};
    const Access scene_hdr_access_swapchain_pass = {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                    VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                                                    VK_QUEUE_FAMILY_IGNORED};

    const Access depth_access_main_pass = {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_QUEUE_FAMILY_IGNORED};

    const Access gui_access_gui_pass = {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                        VK_QUEUE_FAMILY_IGNORED};
    const Access gui_access_swapchain_pass = {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_QUEUE_FAMILY_IGNORED};

    const Access shadow_access_main_pass = {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                            VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_QUEUE_FAMILY_IGNORED};
    const Access shadow_access_shadow_pass = {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                              VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_QUEUE_FAMILY_IGNORED};

    update_culling_pass_descriptor_sets(backend, prepared, resources.cull_resources, resources.mesh_cache);
    update_shadow_map_pass_descriptor_sets(backend, prepared, resources.shadow_map_resources);
    update_main_pass_descriptor_sets(backend, resources.main_pass_resources, resources.material_resources,
                                     resources.shadow_map_resources.shadowMapView);
    // update_histogram_pass_descriptor_set(backend, resources.histogram_pass_resources, hdrBufferView);
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

        imageBarriers.emplace_back(
            get_vk_image_barrier(hdrBuffer.handle, scene_hdr_view, src_undefined, scene_hdr_access_swapchain_pass));
        imageBarriers.emplace_back(
            get_vk_image_barrier(depthBuffer.handle, scene_depth_view, src_undefined, depth_access_main_pass));
        imageBarriers.emplace_back(
            get_vk_image_barrier(guiBuffer.handle, gui_view, src_undefined, gui_access_swapchain_pass));

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
            const Access dst = {VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_MEMORY_READ_BIT,
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

        imageBarriers.emplace_back(get_vk_image_barrier(hdrBuffer.handle, scene_hdr_view,
                                                        scene_hdr_access_swapchain_pass, scene_hdr_access_main_pass));

        const VkDependencyInfo dependencies =
            get_vk_image_barrier_depency_info(static_cast<u32>(imageBarriers.size()), imageBarriers.data());

        vkCmdSetEvent2(cmdBuffer.handle, resources.event, &dependencies);
        vkCmdWaitEvents2(cmdBuffer.handle, 1, &resources.event, &dependencies);
    }

    record_main_pass_command_buffer(
        cmdBuffer, backend, prepared, resources.main_pass_resources, resources.cull_resources, resources.mesh_cache,
        backbufferExtent, get_frame_graph_texture_view(resources.framegraph_resources, main_hdr_usage_handle),
        get_frame_graph_texture_view(resources.framegraph_resources, main_depth_create_usage_handle));

    {
        std::vector<VkImageMemoryBarrier2> imageBarriers;
        imageBarriers.emplace_back(
            get_vk_image_barrier(guiBuffer.handle, gui_view, gui_access_swapchain_pass, gui_access_gui_pass));

        const VkDependencyInfo dependencies =
            get_vk_image_barrier_depency_info(static_cast<u32>(imageBarriers.size()), imageBarriers.data());

        vkCmdSetEvent2(cmdBuffer.handle, resources.event, &dependencies);
        vkCmdWaitEvents2(cmdBuffer.handle, 1, &resources.event, &dependencies);
    }

    record_gui_command_buffer(cmdBuffer, resources.gui_pass_resources, backbufferExtent,
                              get_frame_graph_texture_view(resources.framegraph_resources, gui_create_usage_handle));

    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        std::vector<VkImageMemoryBarrier2> imageBarriers;

        imageBarriers.emplace_back(get_vk_image_barrier(hdrBuffer.handle, scene_hdr_view, scene_hdr_access_main_pass,
                                                        scene_hdr_access_swapchain_pass));
        imageBarriers.emplace_back(
            get_vk_image_barrier(guiBuffer.handle, gui_view, gui_access_gui_pass, gui_access_swapchain_pass));

        const VkDependencyInfo dependencies =
            get_vk_image_barrier_depency_info(static_cast<u32>(imageBarriers.size()), imageBarriers.data());

        vkCmdSetEvent2(cmdBuffer.handle, resources.event, &dependencies);
        vkCmdWaitEvents2(cmdBuffer.handle, 1, &resources.event, &dependencies);
    }

    // record_histogram_command_buffer(cmdBuffer, frame_data, resources.histogram_pass_resources);

    record_swapchain_command_buffer(cmdBuffer, frame_data, resources.swapchain_pass_resources,
                                    backend.presentInfo.imageViews[current_swapchain_index]);

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
