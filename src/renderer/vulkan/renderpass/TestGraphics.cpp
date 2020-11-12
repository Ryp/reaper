////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TestGraphics.h"

#include "Culling.h"
#include "CullingConstants.h"
#include "MainPass.h"
#include "ShadowMap.h"

#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/Debug.h"
#include "renderer/vulkan/Memory.h"
#include "renderer/vulkan/Swapchain.h"
#include "renderer/vulkan/SwapchainRendererBase.h"
#include "renderer/vulkan/Test.h"
#include "renderer/vulkan/api/Vulkan.h"
#include "renderer/vulkan/api/VulkanStringConversion.h"

#include "renderer/Camera.h"
#include "renderer/PrepareBuckets.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/window/Event.h"
#include "renderer/window/Window.h"

#include "input/DS4.h"

#include "mesh/Mesh.h"
#include "mesh/ModelLoader.h"

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
namespace
{
    constexpr bool UseReverseZ = true;

    void cmd_insert_compute_to_compute_barrier(VkCommandBuffer cmdBuffer)
    {
        std::array<VkMemoryBarrier, 1> memoryBarriers = {VkMemoryBarrier{
            VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
        }};

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             static_cast<u32>(memoryBarriers.size()), memoryBarriers.data(), 0, nullptr, 0, nullptr);
    }

    VkClearValue VkClearColor(const glm::vec4& color)
    {
        VkClearValue clearValue;

        clearValue.color.float32[0] = color.x;
        clearValue.color.float32[1] = color.y;
        clearValue.color.float32[2] = color.z;
        clearValue.color.float32[3] = color.w;

        return clearValue;
    }

    VkClearValue VkClearDepthStencil(float depth, u32 stencil)
    {
        VkClearValue clearValue;

        clearValue.depthStencil.depth = depth;
        clearValue.depthStencil.stencil = stencil;

        return clearValue;
    }

    void cmd_transition_swapchain_layout(VulkanBackend& backend, VkCommandBuffer commandBuffer)
    {
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
                backend.physicalDeviceInfo.presentQueueIndex,
                backend.physicalDeviceInfo.presentQueueIndex,
                backend.presentInfo.images[swapchainImageIndex],
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &swapchainImageBarrierInfo);
        }
    }
} // namespace

void vulkan_test_graphics(ReaperRoot& root, VulkanBackend& backend, GlobalResources& resources)
{
    // Read mesh file
    std::ifstream modelFile("res/model/ship.obj");
    const Mesh    mesh = ModelLoader::loadOBJ(modelFile);

    BufferInfo vertexBufferPosition =
        create_buffer(root, backend.device, "Position buffer",
                      DefaultGPUBufferProperties(mesh.vertices.size(), sizeof(mesh.vertices[0]),
                                                 GPUBufferUsage::VertexBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);
    BufferInfo vertexBufferNormal = create_buffer(
        root, backend.device, "Normal buffer",
        DefaultGPUBufferProperties(mesh.normals.size(), sizeof(mesh.normals[0]), GPUBufferUsage::VertexBuffer),
        backend.vma_instance);
    BufferInfo vertexBufferUV =
        create_buffer(root, backend.device, "UV buffer",
                      DefaultGPUBufferProperties(mesh.uvs.size(), sizeof(mesh.uvs[0]), GPUBufferUsage::VertexBuffer),
                      backend.vma_instance);
    BufferInfo indexBuffer = create_buffer(
        root, backend.device, "Index buffer",
        DefaultGPUBufferProperties(mesh.indexes.size(), sizeof(mesh.indexes[0]), GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    upload_buffer_data(backend.device, backend.vma_instance, vertexBufferPosition, mesh.vertices.data(),
                       mesh.vertices.size() * sizeof(mesh.vertices[0]));
    upload_buffer_data(backend.device, backend.vma_instance, vertexBufferNormal, mesh.normals.data(),
                       mesh.normals.size() * sizeof(mesh.normals[0]));
    upload_buffer_data(backend.device, backend.vma_instance, vertexBufferUV, mesh.uvs.data(),
                       mesh.uvs.size() * sizeof(mesh.uvs[0]));
    upload_buffer_data(backend.device, backend.vma_instance, indexBuffer, mesh.indexes.data(),
                       mesh.indexes.size() * sizeof(mesh.indexes[0]));

    // Culling Pass
    CullResources cull_resources = create_culling_resources(root, backend);

    // Shadow Pass
    ShadowMapResources shadow_map_resources = create_shadow_map_resources(root, backend);

    // Main Pass
    const glm::uvec2  swapchain_extent(backend.presentInfo.surfaceExtent.width,
                                      backend.presentInfo.surfaceExtent.height);
    MainPassResources main_pass_resources = create_main_pass_resources(root, backend, swapchain_extent);

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
    build_scene_graph(scene, &mesh);

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
                        resize_main_pass_depth_buffer(root, backend, main_pass_resources, new_swapchain_extent);
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
            uint32_t imageIndex = 0;

            constexpr u32 MaxAcquireTryCount = 10;

            for (u32 acquireTryCount = 0; acquireTryCount < MaxAcquireTryCount; acquireTryCount++)
            {
                log_debug(root, "vulkan: acquiring frame try #{}", acquireTryCount);
                acquireResult =
                    vkAcquireNextImageKHR(backend.device, backend.presentInfo.swapchain, acquireTimeoutUs,
                                          backend.presentInfo.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

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

            log_debug(root, "vulkan: image index = {}", imageIndex);

            const float                 depthClearValue = UseReverseZ ? 0.f : 1.f;
            const glm::fvec4            clearColor = {1.0f, 0.8f, 0.4f, 0.0f};
            std::array<VkClearValue, 2> clearValues = {VkClearColor(clearColor),
                                                       VkClearDepthStencil(depthClearValue, 0)};
            const VkExtent2D            backbufferExtent = backend.presentInfo.surfaceExtent;
            const VkRect2D              blitPassRect = {{0, 0}, backbufferExtent};

            std::array<VkImageView, 2> main_pass_framebuffer_views = {backend.presentInfo.imageViews[imageIndex],
                                                                      main_pass_resources.depthBufferView};

            VkRenderPassAttachmentBeginInfo main_pass_attachments = {
                VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO, nullptr, main_pass_framebuffer_views.size(),
                main_pass_framebuffer_views.data()};

            VkRenderPassBeginInfo blitRenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                             &main_pass_attachments,
                                                             main_pass_resources.mainRenderPass,
                                                             main_pass_resources.swapchain_framebuffer,
                                                             blitPassRect,
                                                             static_cast<u32>(clearValues.size()),
                                                             clearValues.data()};

            // Only wait for a previous frame fence
            if (frameIndex > 0)
            {
                log_debug(root, "vulkan: wait for fence");
                VkResult waitResult;
                {
                    REAPER_PROFILE_SCOPE("Vulkan", MP_ORANGE);
                    const u64 waitTimeoutUs = 100000000;
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

            log_debug(root, "vulkan: record command buffer");
            Assert(vkResetCommandBuffer(resources.gfxCmdBuffer, 0) == VK_SUCCESS);

            const auto timeSecs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
            const auto timeDeltaMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFrameStart);
            const float      timeMs = static_cast<float>(timeSecs.count()) * 0.001f;
            const float      timeDtSecs = static_cast<float>(timeDeltaMs.count()) * 0.001f;
            const glm::uvec2 backbuffer_viewport_extent(backbufferExtent.width, backbufferExtent.height);

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

            // NOTE: double buffer this or something
            Assert(vkResetDescriptorPool(backend.device, backend.frame_descriptor_pool, VK_FLAGS_NONE) == VK_SUCCESS);

            cull_resources.passes.clear();
            for (const CullPassData& cull_pass : prepared.cull_passes)
            {
                CullPassResources& cull_pass_resources = cull_resources.passes.emplace_back();
                cull_pass_resources = create_culling_pass_descriptor_sets(
                    root, backend, cull_resources, cull_pass.pass_index, indexBuffer, vertexBufferPosition);
            }

            for (u32 i = 0; i < shadow_map_resources.shadowMap.size(); i++)
            {
                vmaDestroyImage(backend.vma_instance, shadow_map_resources.shadowMap[i].handle,
                                shadow_map_resources.shadowMap[i].allocation);
                vkDestroyImageView(backend.device, shadow_map_resources.shadowMapView[i], nullptr);
                vkDestroyFramebuffer(backend.device, shadow_map_resources.shadowMapFramebuffer[i], nullptr);
            }

            shadow_map_resources.passes.clear();
            shadow_map_resources.shadowMap.clear();
            shadow_map_resources.shadowMapView.clear();
            shadow_map_resources.shadowMapFramebuffer.clear();
            for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
            {
                ShadowPassResources& shadow_map_pass_resources = shadow_map_resources.passes.emplace_back();
                shadow_map_pass_resources =
                    create_shadow_map_pass_descriptor_sets(root, backend, shadow_map_resources, shadow_pass);

                const GPUTextureProperties texture_properties =
                    get_shadow_map_texture_properties(shadow_pass.shadow_map_size);

                ImageInfo& shadow_map = shadow_map_resources.shadowMap.emplace_back();

                shadow_map = create_image(root, backend.device, "Shadow Map", texture_properties, backend.vma_instance);

                shadow_map_resources.shadowMapView.push_back(create_depth_image_view(root, backend.device, shadow_map));

                shadow_map_resources.shadowMapFramebuffer.push_back(
                    create_shadow_map_framebuffer(backend, shadow_map_resources.shadowMapPass, shadow_map.properties));
            }

            // FIXME
            VkDescriptorSet mainPassDescriptorSet =
                create_main_pass_descriptor_set(root, backend, main_pass_resources, shadow_map_resources.shadowMapView);

            // NOTE: do partial copies if possible
            upload_buffer_data(backend.device, backend.vma_instance, main_pass_resources.drawPassConstantBuffer,
                               &prepared.draw_pass_params, sizeof(DrawPassParams));
            upload_buffer_data(backend.device, backend.vma_instance, main_pass_resources.drawInstanceConstantBuffer,
                               prepared.draw_instance_params.data(),
                               prepared.draw_instance_params.size() * sizeof(DrawInstanceParams));

            shadow_map_prepare_buffers(backend, prepared, shadow_map_resources);
            culling_prepare_buffers(cull_options, backend, prepared, cull_resources);

            VkCommandBufferBeginInfo cmdBufferBeginInfo = {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                0,      // Not caring yet
                nullptr // No inheritance yet
            };

            Assert(vkBeginCommandBuffer(resources.gfxCmdBuffer, &cmdBufferBeginInfo) == VK_SUCCESS);

#if defined(REAPER_USE_MICROPROFILE)
            MicroProfileThreadLogGpu* pGpuLog = MicroProfileThreadLogGpuAlloc();
            MICROPROFILE_GPU_BEGIN(resources.gfxCmdBuffer, pGpuLog);
            MICROPROFILE_GPU_ENTERI_L(pGpuLog, "GPU", "Frame", MP_BLUE2);
#endif

            if (mustTransitionSwapchain)
            {
                REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Barrier", MP_RED);

                cmd_transition_swapchain_layout(backend, resources.gfxCmdBuffer);
                mustTransitionSwapchain = false;
            }

            // Culling
            {
                REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Culling Pass", MP_DARKGOLDENROD);

                if (!cull_options.freeze_culling)
                {
                    vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                      cull_resources.cullPipe.pipeline);

                    for (const CullPassData& cull_pass : prepared.cull_passes)
                    {
                        const CullPassResources& cull_pass_resources = cull_resources.passes[cull_pass.pass_index];

                        vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                cull_resources.cullPipe.pipelineLayout, 0, 1,
                                                &cull_pass_resources.cull_descriptor_set, 0, nullptr);

                        for (const CullCmd& command : cull_pass.cull_cmds)
                        {
                            vkCmdPushConstants(resources.gfxCmdBuffer, cull_resources.cullPipe.pipelineLayout,
                                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(command.push_constants),
                                               &command.push_constants);

                            vkCmdDispatch(resources.gfxCmdBuffer,
                                          div_round_up(command.push_constants.triangleCount, ComputeCullingGroupSize),
                                          command.instanceCount, 1);
                        }
                    }
                }

                {
                    REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Barrier", MP_RED);
                    cmd_insert_compute_to_compute_barrier(resources.gfxCmdBuffer);
                }

                // Compaction prepare pass
                {
                    vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                      cull_resources.compactPrepPipe.pipeline);

                    for (const CullPassData& cull_pass : prepared.cull_passes)
                    {
                        const CullPassResources& cull_pass_resources = cull_resources.passes[cull_pass.pass_index];

                        vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                cull_resources.compactPrepPipe.pipelineLayout, 0, 1,
                                                &cull_pass_resources.compact_prep_descriptor_set, 0, nullptr);

                        vkCmdDispatch(resources.gfxCmdBuffer, 1, 1, 1);
                    }
                }

                {
                    REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Barrier", MP_RED);
                    cmd_insert_compute_to_compute_barrier(resources.gfxCmdBuffer);
                }

                // Compaction pass
                {
                    vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                      cull_resources.compactionPipe.pipeline);

                    for (const CullPassData& cull_pass : prepared.cull_passes)
                    {
                        const CullPassResources& cull_pass_resources = cull_resources.passes[cull_pass.pass_index];

                        vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                cull_resources.compactionPipe.pipelineLayout, 0, 1,
                                                &cull_pass_resources.compact_descriptor_set, 0, nullptr);

                        vkCmdDispatchIndirect(resources.gfxCmdBuffer,
                                              cull_resources.compactionIndirectDispatchBuffer.buffer, 0);
                    }
                }
            }

            {
                REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Barrier", MP_RED);

                std::array<VkMemoryBarrier, 1> memoryBarriers = {VkMemoryBarrier{
                    VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                    nullptr,
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                }};

                vkCmdPipelineBarrier(resources.gfxCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0,
                                     static_cast<u32>(memoryBarriers.size()), memoryBarriers.data(), 0, nullptr, 0,
                                     nullptr);
            }

            // Shadow pass
            for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
            {
                REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Shadow Pass", MP_DARKGOLDENROD);

                const ShadowPassResources& shadow_map_pass_resources =
                    shadow_map_resources.passes[shadow_pass.pass_index];

                const VkClearValue clearValue =
                    VkClearDepthStencil(UseReverseZ ? 0.f : 1.f, 0); // NOTE: handle reverse Z more gracefully
                const VkExtent2D framebuffer_extent = {shadow_pass.shadow_map_size.x, shadow_pass.shadow_map_size.y};
                const VkRect2D   pass_rect = {{0, 0}, framebuffer_extent};

                const VkImageView   shadowMapView = shadow_map_resources.shadowMapView[shadow_pass.pass_index];
                const VkFramebuffer shadowMapFramebuffer =
                    shadow_map_resources.shadowMapFramebuffer[shadow_pass.pass_index];

                VkRenderPassAttachmentBeginInfo shadowMapRenderPassAttachment = {
                    VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO, nullptr, 1, &shadowMapView};

                VkRenderPassBeginInfo shadowMapRenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                                      &shadowMapRenderPassAttachment,
                                                                      shadow_map_resources.shadowMapPass,
                                                                      shadowMapFramebuffer,
                                                                      pass_rect,
                                                                      1,
                                                                      &clearValue};

                vkCmdBeginRenderPass(resources.gfxCmdBuffer, &shadowMapRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  shadow_map_resources.pipe.pipeline);

                {
                    const VkViewport viewport = {0.0f,
                                                 0.0f,
                                                 static_cast<float>(framebuffer_extent.width),
                                                 static_cast<float>(framebuffer_extent.height),
                                                 0.0f,
                                                 1.0f};
                    const VkRect2D   scissor = {{0, 0}, framebuffer_extent};

                    vkCmdSetViewport(resources.gfxCmdBuffer, 0, 1, &viewport);
                    vkCmdSetScissor(resources.gfxCmdBuffer, 0, 1, &scissor);
                }

                std::vector<VkBuffer> vertexBuffers = {
                    vertexBufferPosition.buffer,
                };
                std::vector<VkDeviceSize> vertexBufferOffsets = {0};

                Assert(vertexBuffers.size() == vertexBufferOffsets.size());
                vkCmdBindIndexBuffer(resources.gfxCmdBuffer, cull_resources.dynamicIndexBuffer.buffer, 0,
                                     get_vk_culling_index_type());
                vkCmdBindVertexBuffers(resources.gfxCmdBuffer, 0, static_cast<u32>(vertexBuffers.size()),
                                       vertexBuffers.data(), vertexBufferOffsets.data());
                vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        shadow_map_resources.pipe.pipelineLayout, 0, 1,
                                        &shadow_map_pass_resources.descriptor_set, 0, nullptr);

                const u32 draw_buffer_offset =
                    shadow_pass.culling_pass_index * MaxIndirectDrawCount * sizeof(VkDrawIndexedIndirectCommand);
                const u32 draw_buffer_max_count = MaxIndirectDrawCount;

                if (cull_options.use_compacted_draw)
                {
                    const u32 draw_buffer_count_offset = shadow_pass.culling_pass_index * 1 * sizeof(u32);
                    vkCmdDrawIndexedIndirectCount(
                        resources.gfxCmdBuffer, cull_resources.compactIndirectDrawBuffer.buffer, draw_buffer_offset,
                        cull_resources.compactIndirectDrawCountBuffer.buffer, draw_buffer_count_offset,
                        draw_buffer_max_count, cull_resources.compactIndirectDrawBuffer.descriptor.elementSize);
                }
                else
                {
                    const u32 draw_buffer_count_offset =
                        shadow_pass.culling_pass_index * IndirectDrawCountCount * sizeof(u32);
                    vkCmdDrawIndexedIndirectCount(resources.gfxCmdBuffer, cull_resources.indirectDrawBuffer.buffer,
                                                  draw_buffer_offset, cull_resources.indirectDrawCountBuffer.buffer,
                                                  draw_buffer_count_offset, draw_buffer_max_count,
                                                  cull_resources.indirectDrawBuffer.descriptor.elementSize);
                }

                vkCmdEndRenderPass(resources.gfxCmdBuffer);
            }

            {
                REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Barrier", MP_RED);

                std::vector<VkImageMemoryBarrier> shadowMapImageBarrierInfo;

                for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
                {
                    // FIXME synchronize with a single call
                    shadowMapImageBarrierInfo.push_back(VkImageMemoryBarrier{
                        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        nullptr,
                        0,
                        VK_ACCESS_MEMORY_READ_BIT,
                        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        backend.physicalDeviceInfo.graphicsQueueIndex,
                        backend.physicalDeviceInfo.graphicsQueueIndex,
                        shadow_map_resources.shadowMap[shadow_pass.pass_index].handle,
                        {VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
                }

                // FIXME is this a noop when there's no barriers?
                vkCmdPipelineBarrier(resources.gfxCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                                     static_cast<u32>(shadowMapImageBarrierInfo.size()),
                                     shadowMapImageBarrierInfo.data());
            }

            // Draw pass
            {
                REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Draw Pass", MP_DARKGOLDENROD);

                const u32 pass_index = prepared.draw_culling_pass_index;

                vkCmdBeginRenderPass(resources.gfxCmdBuffer, &blitRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  main_pass_resources.mainPipe.pipeline);

                const VkViewport blitViewport = {
                    0.0f, 0.0f, static_cast<float>(backbufferExtent.width), static_cast<float>(backbufferExtent.height),
                    0.0f, 1.0f};
                const VkRect2D blitScissor = {{0, 0}, backbufferExtent};

                vkCmdSetViewport(resources.gfxCmdBuffer, 0, 1, &blitViewport);
                vkCmdSetScissor(resources.gfxCmdBuffer, 0, 1, &blitScissor);

                std::vector<VkBuffer> vertexBuffers = {
                    vertexBufferPosition.buffer,
                    vertexBufferNormal.buffer,
                    vertexBufferUV.buffer,
                };
                std::vector<VkDeviceSize> vertexBufferOffsets = {
                    0,
                    0,
                    0,
                };
                Assert(vertexBuffers.size() == vertexBufferOffsets.size());
                vkCmdBindIndexBuffer(resources.gfxCmdBuffer, cull_resources.dynamicIndexBuffer.buffer, 0,
                                     get_vk_culling_index_type());
                vkCmdBindVertexBuffers(resources.gfxCmdBuffer, 0, static_cast<u32>(vertexBuffers.size()),
                                       vertexBuffers.data(), vertexBufferOffsets.data());
                vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        main_pass_resources.mainPipe.pipelineLayout, 0, 1, &mainPassDescriptorSet, 0,
                                        nullptr);

                const u32 draw_buffer_offset = pass_index * MaxIndirectDrawCount * sizeof(VkDrawIndexedIndirectCommand);
                const u32 draw_buffer_max_count = MaxIndirectDrawCount;

                if (cull_options.use_compacted_draw)
                {
                    const u32 draw_buffer_count_offset = pass_index * 1 * sizeof(u32);
                    vkCmdDrawIndexedIndirectCount(
                        resources.gfxCmdBuffer, cull_resources.compactIndirectDrawBuffer.buffer, draw_buffer_offset,
                        cull_resources.compactIndirectDrawCountBuffer.buffer, draw_buffer_count_offset,
                        draw_buffer_max_count, cull_resources.compactIndirectDrawBuffer.descriptor.elementSize);
                }
                else
                {
                    const u32 draw_buffer_count_offset = pass_index * IndirectDrawCountCount * sizeof(u32);
                    vkCmdDrawIndexedIndirectCount(resources.gfxCmdBuffer, cull_resources.indirectDrawBuffer.buffer,
                                                  draw_buffer_offset, cull_resources.indirectDrawCountBuffer.buffer,
                                                  draw_buffer_count_offset, draw_buffer_max_count,
                                                  cull_resources.indirectDrawBuffer.descriptor.elementSize);
                }

                vkCmdEndRenderPass(resources.gfxCmdBuffer);
            }

#if defined(REAPER_USE_MICROPROFILE)
            MICROPROFILE_GPU_LEAVE_L(pGpuLog);

            const u64 microprofile_data = MicroProfileGpuEnd(pGpuLog);
            MicroProfileThreadLogGpuFree(pGpuLog);

            MICROPROFILE_GPU_SUBMIT(MicroProfileGetGlobalGpuQueue(), microprofile_data);
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
                0,                    // uint32_t		  waitSemaphoreValueCount;
                nullptr,              // const uint64_t*	  pWaitSemaphoreValues;
                signal_values.size(), // uint32_t		  signalSemaphoreValueCount;
                signal_values.data()  // const uint64_t*	  pSignalSemaphoreValues;
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
                                            &imageIndex,
                                            &presentResult};
            Assert(vkQueuePresentKHR(backend.deviceInfo.presentQueue, &presentInfo) == VK_SUCCESS);
            Assert(presentResult == VK_SUCCESS);

            if (saveMyLaptop)
            {
                REAPER_PROFILE_SCOPE("Vulkan", MP_GREEN);
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

    destroy_main_pass_resources(backend, main_pass_resources);
    destroy_shadow_map_resources(backend, shadow_map_resources);
    destroy_culling_resources(backend, cull_resources);

    vmaDestroyBuffer(backend.vma_instance, indexBuffer.buffer, indexBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, vertexBufferPosition.buffer, vertexBufferPosition.allocation);
    vmaDestroyBuffer(backend.vma_instance, vertexBufferNormal.buffer, vertexBufferNormal.allocation);
    vmaDestroyBuffer(backend.vma_instance, vertexBufferUV.buffer, vertexBufferUV.allocation);
}
} // namespace Reaper
