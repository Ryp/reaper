////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
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
#include "renderer/window/Event.h"
#include "renderer/window/Window.h"

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
        resize_swapchain_pass_resources(root, backend, resources.swapchain_pass_resources, new_swapchain_extent);

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

    log_debug(root, "vulkan: image index = {}", current_swapchain_index);

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

    prepare_shadow_map_objects(root, backend, prepared, resources.shadow_map_resources);

    upload_culling_resources(backend, prepared, resources.cull_resources);
    upload_shadow_map_resources(backend, prepared, resources.shadow_map_resources);
    upload_main_pass_frame_resources(backend, prepared, resources.main_pass_resources);
    upload_histogram_frame_resources(backend, resources.histogram_pass_resources, backbufferExtent);
    upload_swapchain_frame_resources(backend, prepared, resources.swapchain_pass_resources);

    update_culling_pass_descriptor_sets(backend, prepared, resources.cull_resources, resources.mesh_cache);
    update_shadow_map_pass_descriptor_sets(backend, prepared, resources.shadow_map_resources);
    update_main_pass_descriptor_sets(backend, resources.main_pass_resources, resources.material_resources,
                                     resources.shadow_map_resources.shadowMapView);
    update_histogram_pass_descriptor_set(backend, resources.histogram_pass_resources,
                                         resources.main_pass_resources.hdrBufferView);
    update_swapchain_pass_descriptor_set(backend, resources.swapchain_pass_resources,
                                         resources.main_pass_resources.hdrBufferView);

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

    if (backend.mustTransitionSwapchain)
    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        for (u32 swapchainImageIndex = 0; swapchainImageIndex < static_cast<u32>(backend.presentInfo.images.size());
             swapchainImageIndex++)
        {
            VkImageMemoryBarrier swapchainImageBarrierInfo = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                0,
                VK_ACCESS_MEMORY_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                backend.physicalDeviceInfo.presentQueueFamilyIndex,
                backend.physicalDeviceInfo.presentQueueFamilyIndex,
                backend.presentInfo.images[swapchainImageIndex],
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

            vkCmdPipelineBarrier(cmdBuffer.handle, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                 &swapchainImageBarrierInfo);
        }

        backend.mustTransitionSwapchain = false;
    }

    record_culling_command_buffer(backend.options.freeze_culling, cmdBuffer, prepared, resources.cull_resources);

    record_shadow_map_command_buffer(cmdBuffer, backend, prepared, resources.shadow_map_resources,
                                     resources.cull_resources, resources.mesh_cache.vertexBufferPosition.buffer);

    record_main_pass_command_buffer(cmdBuffer, backend, prepared, resources.main_pass_resources,
                                    resources.cull_resources, resources.mesh_cache, backbufferExtent);

    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        VkImageMemoryBarrier hdrImageBarrierInfo = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            0,
            VK_ACCESS_MEMORY_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            0,
            resources.main_pass_resources.hdrBuffer.handle,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

        vkCmdPipelineBarrier(cmdBuffer.handle, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &hdrImageBarrierInfo);
    }

    record_histogram_command_buffer(cmdBuffer, frame_data, resources.histogram_pass_resources);

    record_swapchain_command_buffer(cmdBuffer, frame_data, resources.swapchain_pass_resources,
                                    backend.presentInfo.imageViews[current_swapchain_index]);

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
}
} // namespace Reaper
