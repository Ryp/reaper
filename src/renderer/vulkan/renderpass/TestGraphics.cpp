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
#include "ShadowConstants.h"
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

    void create_framebuffers(ReaperRoot& /*root*/, VulkanBackend& backend, VkRenderPass renderPass,
                             VkImageView depthBufferView, std::vector<VkFramebuffer>& framebuffers)
    {
        const size_t imgCount = backend.presentInfo.imageCount;

        framebuffers.resize(imgCount);

        for (size_t i = 0; i < imgCount; ++i)
        {
            std::array<const VkImageView, 2> imageViews = {backend.presentInfo.imageViews[i], depthBufferView};

            VkFramebufferCreateInfo framebuffer_create_info = {
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType                sType
                nullptr,                                   // const void                    *pNext
                0,                                         // VkFramebufferCreateFlags       flags
                renderPass,                                // VkRenderPass                   renderPass
                static_cast<u32>(imageViews.size()),       // uint32_t                       attachmentCount
                imageViews.data(),                         // const VkImageView             *pAttachments
                backend.presentInfo.surfaceExtent.width,   // uint32_t                       width
                backend.presentInfo.surfaceExtent.height,  // uint32_t                       height
                1                                          // uint32_t                       layers
            };

            Assert(vkCreateFramebuffer(backend.device, &framebuffer_create_info, nullptr, &framebuffers[i])
                   == VK_SUCCESS);
        }
    }

    void destroy_framebuffers(ReaperRoot& /*root*/, VkDevice device, std::vector<VkFramebuffer>& framebuffers)
    {
        for (size_t i = 0; i < framebuffers.size(); i++)
            vkDestroyFramebuffer(device, framebuffers[i], nullptr);

        framebuffers.clear();
    }
} // namespace

void vulkan_test_graphics(ReaperRoot& root, VulkanBackend& backend, GlobalResources& resources)
{
    // Read mesh file
    std::ifstream modelFile("res/model/suzanne.obj");
    const Mesh    mesh = ModelLoader::loadOBJ(modelFile);

    const glm::uvec2  swapchain_extent(backend.presentInfo.surfaceExtent.width,
                                      backend.presentInfo.surfaceExtent.height);
    MainPassResources main_pass_resources = create_main_pass_resources(root, backend, swapchain_extent);

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
    BufferInfo staticIndexBuffer = create_buffer(
        root, backend.device, "Index buffer",
        DefaultGPUBufferProperties(mesh.indexes.size(), sizeof(mesh.indexes[0]), GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    upload_buffer_data(backend.device, backend.vma_instance, vertexBufferPosition, mesh.vertices.data(),
                       mesh.vertices.size() * sizeof(mesh.vertices[0]));
    upload_buffer_data(backend.device, backend.vma_instance, vertexBufferNormal, mesh.normals.data(),
                       mesh.normals.size() * sizeof(mesh.normals[0]));
    upload_buffer_data(backend.device, backend.vma_instance, vertexBufferUV, mesh.uvs.data(),
                       mesh.uvs.size() * sizeof(mesh.uvs[0]));
    upload_buffer_data(backend.device, backend.vma_instance, staticIndexBuffer, mesh.indexes.data(),
                       mesh.indexes.size() * sizeof(mesh.indexes[0]));

    CullResources cull_resources = create_culling_resources(root, backend);

    CullPipelineInfo           cullPipe = create_cull_pipeline(root, backend);
    CompactionPrepPipelineInfo compactPrepPipe = create_compaction_prep_pipeline(root, backend);
    CompactionPipelineInfo     compactionPipe = create_compaction_pipeline(root, backend);
    {
        const u32 pass_count = 2;
        for (u32 pass_index = 0; pass_index < pass_count; pass_index++)
        {
            // FIXME use pass index
            CullPassResources& cull_pass_resources = cull_resources.passes.emplace_back();
            cull_pass_resources.cull_descriptor_set = create_culling_descriptor_sets(
                root, backend, cull_resources, cullPipe.descSetLayout, resources.descriptorPool, staticIndexBuffer,
                vertexBufferPosition, pass_index);

            cull_pass_resources.compact_prep_descriptor_set = create_culling_compact_prep_descriptor_sets(
                root, backend, cull_resources, compactPrepPipe.descSetLayout, resources.descriptorPool, pass_index);

            cull_pass_resources.compact_descriptor_set = create_culling_compact_descriptor_sets(
                root, backend, cull_resources, compactionPipe.descSetLayout, resources.descriptorPool, pass_index);
        }
    }

    // Shadow Pass
    ShadowMapResources    shadowMapResources = create_shadow_map_resources(root, backend);
    ShadowMapPipelineInfo shadowMapPipe =
        create_shadow_map_pipeline(root, backend, shadowMapResources.shadowMapPass, ShadowMapResolution);
    VkDescriptorSet shadowMapPassDescriptorSet = VK_NULL_HANDLE;
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              resources.descriptorPool, 1,
                                                              &shadowMapPipe.descSetLayout};

        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &shadowMapPassDescriptorSet)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}",
                  static_cast<void*>(shadowMapPassDescriptorSet));

        const VkDescriptorBufferInfo shadowMapDescPassParams =
            default_descriptor_buffer_info(shadowMapResources.shadowMapPassConstantBuffer);
        const VkDescriptorBufferInfo shadowMapDescInstanceParams =
            default_descriptor_buffer_info(shadowMapResources.shadowMapInstanceConstantBuffer);

        std::array<VkWriteDescriptorSet, 2> shadowMapPassDescriptorSetWrites = {
            create_buffer_descriptor_write(shadowMapPassDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           &shadowMapDescPassParams),
            create_buffer_descriptor_write(shadowMapPassDescriptorSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &shadowMapDescInstanceParams),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(shadowMapPassDescriptorSetWrites.size()),
                               shadowMapPassDescriptorSetWrites.data(), 0, nullptr);
    }

    // Main Pass
    VkRenderPass mainRenderPass = create_main_pass(root, backend, main_pass_resources.depthBuffer.properties);
    std::vector<VkFramebuffer> framebuffers;
    create_framebuffers(root, backend, mainRenderPass, main_pass_resources.depthBufferView, framebuffers);

    BlitPipelineInfo mainPipe = create_main_pipeline(root, backend, mainRenderPass);
    VkDescriptorSet  pixelConstantsDescriptorSet = VK_NULL_HANDLE;
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              resources.descriptorPool, 1, &mainPipe.descSetLayout};

        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &pixelConstantsDescriptorSet)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}",
                  static_cast<void*>(pixelConstantsDescriptorSet));

        const VkDescriptorBufferInfo drawDescPassParams =
            default_descriptor_buffer_info(main_pass_resources.drawPassConstantBuffer);
        const VkDescriptorBufferInfo drawDescInstanceParams =
            default_descriptor_buffer_info(main_pass_resources.drawInstanceConstantBuffer);
        const VkDescriptorImageInfo drawDescShadowMapTexture = {VK_NULL_HANDLE, shadowMapResources.shadowMapView,
                                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        const VkDescriptorImageInfo drawDescShadowMapSampler = {shadowMapResources.shadowMapSampler, VK_NULL_HANDLE,
                                                                VK_IMAGE_LAYOUT_UNDEFINED};

        std::array<VkWriteDescriptorSet, 4> drawPassDescriptorSetWrites = {
            create_buffer_descriptor_write(pixelConstantsDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           &drawDescPassParams),
            create_buffer_descriptor_write(pixelConstantsDescriptorSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &drawDescInstanceParams),
            create_image_descriptor_write(pixelConstantsDescriptorSet, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                          &drawDescShadowMapTexture),
            create_image_descriptor_write(pixelConstantsDescriptorSet, 3, VK_DESCRIPTOR_TYPE_SAMPLER,
                                          &drawDescShadowMapSampler),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(drawPassDescriptorSetWrites.size()),
                               drawPassDescriptorSetWrites.data(), 0, nullptr);
    }

    // Create fence
    VkFenceCreateInfo fenceInfo = {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
        0 // Not signaled by default
    };

    VkFence drawFence = VK_NULL_HANDLE;
    vkCreateFence(backend.device, &fenceInfo, nullptr, &drawFence);
    log_debug(root, "vulkan: created fence with handle: {}", static_cast<void*>(drawFence));

    IWindow*                   window = root.renderer->window;
    std::vector<Window::Event> events;

    log_info(root, "window: map window");
    window->map();

    const int MaxFrameCount = 0;

    bool mustTransitionSwapchain = true;
    bool saveMyLaptop = true;
    bool shouldExit = false;
    int  frameIndex = 0;
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

    while (!shouldExit)
    {
        const auto currentTime = std::chrono::system_clock::now();
        {
            REAPER_PROFILE_SCOPE("Vulkan", MP_YELLOW);
            log_info(root, "window: pump events");
            window->pumpEvents(events);
            if (!events.empty())
            {
                // Should be front but whatever
                Window::Event event = events.back();

                if (event.type == Window::EventType::Resize)
                {
                    const u32 width = event.message.resize.width;
                    const u32 height = event.message.resize.height;
                    log_debug(root, "window: resize event, width = {}, height = {}", width, height);

                    {
                        vkQueueWaitIdle(backend.deviceInfo.presentQueue);
                        destroy_framebuffers(root, backend.device, framebuffers);

                        resize_vulkan_wm_swapchain(root, backend, backend.presentInfo, {width, height});

                        const glm::uvec2 new_swapchain_extent(backend.presentInfo.surfaceExtent.width,
                                                              backend.presentInfo.surfaceExtent.height);
                        resize_main_pass_depth_buffer(root, backend, main_pass_resources, new_swapchain_extent);

                        create_framebuffers(root, backend, mainRenderPass, main_pass_resources.depthBufferView,
                                            framebuffers);
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

                events.pop_back();
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
            VkRect2D                    blitPassRect = {{0, 0}, backbufferExtent};

            VkRenderPassBeginInfo blitRenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                             nullptr,
                                                             mainRenderPass,
                                                             framebuffers[imageIndex],
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

            // FIXME do partial copies
            upload_buffer_data(backend.device, backend.vma_instance, main_pass_resources.drawPassConstantBuffer,
                               &prepared.draw_pass_params, sizeof(DrawPassParams));
            upload_buffer_data(backend.device, backend.vma_instance, shadowMapResources.shadowMapPassConstantBuffer,
                               &prepared.shadow_pass_params, sizeof(ShadowMapPassParams));
            upload_buffer_data(backend.device, backend.vma_instance, main_pass_resources.drawInstanceConstantBuffer,
                               prepared.draw_instance_params.data(),
                               prepared.draw_instance_params.size() * sizeof(DrawInstanceParams));
            upload_buffer_data(backend.device, backend.vma_instance, shadowMapResources.shadowMapInstanceConstantBuffer,
                               prepared.shadow_instance_params.data(),
                               prepared.shadow_instance_params.size() * sizeof(ShadowMapInstanceParams));

            culling_prepare_buffers(cull_options, backend, prepared, cull_resources);

            VkCommandBufferBeginInfo cmdBufferBeginInfo = {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                0,      // Not caring yet
                nullptr // No inheritance yet
            };

            Assert(vkBeginCommandBuffer(resources.gfxCmdBuffer, &cmdBufferBeginInfo) == VK_SUCCESS);

            if (mustTransitionSwapchain)
            {
                cmd_transition_swapchain_layout(backend, resources.gfxCmdBuffer);
                mustTransitionSwapchain = false;
            }

            // Culling
            {
                const u32 pass_count = prepared.cull_passes.size();

                if (!cull_options.freeze_culling)
                {
                    vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipe.pipeline);

                    for (u32 pass_index = 0; pass_index < pass_count; pass_index++)
                    {
                        const CullPassResources& cull_pass_resources = cull_resources.passes[pass_index];
                        const CullPassData&      cull_pass = prepared.cull_passes[pass_index];

                        vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                cullPipe.pipelineLayout, 0, 1, &cull_pass_resources.cull_descriptor_set,
                                                0, nullptr);

                        for (const CullCmd& command : cull_pass.cull_cmds)
                        {
                            vkCmdPushConstants(resources.gfxCmdBuffer, cullPipe.pipelineLayout,
                                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(command.push_constants),
                                               &command.push_constants);

                            vkCmdDispatch(resources.gfxCmdBuffer,
                                          div_round_up(command.push_constants.triangleCount, ComputeCullingGroupSize),
                                          command.instanceCount, 1);
                        }
                    }
                }

                cmd_insert_compute_to_compute_barrier(resources.gfxCmdBuffer);

                // Compaction prepare pass
                {
                    vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compactPrepPipe.pipeline);

                    for (u32 pass_index = 0; pass_index < pass_count; pass_index++)
                    {
                        const CullPassResources& cull_pass_resources = cull_resources.passes[pass_index];

                        vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                compactPrepPipe.pipelineLayout, 0, 1,
                                                &cull_pass_resources.compact_prep_descriptor_set, 0, nullptr);

                        vkCmdDispatch(resources.gfxCmdBuffer, 1, 1, 1);
                    }
                }

                cmd_insert_compute_to_compute_barrier(resources.gfxCmdBuffer);

                // Compaction pass
                {
                    vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compactionPipe.pipeline);

                    for (u32 pass_index = 0; pass_index < pass_count; pass_index++)
                    {
                        const CullPassResources& cull_pass_resources = cull_resources.passes[pass_index];

                        vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                compactionPipe.pipelineLayout, 0, 1,
                                                &cull_pass_resources.compact_descriptor_set, 0, nullptr);

                        vkCmdDispatchIndirect(resources.gfxCmdBuffer,
                                              cull_resources.compactionIndirectDispatchBuffer.buffer, 0);
                    }
                }
            }

            {
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
            {
                const u32 pass_index = 1; // FIXME

                VkClearValue clearValue = VkClearDepthStencil(UseReverseZ ? 0.f : 1.f, 0); // FIXME
                VkRect2D     passRect = {
                    {0, 0},
                    {shadowMapResources.shadowMap.properties.width, shadowMapResources.shadowMap.properties.height}};

                VkRenderPassBeginInfo shadowMapRenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                                      nullptr,
                                                                      shadowMapResources.shadowMapPass,
                                                                      shadowMapResources.shadowMapFramebuffer,
                                                                      passRect,
                                                                      1,
                                                                      &clearValue};

                vkCmdBeginRenderPass(resources.gfxCmdBuffer, &shadowMapRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMapPipe.pipeline);

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
                                        shadowMapPipe.pipelineLayout, 0, 1, &shadowMapPassDescriptorSet, 0, nullptr);

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

            {
                VkImageMemoryBarrier shadowMapImageBarrierInfo = {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    nullptr,
                    0,
                    VK_ACCESS_MEMORY_READ_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    backend.physicalDeviceInfo.graphicsQueueIndex,
                    backend.physicalDeviceInfo.graphicsQueueIndex,
                    shadowMapResources.shadowMap.handle,
                    {VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

                vkCmdPipelineBarrier(resources.gfxCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                     &shadowMapImageBarrierInfo);
            }

            // Draw pass
            {
                const u32 pass_index = 0; // FIXME

                vkCmdBeginRenderPass(resources.gfxCmdBuffer, &blitRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipe.pipeline);

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
                                        mainPipe.pipelineLayout, 0, 1, &pixelConstantsDescriptorSet, 0, nullptr);

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

            Assert(vkEndCommandBuffer(resources.gfxCmdBuffer) == VK_SUCCESS);
            // Stop recording

            VkPipelineStageFlags blitWaitDstMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

            VkSubmitInfo blitSubmitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                           nullptr,
                                           1,
                                           &backend.presentInfo.imageAvailableSemaphore,
                                           &blitWaitDstMask,
                                           1,
                                           &resources.gfxCmdBuffer,
                                           1,
                                           &backend.presentInfo.renderingFinishedSemaphore};

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
                std::this_thread::sleep_for(std::chrono::milliseconds(60));

            frameIndex++;
            if (frameIndex == MaxFrameCount)
                shouldExit = true;
        }
        MicroProfileFlip(nullptr);

        lastFrameStart = currentTime;
    }

    vkQueueWaitIdle(backend.deviceInfo.presentQueue);

    log_info(root, "window: unmap window");
    window->unmap();

    vkDestroyFence(backend.device, drawFence, nullptr);

    vkDestroyPipeline(backend.device, mainPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, mainPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, mainPipe.descSetLayout, nullptr);

    vkDestroyPipeline(backend.device, shadowMapPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, shadowMapPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, shadowMapPipe.descSetLayout, nullptr);

    vkDestroyPipeline(backend.device, cullPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, cullPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, cullPipe.descSetLayout, nullptr);

    vkDestroyPipeline(backend.device, compactPrepPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, compactPrepPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, compactPrepPipe.descSetLayout, nullptr);

    vkDestroyPipeline(backend.device, compactionPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, compactionPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, compactionPipe.descSetLayout, nullptr);

    destroy_framebuffers(root, backend.device, framebuffers);

    vkDestroyRenderPass(backend.device, mainRenderPass, nullptr);

    destroy_shadow_map_resources(backend, shadowMapResources);

    destroy_culling_resources(backend, cull_resources);

    vmaDestroyBuffer(backend.vma_instance, staticIndexBuffer.buffer, staticIndexBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, vertexBufferPosition.buffer, vertexBufferPosition.allocation);
    vmaDestroyBuffer(backend.vma_instance, vertexBufferNormal.buffer, vertexBufferNormal.allocation);
    vmaDestroyBuffer(backend.vma_instance, vertexBufferUV.buffer, vertexBufferUV.allocation);

    destroy_main_pass_resources(backend, main_pass_resources);
}
} // namespace Reaper
