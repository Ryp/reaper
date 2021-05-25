////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TestGraphics.h"

#include "Culling.h"
#include "CullingConstants.h"
#include "Frame.h"
#include "HistogramPass.h"
#include "MainPass.h"
#include "ShadowMap.h"
#include "SwapchainPass.h"

#include "renderer/vulkan/Debug.h"
#include "renderer/vulkan/MaterialResources.h"
#include "renderer/vulkan/Memory.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/Swapchain.h"
#include "renderer/vulkan/SwapchainRendererBase.h"
#include "renderer/vulkan/Test.h"
#include "renderer/vulkan/api/Vulkan.h"
#include "renderer/vulkan/api/VulkanStringConversion.h"

#include "renderer/Camera.h"
#include "renderer/Mesh2.h"
#include "renderer/PrepareBuckets.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/window/Event.h"
#include "renderer/window/Window.h"

#include "input/DS4.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/BitTricks.h"
#include "core/Profile.h"
#include "core/memory/Allocator.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <chrono>
#include <thread>

namespace Reaper
{
void vulkan_test_graphics(ReaperRoot& root, VulkanBackend& backend, GlobalResources& resources)
{
    std::vector<Mesh2> mesh2_instances;
    MeshCache          mesh_cache = create_mesh_cache(root, backend);

    load_meshes(backend, mesh_cache, mesh2_instances);

    MaterialResources material_resources = create_material_resources(root, backend);

    CullResources cull_resources = create_culling_resources(root, backend);

    ShadowMapResources shadow_map_resources = create_shadow_map_resources(root, backend);

    const glm::uvec2  swapchain_extent(backend.presentInfo.surfaceExtent.width,
                                      backend.presentInfo.surfaceExtent.height);
    MainPassResources main_pass_resources =
        create_main_pass_resources(root, backend, swapchain_extent, material_resources.descSetLayout);

    HistogramPassResources histogram_pass_resources = create_histogram_pass_resources(root, backend);

    SwapchainPassResources swapchain_pass_resources = create_swapchain_pass_resources(root, backend, swapchain_extent);

    // Create fence
    VkFenceCreateInfo fenceInfo = {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
        0 // Not signaled by default
    };

    VkFence drawFence = VK_NULL_HANDLE;
    vkCreateFence(backend.device, &fenceInfo, nullptr, &drawFence);
    log_debug(root, "vulkan: created fence with handle: {}", static_cast<void*>(drawFence));

    IWindow* window = root.renderer->window;
    log_info(root, "window: map window");
    window->map();

    const u64 MaxFrameCount = 0;

    bool mustTransitionSwapchain = true;
    bool saveMyLaptop = true;
    bool shouldExit = false;
    u64  frameIndex = 0;
    bool pauseAnimation = false;

    CullOptions cull_options;
    cull_options.freeze_culling = false;
    cull_options.use_compacted_draw = true;

    DS4 ds4("/dev/input/js0");

    CameraState camera_state = {};
    camera_state.position = glm::vec3(-5.f, 0.f, 0.f);

    SceneGraph scene;
    build_scene_graph(scene, mesh2_instances.data(), mesh2_instances.size());

    const auto startTime = std::chrono::system_clock::now();
    auto       lastFrameStart = startTime;

    VkSemaphore timelineSemaphore = VK_NULL_HANDLE;
    {
        VkSemaphoreTypeCreateInfo timelineCreateInfo;
        timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineCreateInfo.pNext = NULL;
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue = 0;

        VkSemaphoreCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = &timelineCreateInfo;
        createInfo.flags = 0;

        vkCreateSemaphore(backend.device, &createInfo, NULL, &timelineSemaphore);
    }

    while (!shouldExit)
    {
        const auto currentTime = std::chrono::system_clock::now();
        {
            REAPER_PROFILE_SCOPE("Vulkan", MP_YELLOW);
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

                    {
                        vkQueueWaitIdle(backend.deviceInfo.presentQueue);

                        resize_vulkan_wm_swapchain(root, backend, backend.presentInfo, {width, height});

                        const glm::uvec2 new_swapchain_extent(backend.presentInfo.surfaceExtent.width,
                                                              backend.presentInfo.surfaceExtent.height);

                        resize_main_pass_resources(root, backend, main_pass_resources, new_swapchain_extent);
                        resize_swapchain_pass_resources(root, backend, swapchain_pass_resources, new_swapchain_extent);
                    }
                    mustTransitionSwapchain = true;
                }
                else if (event.type == Window::EventType::KeyPress)
                {
                    shouldExit = true;
                    log_warning(root, "window: key press detected: now exiting...");
                }
                else
                {
                    log_warning(root, "window: an unknown event has been caught and will not be handled");
                }
            }

            log_debug(root, "vulkan: begin frame {}", frameIndex);

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
                continue;
                break;
            default:
                Assert(acquireResult == VK_SUCCESS);
                break;
            }

            log_debug(root, "vulkan: image index = {}", current_swapchain_index);

            // Only wait for a previous frame fence
            if (frameIndex > 0)
            {
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

            const auto timeSecs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
            const auto timeDeltaMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFrameStart);
            const float      timeMs = static_cast<float>(timeSecs.count()) * 0.001f;
            const float      timeDtSecs = static_cast<float>(timeDeltaMs.count()) * 0.001f;
            const glm::uvec2 backbuffer_viewport_extent(backbufferExtent.width, backbufferExtent.height);

            FrameData frame_data = {};
            frame_data.backbufferExtent = backbufferExtent;

            ds4.update();

            if (ds4.isPressed(DS4::L1))
                toggle(pauseAnimation);

            if (ds4.isPressed(DS4::Square))
                toggle(cull_options.freeze_culling);

            const glm::vec2 yaw_pitch_delta =
                glm::vec2(ds4.getAxis(DS4::RightAnalogY), -ds4.getAxis(DS4::RightAnalogX)) * timeDtSecs;
            const glm::vec2 forward_side_delta =
                glm::vec2(ds4.getAxis(DS4::LeftAnalogY), ds4.getAxis(DS4::LeftAnalogX)) * timeDtSecs;

            update_camera_state(camera_state, yaw_pitch_delta, forward_side_delta);

            const glm::mat4 view_matrix = compute_camera_view_matrix(camera_state);

            float animationTimeMs = pauseAnimation ? 0.f : timeMs;

            update_scene_graph(scene, animationTimeMs, backbuffer_viewport_extent, view_matrix);

            PreparedData prepared;
            prepare_scene(scene, prepared);

            prepare_shadow_map_objects(root, backend, prepared, shadow_map_resources);

            upload_culling_resources(backend, cull_options, prepared, cull_resources);
            upload_shadow_map_resources(backend, prepared, shadow_map_resources);
            upload_main_pass_frame_resources(backend, prepared, main_pass_resources);
            upload_histogram_frame_resources(backend, histogram_pass_resources, backbufferExtent);
            upload_buffer_data(backend.device, backend.vma_instance, swapchain_pass_resources.passConstantBuffer,
                               &prepared.swapchain_pass_params, sizeof(SwapchainPassParams));

            update_culling_pass_descriptor_sets(backend, prepared, cull_resources, mesh_cache);
            update_shadow_map_pass_descriptor_sets(backend, prepared, shadow_map_resources);
            update_main_pass_descriptor_set(backend, main_pass_resources, shadow_map_resources.shadowMapView);
            update_histogram_pass_descriptor_set(backend, histogram_pass_resources, main_pass_resources.hdrBufferView);
            update_swapchain_pass_descriptor_set(backend, swapchain_pass_resources, main_pass_resources.hdrBufferView);
            update_material_descriptor_set(backend, material_resources);

            log_debug(root, "vulkan: record command buffer");
            Assert(vkResetCommandBuffer(resources.gfxCmdBuffer, 0) == VK_SUCCESS);

            VkCommandBufferBeginInfo cmdBufferBeginInfo = {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                0,      // Not caring yet
                nullptr // No inheritance yet
            };

            Assert(vkBeginCommandBuffer(resources.gfxCmdBuffer, &cmdBufferBeginInfo) == VK_SUCCESS);

#if defined(REAPER_USE_MICROPROFILE)
            MICROPROFILE_GPU_SET_CONTEXT(resources.gfxCmdBuffer, MicroProfileGetGlobalGpuThreadLog());
#endif

            record_material_upload_command_buffer(backend, material_resources.staging, resources.gfxCmdBuffer);

            if (mustTransitionSwapchain)
            {
                REAPER_PROFILE_SCOPE_GPU("Barrier", MP_RED);

                for (u32 swapchainImageIndex = 0;
                     swapchainImageIndex < static_cast<u32>(backend.presentInfo.images.size());
                     swapchainImageIndex++)
                {
                    VkImageMemoryBarrier swapchainImageBarrierInfo = {
                        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        nullptr,
                        0,
                        VK_ACCESS_MEMORY_READ_BIT,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        backend.physicalDeviceInfo.presentQueueIndex,
                        backend.physicalDeviceInfo.presentQueueIndex,
                        backend.presentInfo.images[swapchainImageIndex],
                        {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

                    vkCmdPipelineBarrier(resources.gfxCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                         &swapchainImageBarrierInfo);
                }

                mustTransitionSwapchain = false;
            }

            record_culling_command_buffer(cull_options, resources.gfxCmdBuffer, prepared, cull_resources);

            record_shadow_map_command_buffer(cull_options, resources.gfxCmdBuffer, backend, prepared,
                                             shadow_map_resources, cull_resources,
                                             mesh_cache.vertexBufferPosition.buffer);

            record_main_pass_command_buffer(cull_options, resources.gfxCmdBuffer, prepared, main_pass_resources,
                                            cull_resources, material_resources, mesh_cache, backbufferExtent);

            {
                REAPER_PROFILE_SCOPE_GPU("Barrier", MP_RED);

                VkImageMemoryBarrier hdrImageBarrierInfo = {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    nullptr,
                    0,
                    VK_ACCESS_MEMORY_READ_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    backend.physicalDeviceInfo.graphicsQueueIndex,
                    backend.physicalDeviceInfo.graphicsQueueIndex,
                    main_pass_resources.hdrBuffer.handle,
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

                vkCmdPipelineBarrier(resources.gfxCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                     &hdrImageBarrierInfo);
            }

            record_histogram_command_buffer(resources.gfxCmdBuffer, frame_data, histogram_pass_resources);

            record_swapchain_command_buffer(resources.gfxCmdBuffer, frame_data, swapchain_pass_resources,
                                            backend.presentInfo.imageViews[current_swapchain_index]);

#if defined(REAPER_USE_MICROPROFILE)
            MicroProfileFlip(resources.gfxCmdBuffer);
#endif

            Assert(vkEndCommandBuffer(resources.gfxCmdBuffer) == VK_SUCCESS);
            // Stop recording

            VkPipelineStageFlags blitWaitDstMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

            std::array<VkSemaphore, 2> semaphores_to_signal = {backend.presentInfo.renderingFinishedSemaphore,
                                                               timelineSemaphore};

            std::array<u64, 2> signal_values = {
                0,              // Not a timeline semaphore
                frameIndex + 1, // Unused timeline semaphore value
            };

            VkTimelineSemaphoreSubmitInfo timelineSemaphoreSubmitInfo = {
                VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
                nullptr,
                0,                    // uint32_t         waitSemaphoreValueCount;
                nullptr,              // const uint64_t*  pWaitSemaphoreValues;
                signal_values.size(), // uint32_t         signalSemaphoreValueCount;
                signal_values.data()  // const uint64_t*  pSignalSemaphoreValues;
            };

            VkSubmitInfo blitSubmitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                           &timelineSemaphoreSubmitInfo,
                                           1,
                                           &backend.presentInfo.imageAvailableSemaphore,
                                           &blitWaitDstMask,
                                           1,
                                           &resources.gfxCmdBuffer,
                                           semaphores_to_signal.size(),
                                           semaphores_to_signal.data()};

            log_debug(root, "vulkan: submit drawing commands");
            Assert(vkQueueSubmit(backend.deviceInfo.graphicsQueue, 1, &blitSubmitInfo, drawFence) == VK_SUCCESS);

            log_debug(root, "vulkan: present");

            VkResult         presentResult;
            VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                            nullptr,
                                            1,
                                            &backend.presentInfo.renderingFinishedSemaphore,
                                            1,
                                            &backend.presentInfo.swapchain,
                                            &current_swapchain_index,
                                            &presentResult};
            Assert(vkQueuePresentKHR(backend.deviceInfo.presentQueue, &presentInfo) == VK_SUCCESS);
            Assert(presentResult == VK_SUCCESS);

            if (saveMyLaptop)
            {
                REAPER_PROFILE_SCOPE("Battery saver wait", MP_GREEN);
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
            }

            frameIndex++;
            if (frameIndex == MaxFrameCount)
                shouldExit = true;
        }

        lastFrameStart = currentTime;
    }

    vkQueueWaitIdle(backend.deviceInfo.presentQueue);

    vkDestroySemaphore(backend.device, timelineSemaphore, nullptr);

    log_info(root, "window: unmap window");
    window->unmap();

    vkDestroyFence(backend.device, drawFence, nullptr);

    destroy_swapchain_pass_resources(backend, swapchain_pass_resources);
    destroy_histogram_pass_resources(backend, histogram_pass_resources);
    destroy_main_pass_resources(backend, main_pass_resources);
    destroy_shadow_map_resources(backend, shadow_map_resources);
    destroy_culling_resources(backend, cull_resources);
    destroy_material_resources(backend, material_resources);

    destroy_mesh_cache(backend, mesh_cache);
}
} // namespace Reaper
