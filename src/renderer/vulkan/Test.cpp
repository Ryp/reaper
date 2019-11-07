////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Test.h"

#include "Buffer.h"
#include "Image.h"
#include "Memory.h"
#include "Shader.h"
#include "Swapchain.h"
#include "SwapchainRendererBase.h"

#include "api/Vulkan.h"
#include "api/VulkanStringConversion.h"

#include "allocator/GPUStackAllocator.h"

#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/window/Event.h"
#include "renderer/window/Window.h"

#include "mesh/Mesh.h"
#include "mesh/ModelLoader.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/BitTricks.h"
#include "core/Profile.h"
#include "core/memory/Allocator.h"

#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <thread>

namespace Reaper
{
namespace
{
    struct BlitPipelineInfo
    {
        VkPipeline            pipeline;
        VkPipelineLayout      pipelineLayout;
        VkDescriptorSetLayout descSetLayout;
    };

    struct GlobalResources
    {
        ImageInfo         image;
        VkImageView       imageView;
        GPUStackAllocator mainAllocator;
        VkDescriptorPool  descriptorPool;
        VkCommandBuffer   gfxCmdBuffer;
    };

    struct UniformBufferObject
    {
        glm::mat4 model;
        glm::mat4 viewProj;
        glm::mat3 modelViewInvT;
        float     _pad0;
        float     _pad1;
        float     _pad2;
        float     timeMs;
    };

    void debug_memory_heap_properties(ReaperRoot& root, const VulkanBackend& backend, uint32_t memoryTypeIndex)
    {
        const VkMemoryType& memoryType = backend.physicalDeviceInfo.memory.memoryTypes[memoryTypeIndex];

        log_debug(root, "vulkan: selecting memory heap {} with these properties:", memoryType.heapIndex);
        for (u32 i = 0; i < sizeof(VkMemoryPropertyFlags) * 8; i++)
        {
            const VkMemoryPropertyFlags flag = 1 << i;

            if (memoryType.propertyFlags & flag)
                log_debug(root, "- {}", GetMemoryPropertyFlagBitToString(flag));
        }
    }

    void vulkan_cmd_transition_swapchain_layout(VulkanBackend& backend, VkCommandBuffer commandBuffer)
    {
        for (u32 swapchainImageIndex = 0; swapchainImageIndex < static_cast<u32>(backend.presentInfo.images.size());
             swapchainImageIndex++)
        {
            VkImageMemoryBarrier swapchainImageBarrierInfo = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                0, // VK_ACCESS_SHADER_WRITE_BIT ?
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

    BlitPipelineInfo vulkan_create_blit_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        VkShaderModule        blitShaderFS = VK_NULL_HANDLE;
        VkShaderModule        blitShaderVS = VK_NULL_HANDLE;
        const char*           fileNameVS = "./build/spv/mesh_transformed_shaded.vert.spv";
        const char*           fileNameFS = "./build/spv/mesh_transformed_shaded.frag.spv";
        const char*           entryPoint = "main";
        VkSpecializationInfo* specialization = nullptr;

        vulkan_create_shader_module(blitShaderFS, backend.device, fileNameFS);
        vulkan_create_shader_module(blitShaderVS, backend.device, fileNameVS);

        const u32                             meshVertexStride = sizeof(float) * 3; // Position only
        const VkVertexInputBindingDescription vertexInfoShaderBinding = {
            0,                          // binding
            meshVertexStride,           // stride
            VK_VERTEX_INPUT_RATE_VERTEX // input rate
        };

        std::vector<VkVertexInputAttributeDescription> vertexAttributes = {
            {
                0,                          // location
                0,                          // binding
                VK_FORMAT_R32G32B32_SFLOAT, // format
                0                           // offset
            },
            {
                1,                          // location
                0,                          // binding
                VK_FORMAT_R32G32B32_SFLOAT, // format
                0                           // offset
            },
            {
                2,                       // location
                0,                       // binding
                VK_FORMAT_R32G32_SFLOAT, // format
                0                        // offset
            },
        };

        std::vector<VkPipelineShaderStageCreateInfo> blitShaderStages = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, blitShaderVS,
             entryPoint, specialization},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
             blitShaderFS, entryPoint, specialization}};

        VkPipelineVertexInputStateCreateInfo blitVertexInputStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            1,
            &vertexInfoShaderBinding,
            static_cast<u32>(vertexAttributes.size()),
            vertexAttributes.data()};

        VkPipelineInputAssemblyStateCreateInfo blitInputAssemblyInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, VK_FLAGS_NONE,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};

        const VkExtent2D backbufferExtent = backend.presentInfo.surfaceExtent;

        VkViewport blitViewport = {
            0.0f, 0.0f, static_cast<float>(backbufferExtent.width), static_cast<float>(backbufferExtent.height),
            0.0f, 1.0f};

        VkRect2D blitScissors = {{0, 0}, backbufferExtent};

        VkPipelineViewportStateCreateInfo blitViewportStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            1,
            &blitViewport,
            1,
            &blitScissors};

        VkPipelineRasterizationStateCreateInfo blitRasterStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            VK_FALSE,
            VK_FALSE,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_CLOCKWISE, // FIXME
            VK_FALSE,
            0.0f,
            0.0f,
            0.0f,
            1.0f};

        VkPipelineMultisampleStateCreateInfo blitMSStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FALSE,
            1.0f,
            nullptr,
            VK_FALSE,
            VK_FALSE};

        VkPipelineColorBlendAttachmentState blitBlendAttachmentState = {
            VK_FALSE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

        VkPipelineColorBlendStateCreateInfo blitBlendStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            1,
            &blitBlendAttachmentState,
            {0.0f, 0.0f, 0.0f, 0.0f}};

        VkDescriptorSetLayoutBinding descriptorSetLayoutBindingCB = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                                                     VK_SHADER_STAGE_VERTEX_BIT, nullptr};

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                                   nullptr, 0, 1, &descriptorSetLayoutBindingCB};

        VkDescriptorSetLayout descriptorSetLayoutCB = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayoutCB)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayoutCB));

        VkPipelineLayoutCreateInfo blitPipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                             nullptr,
                                                             VK_FLAGS_NONE,
                                                             1,
                                                             &descriptorSetLayoutCB,
                                                             0,
                                                             nullptr};

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Assert(vkCreatePipelineLayout(backend.device, &blitPipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);
        log_debug(root, "vulkan: created blit pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

        VkPipelineCache cache = VK_NULL_HANDLE;

        const VkDynamicState             dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo blitDynamicState = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dynamicStates,
        };

        VkGraphicsPipelineCreateInfo blitPipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                               nullptr,
                                                               VK_FLAGS_NONE,
                                                               static_cast<u32>(blitShaderStages.size()),
                                                               blitShaderStages.data(),
                                                               &blitVertexInputStateInfo,
                                                               &blitInputAssemblyInfo,
                                                               nullptr,
                                                               &blitViewportStateInfo, // Dynamic
                                                               &blitRasterStateInfo,
                                                               &blitMSStateInfo,
                                                               nullptr,
                                                               &blitBlendStateInfo,
                                                               &blitDynamicState,
                                                               pipelineLayout,
                                                               backend.presentInfo.renderPass,
                                                               0,
                                                               VK_NULL_HANDLE,
                                                               -1};

        VkPipeline pipeline = VK_NULL_HANDLE;
        Assert(vkCreateGraphicsPipelines(backend.device, cache, 1, &blitPipelineCreateInfo, nullptr, &pipeline)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created blit pipeline with handle: {}", static_cast<void*>(pipeline));

        Assert(backend.physicalDeviceInfo.graphicsQueueIndex == backend.physicalDeviceInfo.presentQueueIndex);

        vkDestroyShaderModule(backend.device, blitShaderVS, nullptr);
        vkDestroyShaderModule(backend.device, blitShaderFS, nullptr);

        return BlitPipelineInfo{pipeline, pipelineLayout, descriptorSetLayoutCB};
    }

    void update_transform_constant_buffer(UniformBufferObject& ubo, float timeMs, float aspectRatio)
    {
        const glm::vec3 object_position_ws = glm::vec3(0.0f, 0.0f, 0.0f);
        const glm::mat3 model = glm::translate(glm::mat4(1.0f), object_position_ws);
        ubo.model = model;

        const glm::vec3 camera_position_ws = glm::vec3(5.0f, 5.0f, 5.0f);
        const glm::mat4 view = glm::lookAt(camera_position_ws, object_position_ws, glm::vec3(0, -1, 0));

        const float near_plane_distance = 0.1f;
        const float far_plane_distance = 100.f;
        const float fov_radian = glm::pi<float>() * 0.25f;
        ubo.viewProj = glm::perspective(fov_radian, aspectRatio, near_plane_distance, far_plane_distance) * view;

        const glm::mat3 modelView = glm::mat3(view) * glm::mat3(model);
        ubo.modelViewInvT = glm::transpose(glm::inverse(modelView));

        ubo.timeMs = timeMs;
    }

    void vulkan_test_compute(ReaperRoot& root, VulkanBackend& backend, GlobalResources& resources)
    {
        VkDescriptorSetLayoutBinding descriptorSetLayoutBindingTest = {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                                                       VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                                   nullptr, 0, 1, &descriptorSetLayoutBindingTest};

        VkDescriptorSetLayout descriptorSetLayoutOne = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayoutOne)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayoutOne));

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                         nullptr,
                                                         VK_FLAGS_NONE,
                                                         1,
                                                         &descriptorSetLayoutOne,
                                                         0,
                                                         nullptr};

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Assert(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

        log_debug(root, "vulkan: created pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

        // TODO make this a real shader
        VkShaderModule        computeShader = VK_NULL_HANDLE;
        const char*           fileName = "./build/spv/test.comp.spv";
        const char*           entryPoint = "main";
        VkSpecializationInfo* specialization = nullptr;

        vulkan_create_shader_module(computeShader, backend.device, fileName);

        VkPipelineShaderStageCreateInfo shaderStage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       nullptr,
                                                       0,
                                                       VK_SHADER_STAGE_COMPUTE_BIT,
                                                       computeShader,
                                                       entryPoint,
                                                       specialization};

        VkComputePipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                          nullptr,
                                                          0,
                                                          shaderStage,
                                                          pipelineLayout,
                                                          VK_NULL_HANDLE, // do not care about pipeline derivatives
                                                          0};

        VkPipeline      pipeline = VK_NULL_HANDLE;
        VkPipelineCache cache = VK_NULL_HANDLE;

        Assert(vkCreateComputePipelines(backend.device, cache, 1, &pipelineCreateInfo, nullptr, &pipeline)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created pipeline with handle: {}", static_cast<void*>(pipeline));

        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              resources.descriptorPool, 1, &descriptorSetLayoutOne};

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptorSet) == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptorSet));

        VkDescriptorImageInfo descriptorSetImageInfo = {VK_NULL_HANDLE, resources.imageView, VK_IMAGE_LAYOUT_GENERAL};

        VkWriteDescriptorSet descriptorSetWrite = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            descriptorSet,
            0, // 0th binding
            0, // not an array
            1, // not an array
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            &descriptorSetImageInfo,
            nullptr,
            nullptr,
        };

        vkUpdateDescriptorSets(backend.device, 1, &descriptorSetWrite, 0, nullptr);

        VkCommandBufferBeginInfo cmdBufferBeginInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
            0,      // Not caring yet
            nullptr // No inheritance yet
        };

        Assert(vkBeginCommandBuffer(resources.gfxCmdBuffer, &cmdBufferBeginInfo) == VK_SUCCESS);

        VkImageMemoryBarrier imageBarrierInfo = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            0, // VK_ACCESS_SHADER_WRITE_BIT ?
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            backend.physicalDeviceInfo.graphicsQueueIndex,
            backend.physicalDeviceInfo.graphicsQueueIndex,
            resources.image.handle,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

        vkCmdPipelineBarrier(resources.gfxCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrierInfo);

        vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
                                &descriptorSet, 0, nullptr);
        vkCmdDispatch(resources.gfxCmdBuffer, 1, 1, 1);

        Assert(vkEndCommandBuffer(resources.gfxCmdBuffer) == VK_SUCCESS);

        VkFenceCreateInfo fenceInfo = {
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
            0 // Not signaled by default
        };

        VkFence drawFence = VK_NULL_HANDLE;
        vkCreateFence(backend.device, &fenceInfo, nullptr, &drawFence);
        log_debug(root, "vulkan: created fence with handle: {}", static_cast<void*>(drawFence));

        VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0,      nullptr, nullptr, 1,
                                   &resources.gfxCmdBuffer,       0,       nullptr};

        log_debug(root, "vulkan: queue submit");
        Assert(vkQueueSubmit(backend.deviceInfo.graphicsQueue, 1, &submitInfo, drawFence) == VK_SUCCESS);

        log_debug(root, "vulkan: wait for fence");
        {
            const u64 waitTimeoutUs = 1000000;
            VkResult  waitResult = vkWaitForFences(backend.device, 1, &drawFence, VK_TRUE, waitTimeoutUs);
            Assert(waitResult == VK_SUCCESS || waitResult == VK_TIMEOUT);
            Assert(waitResult != VK_TIMEOUT); // TODO Handle timeout correctly

            Assert(vkGetFenceStatus(backend.device, drawFence) == VK_SUCCESS);

            Assert(vkResetFences(backend.device, 1, &drawFence) == VK_SUCCESS);
        }

        // cleanup
        vkDestroyFence(backend.device, drawFence, nullptr);

        vkDestroyPipeline(backend.device, pipeline, nullptr);

        vkDestroyShaderModule(backend.device, computeShader, nullptr);
        vkDestroyPipelineLayout(backend.device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(backend.device, descriptorSetLayoutOne, nullptr);
    }

    void vulkan_test_graphics(ReaperRoot& root, VulkanBackend& backend, GlobalResources& resources)
    {
        // Read mesh file
        std::ifstream modelFile("res/model/suzanne.obj");
        const Mesh    mesh = ModelLoader::loadOBJ(modelFile);

        // Create vk buffers
        BufferInfo constantBuffer =
            create_constant_buffer(root, backend.device, sizeof(UniformBufferObject), resources.mainAllocator);

        BufferInfo vertexBufferPosition = create_vertex_buffer(root, backend.device, mesh.vertices.size(),
                                                               sizeof(mesh.vertices[0]), resources.mainAllocator);
        BufferInfo vertexBufferNormal = create_vertex_buffer(root, backend.device, mesh.normals.size(),
                                                             sizeof(mesh.normals[0]), resources.mainAllocator);
        BufferInfo vertexBufferUV =
            create_vertex_buffer(root, backend.device, mesh.uvs.size(), sizeof(mesh.uvs[0]), resources.mainAllocator);
        BufferInfo indexBuffer = create_index_buffer(root, backend.device, mesh.indexes.size(), sizeof(mesh.indexes[0]),
                                                     resources.mainAllocator);

        upload_buffer_data(backend.device, vertexBufferPosition, mesh.vertices.data(),
                           mesh.vertices.size() * sizeof(mesh.vertices[0]));
        upload_buffer_data(backend.device, vertexBufferNormal, mesh.normals.data(),
                           mesh.normals.size() * sizeof(mesh.normals[0]));
        upload_buffer_data(backend.device, vertexBufferUV, mesh.uvs.data(), mesh.uvs.size() * sizeof(mesh.uvs[0]));
        upload_buffer_data(backend.device, indexBuffer, mesh.indexes.data(),
                           mesh.indexes.size() * sizeof(mesh.indexes[0]));

        VkFenceCreateInfo fenceInfo = {
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
            0 // Not signaled by default
        };

        BlitPipelineInfo blitPipe = vulkan_create_blit_pipeline(root, backend);
        VkDescriptorSet  descriptorSet = VK_NULL_HANDLE;
        {
            VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                                  nullptr, resources.descriptorPool, 1,
                                                                  &blitPipe.descSetLayout};

            Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptorSet) == VK_SUCCESS);
            log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptorSet));

            VkDescriptorBufferInfo descriptorSetCB = {constantBuffer.buffer, 0, constantBuffer.alloc.size};

            VkWriteDescriptorSet descriptorSetWrite = {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                nullptr,
                descriptorSet,
                0, // 0th binding
                0, // not an array
                1, // not an array
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                nullptr,
                &descriptorSetCB,
                nullptr,
            };

            vkUpdateDescriptorSets(backend.device, 1, &descriptorSetWrite, 0, nullptr);
        }

        VkFence drawFence = VK_NULL_HANDLE;
        vkCreateFence(backend.device, &fenceInfo, nullptr, &drawFence);
        log_debug(root, "vulkan: created fence with handle: {}", static_cast<void*>(drawFence));

        IWindow*                   window = root.renderer->window;
        std::vector<Window::Event> events;

        log_info(root, "window: map window");
        window->map();

        UniformBufferObject ubo = {};

        const int MaxFrameCount = 0;

        bool mustTransitionSwapchain = true;
        bool shouldExit = false;
        int  frameIndex = 0;

        const auto startTime = std::chrono::system_clock::now();

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

                        resize_vulkan_wm_swapchain(root, backend, backend.presentInfo, {width, height});
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

                // VkImage currentBackBuffer = backend.presentInfo.images[imageIndex];

                VkClearValue     clearValue = {{1.0f, 0.8f, 0.4f, 0.0f}};
                const VkExtent2D backbufferExtent = backend.presentInfo.surfaceExtent;
                VkRect2D         blitPassRect = {{0, 0}, backbufferExtent};

                VkRenderPassBeginInfo blitRenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                                 nullptr,
                                                                 backend.presentInfo.renderPass,
                                                                 backend.presentInfo.framebuffers[imageIndex],
                                                                 blitPassRect,
                                                                 1,
                                                                 &clearValue};

                // Only wait for a previous frame fence
                if (frameIndex > 0)
                {
                    log_debug(root, "vulkan: wait for fence");
                    VkResult waitResult;
                    {
                        REAPER_PROFILE_SCOPE("Vulkan", MP_ORANGE);
                        const u64 waitTimeoutUs = 10000000;
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

                const auto  timeSecs = std::chrono::duration_cast<std::chrono::milliseconds>(startTime - currentTime);
                const float timeMs = static_cast<float>(timeSecs.count()) * 0.001f;
                const float aspectRatio =
                    static_cast<float>(backbufferExtent.width) / static_cast<float>(backbufferExtent.height);
                update_transform_constant_buffer(ubo, timeMs, aspectRatio);
                upload_buffer_data(backend.device, constantBuffer, &ubo, sizeof(UniformBufferObject));

                VkCommandBufferBeginInfo cmdBufferBeginInfo = {
                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                    0,      // Not caring yet
                    nullptr // No inheritance yet
                };

                Assert(vkBeginCommandBuffer(resources.gfxCmdBuffer, &cmdBufferBeginInfo) == VK_SUCCESS);

                if (mustTransitionSwapchain)
                {
                    vulkan_cmd_transition_swapchain_layout(backend, resources.gfxCmdBuffer);
                    mustTransitionSwapchain = false;
                }

                vkCmdBeginRenderPass(resources.gfxCmdBuffer, &blitRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blitPipe.pipeline);

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
                vkCmdBindIndexBuffer(resources.gfxCmdBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdBindVertexBuffers(resources.gfxCmdBuffer, 0, static_cast<u32>(vertexBuffers.size()),
                                       vertexBuffers.data(), vertexBufferOffsets.data());
                vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        blitPipe.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

                vkCmdDrawIndexed(resources.gfxCmdBuffer, static_cast<u32>(mesh.indexes.size()), 1, 0, 0, 0);

                vkCmdEndRenderPass(resources.gfxCmdBuffer);

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

                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                frameIndex++;
                if (frameIndex == MaxFrameCount)
                    shouldExit = true;
            }
            MicroProfileFlip(nullptr);
        }

        vkQueueWaitIdle(backend.deviceInfo.presentQueue);

        log_info(root, "window: unmap window");
        window->unmap();

        vkDestroyFence(backend.device, drawFence, nullptr);

        vkDestroyPipeline(backend.device, blitPipe.pipeline, nullptr);
        vkDestroyPipelineLayout(backend.device, blitPipe.pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(backend.device, blitPipe.descSetLayout, nullptr);

        vkDestroyBuffer(backend.device, constantBuffer.buffer, nullptr);
        vkDestroyBuffer(backend.device, vertexBufferPosition.buffer, nullptr);
        vkDestroyBuffer(backend.device, vertexBufferNormal.buffer, nullptr);
        vkDestroyBuffer(backend.device, vertexBufferUV.buffer, nullptr);
        vkDestroyBuffer(backend.device, indexBuffer.buffer, nullptr);
    }
} // namespace

void vulkan_test(ReaperRoot& root, VulkanBackend& backend)
{
    log_info(root, "test ////////////////////////////////////////");

    // Create main memory pool
    VkDeviceMemory mainMemPoom = VK_NULL_HANDLE;
    const u32      mainMemPoolIndex = 1; // FIXME
    const u64      mainMemPoolSize = 10_MiB;

    const VkMemoryAllocateInfo mainMemPoolAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, mainMemPoolSize,
                                                       mainMemPoolIndex};

    Assert(vkAllocateMemory(backend.device, &mainMemPoolAllocInfo, nullptr, &mainMemPoom) == VK_SUCCESS);
    log_debug(root, "vulkan: allocated memory: handle = {}, size = {}, index = {}", static_cast<void*>(mainMemPoom),
              mainMemPoolSize, mainMemPoolIndex);

    debug_memory_heap_properties(root, backend, mainMemPoolIndex);

    GPUStackAllocator mainAllocator(mainMemPoom, (1 << mainMemPoolIndex), mainMemPoolSize);

    // Create a texture
    GPUTextureProperties properties = DefaultGPUTextureProperties(800, 600, PixelFormat::R16G16B16A16_UNORM);
    properties.usageFlags = GPUTextureUsage::Sampled | GPUTextureUsage::Storage | GPUTextureUsage::ColorAttachment;

    log_debug(root, "vulkan: creating new image: extent = {}x{}x{}, format = {}", properties.width, properties.height,
              properties.depth, static_cast<u32>(properties.format)); // FIXME print format
    log_debug(root, "- mips = {}, layers = {}, samples = {}", properties.mipCount, properties.layerCount,
              properties.sampleCount);

    const ImageInfo image = CreateVulkanImage(backend.device, properties, mainAllocator);
    log_debug(root, "vulkan: created image with handle: {}", static_cast<void*>(image.handle));

    VkImageView imageView = create_default_image_view(backend.device, image);
    log_debug(root, "vulkan: created image view with handle: {}", static_cast<void*>(imageView));

    // Create descriptor pool
    constexpr u32                     MaxDescriptorSets = 100; // FIXME
    std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {{VK_DESCRIPTOR_TYPE_SAMPLER, 10},
                                                             {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10},
                                                             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
                                                             {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10}};

    VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                           nullptr,
                                           0, // No special alloc flags
                                           MaxDescriptorSets,
                                           static_cast<uint32_t>(descriptorPoolSizes.size()),
                                           descriptorPoolSizes.data()};

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    Assert(vkCreateDescriptorPool(backend.device, &poolInfo, nullptr, &descriptorPool) == VK_SUCCESS);
    log_debug(root, "vulkan: created descriptor pool with handle: {}", static_cast<void*>(descriptorPool));

    // Create command buffer
    VkCommandPool           graphicsCommandPool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                              backend.physicalDeviceInfo.graphicsQueueIndex};

    Assert(vkCreateCommandPool(backend.device, &poolCreateInfo, nullptr, &graphicsCommandPool) == VK_SUCCESS);
    log_debug(root, "vulkan: created command pool with handle: {}", static_cast<void*>(graphicsCommandPool));

    VkCommandBufferAllocateInfo cmdBufferAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
                                                      graphicsCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};

    VkCommandBuffer gfxCmdBuffer = VK_NULL_HANDLE;
    Assert(vkAllocateCommandBuffers(backend.device, &cmdBufferAllocInfo, &gfxCmdBuffer) == VK_SUCCESS);
    log_debug(root, "vulkan: created command buffer with handle: {}", static_cast<void*>(gfxCmdBuffer));

    {
        GlobalResources resources = {image, imageView, mainAllocator, descriptorPool, gfxCmdBuffer};

        vulkan_test_compute(root, backend, resources);
        vulkan_test_graphics(root, backend, resources);
    }

    // cleanup
    vkFreeCommandBuffers(backend.device, graphicsCommandPool, 1, &gfxCmdBuffer);
    vkDestroyCommandPool(backend.device, graphicsCommandPool, nullptr);

    vkDestroyDescriptorPool(backend.device, descriptorPool, nullptr);

    vkDestroyImageView(backend.device, imageView, nullptr);
    vkDestroyImage(backend.device, image.handle, nullptr);

    vkFreeMemory(backend.device, mainMemPoom, nullptr);

    log_info(root, "test ////////////////////////////////////////");
}
} // namespace Reaper
