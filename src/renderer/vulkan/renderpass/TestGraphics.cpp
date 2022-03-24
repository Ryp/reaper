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

void resize_swapchain(ReaperRoot& root, VulkanBackend& backend, BackendResources& resources)
{
    // Resize swapchain if necessary
    if (backend.new_swapchain_extent.width != 0)
    {
        vkQueueWaitIdle(backend.deviceInfo.presentQueue); // FIXME

        Assert(backend.new_swapchain_extent.height > 0);
        resize_vulkan_wm_swapchain(root, backend, backend.presentInfo, backend.new_swapchain_extent);

        const glm::uvec2 new_swapchain_extent(backend.presentInfo.surfaceExtent.width,
                                              backend.presentInfo.surfaceExtent.height);

        resize_main_pass_resources(root, backend, resources.main_pass_resources, new_swapchain_extent);
        resize_gui_pass_resources(root, backend, resources.gui_pass_resources, new_swapchain_extent);

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

    constexpr PixelFormat HDRColorFormat = PixelFormat::B10G11R11_UFLOAT_PACK32;
    GPUTextureProperties  scene_hdr_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, HDRColorFormat);
    scene_hdr_properties.usageFlags = GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled; // FIXME deduced?

    const GPUTextureView scene_hdr_view = DefaultGPUTextureView(scene_hdr_properties);
    TGPUTextureUsage     scene_hdr_texture_usage = {};
    scene_hdr_texture_usage.LoadOp = ELoadOp::DontCare;
    scene_hdr_texture_usage.Layout = EImageLayout::ColorAttachmentOptimal;
    scene_hdr_texture_usage.view = scene_hdr_view;

    const ResourceUsageHandle main_hdr_usage_handle =
        builder.create_texture(main_pass_handle, "Scene HDR", scene_hdr_properties, scene_hdr_texture_usage);

    // Swapchain
    const RenderPassHandle swapchain_pass_handle = builder.create_render_pass("Swapchain Pass", true);

    TGPUTextureUsage swapchain_hdr_usage = {};
    swapchain_hdr_usage.LoadOp = ELoadOp::DontCare;
    swapchain_hdr_usage.Layout = EImageLayout::ColorAttachmentOptimal;
    swapchain_hdr_usage.view = scene_hdr_view;

    const ResourceUsageHandle swapchain_hdr_usage_handle =
        builder.read_texture(swapchain_pass_handle, main_hdr_usage_handle, swapchain_hdr_usage);
    builder.build();

    allocate_framegraph_resources(root, backend, resources.framegraph_resources, frame_graph);

    const ResourceUsage& main_hdr_usage = GetResourceUsage(frame_graph, main_hdr_usage_handle);
    const ResourceHandle main_hdr_resource_handle = main_hdr_usage.Resource;

    ImageInfo& hdrBuffer = get_frame_graph_texture(resources.framegraph_resources, main_hdr_resource_handle);

    update_culling_pass_descriptor_sets(backend, prepared, resources.cull_resources, resources.mesh_cache);
    update_shadow_map_pass_descriptor_sets(backend, prepared, resources.shadow_map_resources);
    update_main_pass_descriptor_sets(backend, resources.main_pass_resources, resources.material_resources,
                                     resources.shadow_map_resources.shadowMapView);
    // update_histogram_pass_descriptor_set(backend, resources.histogram_pass_resources, hdrBufferView);
    update_swapchain_pass_descriptor_set(
        backend, resources.swapchain_pass_resources,
        get_frame_graph_texture_view(resources.framegraph_resources, swapchain_hdr_usage_handle),
        resources.gui_pass_resources.guiTextureView);
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
            imageBarriers.emplace_back(VkImageMemoryBarrier2{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                nullptr,
                VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                shadow_map.handle,
                {VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
        }

        imageBarriers.emplace_back(VkImageMemoryBarrier2{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            nullptr,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, // So that we can keep the barrier before the render pass
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            hdrBuffer.handle,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});

        const VkDependencyInfo dependencies = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            nullptr,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            nullptr,
            0,
            nullptr,
            static_cast<u32>(imageBarriers.size()),
            imageBarriers.data(),
        };

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    if (backend.mustTransitionSwapchain)
    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        std::vector<VkImageMemoryBarrier2> imageBarriers;

        for (u32 swapchainImageIndex = 0; swapchainImageIndex < static_cast<u32>(backend.presentInfo.images.size());
             swapchainImageIndex++)
        {
            imageBarriers.emplace_back(VkImageMemoryBarrier2{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                nullptr,
                VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                VK_ACCESS_2_MEMORY_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                backend.presentInfo.images[swapchainImageIndex],
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
        }

        imageBarriers.emplace_back(VkImageMemoryBarrier2{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            nullptr,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            resources.main_pass_resources.depthBuffer.handle,
            {VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});

        imageBarriers.emplace_back(VkImageMemoryBarrier2KHR{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            nullptr,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            resources.gui_pass_resources.guiTexture.handle,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});

        const VkDependencyInfo dependencies = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            nullptr,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            nullptr,
            0,
            nullptr,
            static_cast<u32>(imageBarriers.size()),
            imageBarriers.data(),
        };

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);

        backend.mustTransitionSwapchain = false;
    }

    record_culling_command_buffer(backend.options.freeze_culling, cmdBuffer, prepared, resources.cull_resources);

    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        std::vector<VkImageMemoryBarrier2> imageBarriers;

        for (ImageInfo& shadow_map : resources.shadow_map_resources.shadowMap)
        {
            imageBarriers.emplace_back(VkImageMemoryBarrier2{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                nullptr,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                shadow_map.handle,
                {VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
        }

        const VkDependencyInfo dependencies = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            nullptr,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            nullptr,
            0,
            nullptr,
            static_cast<u32>(imageBarriers.size()),
            imageBarriers.data(),
        };

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    record_shadow_map_command_buffer(cmdBuffer, backend, prepared, resources.shadow_map_resources,
                                     resources.cull_resources, resources.mesh_cache.vertexBufferPosition.buffer);

    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        std::vector<VkImageMemoryBarrier2> imageBarriers;

        for (ImageInfo& shadow_map : resources.shadow_map_resources.shadowMap)
        {
            imageBarriers.emplace_back(VkImageMemoryBarrier2{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                nullptr,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                shadow_map.handle,
                {VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
        }

        const VkDependencyInfo dependencies = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            nullptr,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            nullptr,
            0,
            nullptr,
            static_cast<u32>(imageBarriers.size()),
            imageBarriers.data(),
        };

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        std::vector<VkImageMemoryBarrier2> imageBarriers;

        imageBarriers.emplace_back(VkImageMemoryBarrier2{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            nullptr,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            hdrBuffer.handle,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});

        const VkDependencyInfo dependencies = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            nullptr,
            VK_FLAGS_NONE,
            0,
            nullptr,
            0,
            nullptr,
            static_cast<u32>(imageBarriers.size()),
            imageBarriers.data(),
        };

        vkCmdSetEvent2(cmdBuffer.handle, resources.my_event, &dependencies);
        vkCmdWaitEvents2(cmdBuffer.handle, 1, &resources.my_event, &dependencies);
    }

    record_main_pass_command_buffer(
        cmdBuffer, backend, prepared, resources.main_pass_resources, resources.cull_resources, resources.mesh_cache,
        backbufferExtent, get_frame_graph_texture_view(resources.framegraph_resources, main_hdr_usage_handle));

    // Render GUI
    {
        {
            std::vector<VkImageMemoryBarrier2> imageBarriers;

            imageBarriers.emplace_back(VkImageMemoryBarrier2{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                nullptr,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                resources.gui_pass_resources.guiTexture.handle,
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});

            const VkDependencyInfo dependencies = {
                VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                nullptr,
                VK_DEPENDENCY_BY_REGION_BIT,
                0,
                nullptr,
                0,
                nullptr,
                static_cast<u32>(imageBarriers.size()),
                imageBarriers.data(),
            };

            vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
        }

        record_gui_command_buffer(cmdBuffer, resources.gui_pass_resources);
    }

    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        std::vector<VkImageMemoryBarrier2> imageBarriers;

        imageBarriers.emplace_back(VkImageMemoryBarrier2{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            nullptr,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            hdrBuffer.handle,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});

        imageBarriers.emplace_back(VkImageMemoryBarrier2{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            nullptr,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            resources.gui_pass_resources.guiTexture.handle,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});

        const VkDependencyInfo dependencies = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            nullptr,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            nullptr,
            0,
            nullptr,
            static_cast<u32>(imageBarriers.size()),
            imageBarriers.data(),
        };

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
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
