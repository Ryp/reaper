////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TestGraphics.h"

#include "Culling.h"
#include "CullingConstants.h"
#include "ShadowConstants.h"
#include "ShadowMap.h"

#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/Debug.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Memory.h"
#include "renderer/vulkan/Shader.h"
#include "renderer/vulkan/Swapchain.h"
#include "renderer/vulkan/SwapchainRendererBase.h"
#include "renderer/vulkan/Test.h"
#include "renderer/vulkan/api/Vulkan.h"
#include "renderer/vulkan/api/VulkanStringConversion.h"

#include "renderer/GPUBufferProperties.h"
#include "renderer/PrepareBuckets.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/window/Event.h"
#include "renderer/window/Window.h"

#include "input/DS4.h"
#include "renderer/Camera.h"

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

    struct CompactionPrepPipelineInfo
    {
        VkPipeline            pipeline;
        VkPipelineLayout      pipelineLayout;
        VkDescriptorSetLayout descSetLayout;
    };

    CompactionPrepPipelineInfo vulkan_create_compaction_prep_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        std::array<VkDescriptorSetLayoutBinding, 3> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, VK_FLAGS_NONE, 1, &descriptorSetLayout, 0, nullptr};

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Assert(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

        log_debug(root, "vulkan: created pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

        VkShaderModule        computeShader = VK_NULL_HANDLE;
        const char*           fileName = "./build/shader/compaction_prepare_indirect_dispatch.comp.spv";
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

        vkDestroyShaderModule(backend.device, computeShader, nullptr);

        log_debug(root, "vulkan: created compute pipeline with handle: {}", static_cast<void*>(pipeline));

        return CompactionPrepPipelineInfo{pipeline, pipelineLayout, descriptorSetLayout};
    }

    struct CompactionPipelineInfo
    {
        VkPipeline            pipeline;
        VkPipelineLayout      pipelineLayout;
        VkDescriptorSetLayout descSetLayout;
    };

    CompactionPipelineInfo vulkan_create_compaction_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        std::array<VkDescriptorSetLayoutBinding, 4> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, VK_FLAGS_NONE, 1, &descriptorSetLayout, 0, nullptr};

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Assert(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

        log_debug(root, "vulkan: created pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

        VkShaderModule        computeShader = VK_NULL_HANDLE;
        const char*           fileName = "./build/shader/draw_command_compaction.comp.spv";
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

        vkDestroyShaderModule(backend.device, computeShader, nullptr);

        log_debug(root, "vulkan: created compute pipeline with handle: {}", static_cast<void*>(pipeline));

        return CompactionPipelineInfo{pipeline, pipelineLayout, descriptorSetLayout};
    }

    constexpr u32  ShadowInstanceCountMax = 512;
    constexpr u32  DrawInstanceCountMax = 512;
    constexpr bool UseReverseZ = true;

    struct BlitPipelineInfo
    {
        VkPipeline            pipeline;
        VkPipelineLayout      pipelineLayout;
        VkDescriptorSetLayout descSetLayout;
    };

    VkClearValue VkClearColor(const glm::fvec4& color)
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

    VkRenderPass create_main_raster_pass(ReaperRoot& /*root*/, VulkanBackend& backend,
                                         const GPUTextureProperties& depthProperties)
    {
        // FIXME build this at swapchain creation
        GPUTextureProperties swapchainProperties = depthProperties;
        swapchainProperties.usageFlags = GPUTextureUsage::Swapchain;
        swapchainProperties.miscFlags = 0;

        // Create a separate render pass for the offscreen rendering as it may differ from the one used for scene
        // rendering
        std::array<VkAttachmentDescription, 2> attachmentDescriptions = {};
        constexpr u32                          color_index = 0;
        constexpr u32                          depth_index = 1;

        // Color attachment
        attachmentDescriptions[color_index].format = backend.presentInfo.surfaceFormat.format; // FIXME
        attachmentDescriptions[color_index].samples = SampleCountToVulkan(swapchainProperties.sampleCount);
        attachmentDescriptions[color_index].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescriptions[color_index].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDescriptions[color_index].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescriptions[color_index].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescriptions[color_index].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachmentDescriptions[color_index].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Depth attachment
        attachmentDescriptions[depth_index].format = PixelFormatToVulkan(depthProperties.format);
        attachmentDescriptions[depth_index].samples = SampleCountToVulkan(depthProperties.sampleCount);
        attachmentDescriptions[depth_index].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescriptions[depth_index].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescriptions[depth_index].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescriptions[depth_index].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescriptions[depth_index].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescriptions[depth_index].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorReference = {color_index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthReference = {depth_index, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpassDescription = {};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &colorReference;
        subpassDescription.pDepthStencilAttachment = &depthReference;

        // Use subpass dependencies for layout transitions
        std::array<VkSubpassDependency, 2> dependencies;

        dependencies[color_index].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[color_index].dstSubpass = 0;
        dependencies[color_index].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[color_index].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[color_index].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[color_index].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[color_index].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[depth_index].srcSubpass = 0;
        dependencies[depth_index].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[depth_index].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[depth_index].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[depth_index].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[depth_index].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[depth_index].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Create the actual renderpass
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
        renderPassInfo.pAttachments = attachmentDescriptions.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassDescription;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        VkRenderPass renderPass = VK_NULL_HANDLE;
        Assert(vkCreateRenderPass(backend.device, &renderPassInfo, nullptr, &renderPass) == VK_SUCCESS);

        return renderPass;
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

    BlitPipelineInfo vulkan_create_blit_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass)
    {
        VkShaderModule        blitShaderFS = VK_NULL_HANDLE;
        VkShaderModule        blitShaderVS = VK_NULL_HANDLE;
        const char*           fileNameVS = "./build/shader/mesh_transformed_shaded.vert.spv";
        const char*           fileNameFS = "./build/shader/mesh_transformed_shaded.frag.spv";
        const char*           entryPoint = "main";
        VkSpecializationInfo* specialization = nullptr;

        vulkan_create_shader_module(blitShaderFS, backend.device, fileNameFS);
        vulkan_create_shader_module(blitShaderVS, backend.device, fileNameVS);

        std::vector<VkVertexInputBindingDescription> vertexInfoShaderBinding = {
            {
                0,                          // binding
                sizeof(hlsl_float3),        // stride
                VK_VERTEX_INPUT_RATE_VERTEX // input rate
            },
            {
                1,                          // binding
                sizeof(hlsl_float3),        // stride
                VK_VERTEX_INPUT_RATE_VERTEX // input rate
            },
            {
                2,                          // binding
                sizeof(hlsl_float2),        // stride
                VK_VERTEX_INPUT_RATE_VERTEX // input rate
            },
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
                1,                          // binding
                VK_FORMAT_R32G32B32_SFLOAT, // format
                0                           // offset
            },
            {
                2,                       // location
                2,                       // binding
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
            static_cast<u32>(vertexInfoShaderBinding.size()),
            vertexInfoShaderBinding.data(),
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
            VK_FRONT_FACE_COUNTER_CLOCKWISE,
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

        const VkPipelineDepthStencilStateCreateInfo blitDepthStencilInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            nullptr,
            0,
            true, // depth test
            true, // depth write
            UseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS,
            false,
            false,
            VkStencilOpState{},
            VkStencilOpState{},
            0.f,
            0.f};

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

        std::array<VkDescriptorSetLayoutBinding, 4> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

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
                                                               &blitDepthStencilInfo,
                                                               &blitBlendStateInfo,
                                                               &blitDynamicState,
                                                               pipelineLayout,
                                                               renderPass,
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
} // namespace

void vulkan_test_graphics(ReaperRoot& root, VulkanBackend& backend, GlobalResources& resources)
{
    // Read mesh file
    std::ifstream modelFile("res/model/suzanne.obj");
    const Mesh    mesh = ModelLoader::loadOBJ(modelFile);

    // Create vk buffers
    BufferInfo shadowMapPassConstantBuffer =
        create_buffer(root, backend.device, "Shadow Map Pass Constant buffer",
                      DefaultGPUBufferProperties(1, sizeof(ShadowMapPassParams), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance);
    BufferInfo shadowMapInstanceConstantBuffer =
        create_buffer(root, backend.device, "Shadow Map Instance Constant buffer",
                      DefaultGPUBufferProperties(ShadowInstanceCountMax, sizeof(ShadowMapInstanceParams),
                                                 GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    BufferInfo drawPassConstantBuffer = create_buffer(
        root, backend.device, "Draw Pass Constant buffer",
        DefaultGPUBufferProperties(1, sizeof(DrawPassParams), GPUBufferUsage::UniformBuffer), backend.vma_instance);
    BufferInfo drawInstanceConstantBuffer = create_buffer(
        root, backend.device, "Draw Instance Constant buffer",
        DefaultGPUBufferProperties(DrawInstanceCountMax, sizeof(DrawInstanceParams), GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

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

    // Create depth buffer
    GPUTextureProperties depthProperties = DefaultGPUTextureProperties(
        backend.presentInfo.surfaceExtent.width, backend.presentInfo.surfaceExtent.height, PixelFormat::D16_UNORM);
    depthProperties.usageFlags = GPUTextureUsage::DepthStencilAttachment;

    ImageInfo depthBuffer =
        create_image(root, backend.device, "Main Depth Buffer", depthProperties, backend.vma_instance);

    VkImageView depthBufferView = create_depth_image_view(backend.device, depthBuffer);
    log_debug(root, "vulkan: created image view with handle: {}", static_cast<void*>(depthBufferView));

    // Create shadow map
    GPUTextureProperties shadowMapProperties =
        DefaultGPUTextureProperties(ShadowMapResolution, ShadowMapResolution, PixelFormat::D16_UNORM);
    shadowMapProperties.usageFlags =
        GPUTextureUsage::DepthStencilAttachment | GPUTextureUsage::InputAttachment | GPUTextureUsage::Sampled;

    ImageInfo shadowMap = create_image(root, backend.device, "Shadow Map", shadowMapProperties, backend.vma_instance);

    VkSampler           shadowMapSampler = VK_NULL_HANDLE;
    VkSamplerCreateInfo shadowMapSamplerCreateInfo = {};
    shadowMapSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    shadowMapSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    shadowMapSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    shadowMapSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowMapSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowMapSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowMapSamplerCreateInfo.anisotropyEnable = VK_FALSE;
    shadowMapSamplerCreateInfo.maxAnisotropy = 16;
    shadowMapSamplerCreateInfo.borderColor =
        UseReverseZ ? VK_BORDER_COLOR_INT_OPAQUE_BLACK : VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    shadowMapSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    shadowMapSamplerCreateInfo.compareEnable = VK_TRUE;
    shadowMapSamplerCreateInfo.compareOp = UseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
    shadowMapSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadowMapSamplerCreateInfo.mipLodBias = 0.f;
    shadowMapSamplerCreateInfo.minLod = 0.f;
    shadowMapSamplerCreateInfo.maxLod = FLT_MAX;

    Assert(vkCreateSampler(backend.device, &shadowMapSamplerCreateInfo, nullptr, &shadowMapSampler) == VK_SUCCESS);
    log_debug(root, "vulkan: created sampler with handle: {}", static_cast<void*>(shadowMapSampler));

    VkImageView shadowMapView = create_depth_image_view(backend.device, shadowMap);
    log_debug(root, "vulkan: created image view with handle: {}", static_cast<void*>(shadowMapView));

    CullPipelineInfo           cullPipe = create_cull_pipeline(root, backend);
    CompactionPrepPipelineInfo compactPrepPipe = vulkan_create_compaction_prep_pipeline(root, backend);
    CompactionPipelineInfo     compactionPipe = vulkan_create_compaction_pipeline(root, backend);
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
    VkRenderPass  shadowMapPass = create_shadow_raster_pass(root, backend, shadowMap.properties);
    VkFramebuffer shadowMapFramebuffer = VK_NULL_HANDLE;
    {
        VkFramebufferCreateInfo shadowMapFramebufferInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType                sType
            nullptr,                                   // const void                    *pNext
            0,                                         // VkFramebufferCreateFlags       flags
            shadowMapPass,                             // VkRenderPass                   renderPass
            1,                                         // uint32_t                       attachmentCount
            &shadowMapView,                            // const VkImageView             *pAttachments
            shadowMap.properties.width,                // uint32_t                       width
            shadowMap.properties.height,               // uint32_t                       height
            1                                          // uint32_t                       layers
        };

        Assert(vkCreateFramebuffer(backend.device, &shadowMapFramebufferInfo, nullptr, &shadowMapFramebuffer)
               == VK_SUCCESS);
    }

    ShadowMapPipelineInfo shadowMapPipe = create_shadow_map_pipeline(root, backend, shadowMapPass, ShadowMapResolution);
    VkDescriptorSet       shadowMapPassDescriptorSet = VK_NULL_HANDLE;
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              resources.descriptorPool, 1,
                                                              &shadowMapPipe.descSetLayout};

        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &shadowMapPassDescriptorSet)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}",
                  static_cast<void*>(shadowMapPassDescriptorSet));

        const VkDescriptorBufferInfo shadowMapDescPassParams =
            default_descriptor_buffer_info(shadowMapPassConstantBuffer);
        const VkDescriptorBufferInfo shadowMapDescInstanceParams =
            default_descriptor_buffer_info(shadowMapInstanceConstantBuffer);

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
    VkRenderPass               offscreenRenderPass = create_main_raster_pass(root, backend, depthBuffer.properties);
    std::vector<VkFramebuffer> framebuffers;
    create_framebuffers(root, backend, offscreenRenderPass, depthBufferView, framebuffers);

    BlitPipelineInfo blitPipe = vulkan_create_blit_pipeline(root, backend, offscreenRenderPass);
    VkDescriptorSet  pixelConstantsDescriptorSet = VK_NULL_HANDLE;
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              resources.descriptorPool, 1, &blitPipe.descSetLayout};

        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &pixelConstantsDescriptorSet)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}",
                  static_cast<void*>(pixelConstantsDescriptorSet));

        const VkDescriptorBufferInfo drawDescPassParams = default_descriptor_buffer_info(drawPassConstantBuffer);
        const VkDescriptorBufferInfo drawDescInstanceParams =
            default_descriptor_buffer_info(drawInstanceConstantBuffer);
        const VkDescriptorImageInfo drawDescShadowMapTexture = {VK_NULL_HANDLE, shadowMapView,
                                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        const VkDescriptorImageInfo drawDescShadowMapSampler = {shadowMapSampler, VK_NULL_HANDLE,
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

    DS4    ds4("/dev/input/js0");
    Camera camera;
    camera.setPosition(glm::vec3(-5.f, 0.f, 0.f));
    camera.setDirection(glm::vec3(1.f, 0.f, 0.f));

    SceneGraph scene;
    build_scene_graph(scene, &mesh);

    const auto startTime = std::chrono::system_clock::now();
    auto       lastFrameStart = std::chrono::system_clock::now();

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

                    // FIXME the allocator does not support freeing so we'll run out of memory if we're constantly
                    // resizing
                    {
                        vkQueueWaitIdle(backend.deviceInfo.presentQueue);
                        destroy_framebuffers(root, backend.device, framebuffers);

                        vkDestroyImageView(backend.device, depthBufferView, nullptr);
                        vmaDestroyImage(backend.vma_instance, depthBuffer.handle, depthBuffer.allocation);

                        resize_vulkan_wm_swapchain(root, backend, backend.presentInfo, {width, height});

                        depthProperties = DefaultGPUTextureProperties(backend.presentInfo.surfaceExtent.width,
                                                                      backend.presentInfo.surfaceExtent.height,
                                                                      PixelFormat::D16_UNORM);
                        depthProperties.usageFlags = GPUTextureUsage::DepthStencilAttachment;

                        log_debug(root, "vulkan: creating new image: extent = {}x{}x{}, format = {}",
                                  depthProperties.width, depthProperties.height, depthProperties.depth,
                                  static_cast<u32>(depthProperties.format)); // FIXME print format
                        log_debug(root, "- mips = {}, layers = {}, samples = {}", depthProperties.mipCount,
                                  depthProperties.layerCount, depthProperties.sampleCount);

                        depthBuffer = create_image(root, backend.device, "Main Depth Buffer", depthProperties,
                                                   backend.vma_instance);
                        depthBufferView = create_depth_image_view(backend.device, depthBuffer);

                        create_framebuffers(root, backend, offscreenRenderPass, depthBufferView, framebuffers);
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
                                                             offscreenRenderPass,
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
            {
                pauseAnimation = !pauseAnimation;
            }

            if (ds4.isPressed(DS4::Square))
            {
                cull_options.freeze_culling = !cull_options.freeze_culling;
            }

            constexpr float yaw_sensitivity = 2.6f;
            constexpr float pitch_sensitivity = 2.5f; // radian per sec
            constexpr float translation_speed = 8.0f; // game units per sec

            const float yaw_diff = ds4.getAxis(DS4::RightAnalogY);
            const float pitch_diff = -ds4.getAxis(DS4::RightAnalogX);

            const glm::mat4 inv_view_matrix = glm::inverse(camera.getViewMatrix());
            const glm::vec3 forward = inv_view_matrix * glm::vec4(0.f, 0.f, 1.f, 0.f);
            const glm::vec3 side = inv_view_matrix * glm::vec4(1.f, 0.f, 0.f, 0.f);

            const float fwd_diff = ds4.getAxis(DS4::LeftAnalogY);
            const float side_diff = ds4.getAxis(DS4::LeftAnalogX);

            glm::vec3   translation = forward * fwd_diff + side * side_diff;
            const float magnitudeSq = glm::dot(translation, translation);

            if (magnitudeSq > 1.f)
                translation *= 1.f / glm::sqrt(magnitudeSq);

            const glm::vec3 camera_offset = translation * translation_speed * timeDtSecs;

            camera.translate(camera_offset);
            camera.rotate(yaw_diff * yaw_sensitivity * timeDtSecs, pitch_diff * pitch_sensitivity * timeDtSecs);
            camera.update(Camera::Spherical);

            const glm::mat4 view_matrix = camera.getViewMatrix();

            float animationTimeMs = pauseAnimation ? 0.f : timeMs;

            update_scene_graph(scene, animationTimeMs, backbuffer_viewport_extent, view_matrix);

            PreparedData prepared;
            prepare_scene(scene, prepared);

            // FIXME do partial copies
            upload_buffer_data(backend.device, backend.vma_instance, drawPassConstantBuffer, &prepared.draw_pass_params,
                               sizeof(DrawPassParams));
            upload_buffer_data(backend.device, backend.vma_instance, shadowMapPassConstantBuffer,
                               &prepared.shadow_pass_params, sizeof(ShadowMapPassParams));
            upload_buffer_data(backend.device, backend.vma_instance, drawInstanceConstantBuffer,
                               prepared.draw_instance_params.data(),
                               prepared.draw_instance_params.size() * sizeof(DrawInstanceParams));
            upload_buffer_data(backend.device, backend.vma_instance, shadowMapInstanceConstantBuffer,
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
                VkRect2D     passRect = {{0, 0}, {shadowMap.properties.width, shadowMap.properties.height}};

                VkRenderPassBeginInfo shadowMapRenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                                      nullptr,
                                                                      shadowMapPass,
                                                                      shadowMapFramebuffer,
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
                    shadowMap.handle,
                    {VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

                vkCmdPipelineBarrier(resources.gfxCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                     &shadowMapImageBarrierInfo);
            }

            // Draw pass
            {
                const u32 pass_index = 0; // FIXME

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
                vkCmdBindIndexBuffer(resources.gfxCmdBuffer, cull_resources.dynamicIndexBuffer.buffer, 0,
                                     get_vk_culling_index_type());
                vkCmdBindVertexBuffers(resources.gfxCmdBuffer, 0, static_cast<u32>(vertexBuffers.size()),
                                       vertexBuffers.data(), vertexBufferOffsets.data());
                vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        blitPipe.pipelineLayout, 0, 1, &pixelConstantsDescriptorSet, 0, nullptr);

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

    vkDestroyPipeline(backend.device, blitPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, blitPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, blitPipe.descSetLayout, nullptr);

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

    vkDestroyRenderPass(backend.device, offscreenRenderPass, nullptr);

    vkDestroyFramebuffer(backend.device, shadowMapFramebuffer, nullptr);
    vkDestroyRenderPass(backend.device, shadowMapPass, nullptr);
    vkDestroySampler(backend.device, shadowMapSampler, nullptr);
    vkDestroyImageView(backend.device, shadowMapView, nullptr);
    vmaDestroyImage(backend.vma_instance, shadowMap.handle, shadowMap.allocation);

    vkDestroyImageView(backend.device, depthBufferView, nullptr);
    vmaDestroyImage(backend.vma_instance, depthBuffer.handle, depthBuffer.allocation);

    destroy_culling_resources(backend, cull_resources);

    vmaDestroyBuffer(backend.vma_instance, staticIndexBuffer.buffer, staticIndexBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, vertexBufferPosition.buffer, vertexBufferPosition.allocation);
    vmaDestroyBuffer(backend.vma_instance, vertexBufferNormal.buffer, vertexBufferNormal.allocation);
    vmaDestroyBuffer(backend.vma_instance, vertexBufferUV.buffer, vertexBufferUV.allocation);

    vmaDestroyBuffer(backend.vma_instance, drawPassConstantBuffer.buffer, drawPassConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, drawInstanceConstantBuffer.buffer, drawInstanceConstantBuffer.allocation);

    vmaDestroyBuffer(backend.vma_instance, shadowMapPassConstantBuffer.buffer, shadowMapPassConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, shadowMapInstanceConstantBuffer.buffer,
                     shadowMapInstanceConstantBuffer.allocation);
}
} // namespace Reaper
