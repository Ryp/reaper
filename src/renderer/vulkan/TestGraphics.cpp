////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TestGraphics.h"

#include "Buffer.h"
#include "ComputeHelper.h"
#include "Debug.h"
#include "Image.h"
#include "Memory.h"
#include "Shader.h"
#include "Swapchain.h"
#include "SwapchainRendererBase.h"
#include "Test.h"

#include "api/Vulkan.h"
#include "api/VulkanStringConversion.h"

#include "allocator/GPUStackAllocator.h"

#include "renderer/GPUBufferProperties.h"
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

#include "renderer/shader/share/culling.hlsl"
#include "renderer/shader/share/draw.hlsl"

namespace Reaper
{
namespace
{
    struct CullPipelineInfo
    {
        VkPipeline            pipeline;
        VkPipelineLayout      pipelineLayout;
        VkDescriptorSetLayout descSetLayout;
    };

    CullPipelineInfo vulkan_create_cull_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        std::array<VkDescriptorSetLayoutBinding, 6> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayout));

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullPushConstants)};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                         nullptr,
                                                         VK_FLAGS_NONE,
                                                         1,
                                                         &descriptorSetLayout,
                                                         1,
                                                         &cullPushConstantRange};

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Assert(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

        log_debug(root, "vulkan: created pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

        VkShaderModule        computeShader = VK_NULL_HANDLE;
        const char*           fileName = "./build/spv/cull_triangle_batch.comp.spv";
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

        return CullPipelineInfo{pipeline, pipelineLayout, descriptorSetLayout};
    }

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

    VkDescriptorBufferInfo default_descriptor_buffer_info(const BufferInfo& bufferInfo)
    {
        return {bufferInfo.buffer, 0, bufferInfo.alloc.size};
    }

    VkWriteDescriptorSet create_buffer_descriptor_write(VkDescriptorSet descriptorSet, u32 binding,
                                                        VkDescriptorType              descriptorType,
                                                        const VkDescriptorBufferInfo* bufferInfo)
    {
        return {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            descriptorSet,
            binding,
            0,
            1,
            descriptorType,
            nullptr,
            bufferInfo,
            nullptr,
        };
    }

    void vulkan_cmd_transition_swapchain_layout(VulkanBackend& backend, VkCommandBuffer commandBuffer)
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

    VkRenderPass create_render_pass(ReaperRoot& /*root*/, VulkanBackend& backend,
                                    const GPUTextureProperties& depthProperties)
    {
        // FIXME build this at swapchain creation
        GPUTextureProperties swapchainProperties = depthProperties;
        swapchainProperties.usageFlags = GPUTextureUsage::Swapchain;
        swapchainProperties.miscFlags = 0;

        // Create a separate render pass for the offscreen rendering as it may differ from the one used for scene
        // rendering
        std::array<VkAttachmentDescription, 2> attachmentDescriptions = {};
        // Color attachment
        attachmentDescriptions[0].format = backend.presentInfo.surfaceFormat.format; // FIXME
        attachmentDescriptions[0].samples = SampleCountToVulkan(swapchainProperties.sampleCount);
        attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Depth attachment
        attachmentDescriptions[1].format = PixelFormatToVulkan(depthProperties.format);
        attachmentDescriptions[1].samples = SampleCountToVulkan(depthProperties.sampleCount);
        attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpassDescription = {};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &colorReference;
        subpassDescription.pDepthStencilAttachment = &depthReference;

        // Use subpass dependencies for layout transitions
        std::array<VkSubpassDependency, 2> dependencies;

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

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

        std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
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

    void update_pass_params(DrawPassParams& draw_pass_params, float timeMs, float aspectRatio, const glm::mat4& view)
    {
        const float near_plane_distance = 0.1f;
        const float far_plane_distance = 100.f;
        const float fov_radian = glm::pi<float>() * 0.25f;

        glm::mat4 projection = glm::perspective(fov_radian, aspectRatio, near_plane_distance, far_plane_distance);

        // Flip viewport Y
        projection[1] = -projection[1];

        if (UseReverseZ)
        {
            // NOTE: we might want to do it by hand to limit precision loss
            glm::mat4 reverse_z_transform(1.f);
            reverse_z_transform[3][2] = 1.f;
            reverse_z_transform[2][2] = -1.f;

            projection = reverse_z_transform * projection;
        }

        draw_pass_params.viewProj = projection * view;

        draw_pass_params.timeMs = timeMs;
    }
} // namespace

void vulkan_test_graphics(ReaperRoot& root, VulkanBackend& backend, GlobalResources& resources)
{
    // Read mesh file
    std::ifstream modelFile("res/model/ship.obj");
    const Mesh    mesh = ModelLoader::loadOBJ(modelFile);

    const u32 instanceCount = 6;

    // Create vk buffers
    BufferInfo drawPassConstantBuffer = create_buffer(
        root, backend.device, "Draw Pass Constant buffer",
        DefaultGPUBufferProperties(1, sizeof(DrawPassParams), GPUBufferUsage::UniformBuffer), resources.mainAllocator);
    BufferInfo drawInstanceConstantBuffer = create_buffer(
        root, backend.device, "Draw Instance Constant buffer",
        DefaultGPUBufferProperties(instanceCount, sizeof(DrawInstanceParams), GPUBufferUsage::StorageBuffer),
        resources.mainAllocator);

    BufferInfo vertexBufferPosition =
        create_buffer(root, backend.device, "Position buffer",
                      DefaultGPUBufferProperties(mesh.vertices.size(), sizeof(mesh.vertices[0]),
                                                 GPUBufferUsage::VertexBuffer | GPUBufferUsage::StorageBuffer),
                      resources.mainAllocator);
    BufferInfo vertexBufferNormal = create_buffer(
        root, backend.device, "Normal buffer",
        DefaultGPUBufferProperties(mesh.normals.size(), sizeof(mesh.normals[0]), GPUBufferUsage::VertexBuffer),
        resources.mainAllocator);
    BufferInfo vertexBufferUV =
        create_buffer(root, backend.device, "UV buffer",
                      DefaultGPUBufferProperties(mesh.uvs.size(), sizeof(mesh.uvs[0]), GPUBufferUsage::VertexBuffer),
                      resources.mainAllocator);
    BufferInfo staticIndexBuffer = create_buffer(
        root, backend.device, "Index buffer",
        DefaultGPUBufferProperties(mesh.indexes.size(), sizeof(mesh.indexes[0]), GPUBufferUsage::StorageBuffer),
        resources.mainAllocator);

    upload_buffer_data(backend.device, vertexBufferPosition, mesh.vertices.data(),
                       mesh.vertices.size() * sizeof(mesh.vertices[0]));
    upload_buffer_data(backend.device, vertexBufferNormal, mesh.normals.data(),
                       mesh.normals.size() * sizeof(mesh.normals[0]));
    upload_buffer_data(backend.device, vertexBufferUV, mesh.uvs.data(), mesh.uvs.size() * sizeof(mesh.uvs[0]));
    upload_buffer_data(backend.device, staticIndexBuffer, mesh.indexes.data(),
                       mesh.indexes.size() * sizeof(mesh.indexes[0]));

    const u32 maxIndirectDrawCount = 2000;

    BufferInfo indirectDrawBuffer =
        create_buffer(root, backend.device, "Indirect draw buffer",
                      DefaultGPUBufferProperties(maxIndirectDrawCount, sizeof(VkDrawIndexedIndirectCommand),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      resources.mainAllocator);

    const u32  indirectDrawCountSize = 2; // Second uint is for keeping track of total triangles
    BufferInfo indirectDrawCountBuffer =
        create_buffer(root, backend.device, "Indirect draw count buffer",
                      DefaultGPUBufferProperties(indirectDrawCountSize, sizeof(u32),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      resources.mainAllocator);

    Assert(maxIndirectDrawCount < backend.physicalDeviceProperties.limits.maxDrawIndirectCount);

    BufferInfo cullInstanceParamsBuffer = create_buffer(
        root, backend.device, "Culling instance constant buffer",
        DefaultGPUBufferProperties(instanceCount, sizeof(CullInstanceParams), GPUBufferUsage::StorageBuffer),
        resources.mainAllocator);

    const u32  dynamicIndexBufferSize = static_cast<u32>(2000_kiB);
    BufferInfo dynamicIndexBuffer =
        create_buffer(root, backend.device, "Culling dynamic index buffer",
                      DefaultGPUBufferProperties(dynamicIndexBufferSize, 1,
                                                 GPUBufferUsage::IndexBuffer | GPUBufferUsage::StorageBuffer),
                      resources.mainAllocator);

    // Create depth buffer
    GPUTextureProperties properties = DefaultGPUTextureProperties(
        backend.presentInfo.surfaceExtent.width, backend.presentInfo.surfaceExtent.height, PixelFormat::D16_UNORM);
    properties.usageFlags = GPUTextureUsage::DepthStencilAttachment;

    ImageInfo depthBuffer = CreateVulkanImage(backend.device, properties, resources.mainAllocator);
    log_debug(root, "vulkan: created image with handle: {}", static_cast<void*>(depthBuffer.handle));

    VkImageView depthBufferView = create_depth_image_view(backend.device, depthBuffer);
    log_debug(root, "vulkan: created image view with handle: {}", static_cast<void*>(depthBufferView));

    VkRenderPass               offscreenRenderPass = create_render_pass(root, backend, depthBuffer.properties);
    std::vector<VkFramebuffer> framebuffers;
    create_framebuffers(root, backend, offscreenRenderPass, depthBufferView, framebuffers);

    // Create fence
    VkFenceCreateInfo fenceInfo = {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
        0 // Not signaled by default
    };

    CullPipelineInfo cullPipe = vulkan_create_cull_pipeline(root, backend);
    VkDescriptorSet  cullPassDescriptorSet = VK_NULL_HANDLE;
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              resources.descriptorPool, 1, &cullPipe.descSetLayout};

        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &cullPassDescriptorSet) == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(cullPassDescriptorSet));

        const VkDescriptorBufferInfo cullDescIndices = default_descriptor_buffer_info(staticIndexBuffer);
        const VkDescriptorBufferInfo cullDescVertexPositions = default_descriptor_buffer_info(vertexBufferPosition);
        const VkDescriptorBufferInfo cullDescInstanceParams = default_descriptor_buffer_info(cullInstanceParamsBuffer);
        const VkDescriptorBufferInfo cullDescIndicesOut = default_descriptor_buffer_info(dynamicIndexBuffer);
        const VkDescriptorBufferInfo cullDescDrawCommandOut = default_descriptor_buffer_info(indirectDrawBuffer);
        const VkDescriptorBufferInfo cullDescDrawCountOut = default_descriptor_buffer_info(indirectDrawCountBuffer);

        std::array<VkWriteDescriptorSet, 6> cullPassDescriptorSetWrites = {
            create_buffer_descriptor_write(cullPassDescriptorSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &cullDescIndices),
            create_buffer_descriptor_write(cullPassDescriptorSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &cullDescVertexPositions),
            create_buffer_descriptor_write(cullPassDescriptorSet, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &cullDescInstanceParams),
            create_buffer_descriptor_write(cullPassDescriptorSet, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &cullDescIndicesOut),
            create_buffer_descriptor_write(cullPassDescriptorSet, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &cullDescDrawCommandOut),
            create_buffer_descriptor_write(cullPassDescriptorSet, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &cullDescDrawCountOut),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(cullPassDescriptorSetWrites.size()),
                               cullPassDescriptorSetWrites.data(), 0, nullptr);
    }

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

        std::array<VkWriteDescriptorSet, 2> drawPassDescriptorSetWrites = {
            create_buffer_descriptor_write(pixelConstantsDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           &drawDescPassParams),
            create_buffer_descriptor_write(pixelConstantsDescriptorSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &drawDescInstanceParams),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(drawPassDescriptorSetWrites.size()),
                               drawPassDescriptorSetWrites.data(), 0, nullptr);
    }

    VkFence drawFence = VK_NULL_HANDLE;
    vkCreateFence(backend.device, &fenceInfo, nullptr, &drawFence);
    log_debug(root, "vulkan: created fence with handle: {}", static_cast<void*>(drawFence));

    IWindow*                   window = root.renderer->window;
    std::vector<Window::Event> events;

    log_info(root, "window: map window");
    window->map();

    DrawPassParams                                draw_pass_params = {};
    std::array<DrawInstanceParams, instanceCount> draw_instance_params = {};
    std::array<CullInstanceParams, instanceCount> cull_instance_params = {};

    const int MaxFrameCount = 0;

    bool mustTransitionSwapchain = true;
    bool saveMyLaptop = true;
    bool shouldExit = false;
    int  frameIndex = 0;
    bool freezeCulling = false;
    bool pauseAnimation = false;

    DS4    ds4("/dev/input/js0");
    Camera camera;
    camera.setPosition(glm::vec3(-5.f, 0.f, 0.f));
    camera.setDirection(glm::vec3(1.f, 0.f, 0.f));

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
                        vkDestroyImage(backend.device, depthBuffer.handle, nullptr);

                        resize_vulkan_wm_swapchain(root, backend, backend.presentInfo, {width, height});

                        properties = DefaultGPUTextureProperties(backend.presentInfo.surfaceExtent.width,
                                                                 backend.presentInfo.surfaceExtent.height,
                                                                 PixelFormat::D16_UNORM);
                        properties.usageFlags = GPUTextureUsage::DepthStencilAttachment;

                        log_debug(root, "vulkan: creating new image: extent = {}x{}x{}, format = {}", properties.width,
                                  properties.height, properties.depth,
                                  static_cast<u32>(properties.format)); // FIXME print format
                        log_debug(root, "- mips = {}, layers = {}, samples = {}", properties.mipCount,
                                  properties.layerCount, properties.sampleCount);

                        depthBuffer = CreateVulkanImage(backend.device, properties, resources.mainAllocator);
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
            const float timeMs = static_cast<float>(timeSecs.count()) * 0.001f;
            const float timeDtSecs = static_cast<float>(timeDeltaMs.count()) * 0.001f;
            const float aspectRatio =
                static_cast<float>(backbufferExtent.width) / static_cast<float>(backbufferExtent.height);

            ds4.update();

            if (ds4.isPressed(DS4::L1))
            {
                pauseAnimation = !pauseAnimation;
            }

            if (ds4.isPressed(DS4::Square))
            {
                freezeCulling = !freezeCulling;
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

            const glm::mat4 view = camera.getViewMatrix();

            float animationTimeMs = pauseAnimation ? 0.f : timeMs;

            update_pass_params(draw_pass_params, animationTimeMs, aspectRatio, view);

            for (u32 i = 0; i < instanceCount; i++)
            {
                const float     ratio = static_cast<float>(i) / static_cast<float>(instanceCount) * 3.1415f * 2.f;
                const glm::vec3 object_position_ws =
                    glm::vec3(glm::cos(ratio + animationTimeMs), glm::cos(ratio), glm::sin(ratio));
                const float     uniform_scale = 0.0005f;
                const glm::mat4 model = glm::rotate(glm::scale(glm::translate(glm::mat4(1.0f), object_position_ws),
                                                               glm::vec3(uniform_scale, uniform_scale, uniform_scale)),
                                                    animationTimeMs + ratio, glm::vec3(0.f, 1.f, 1.f));

                draw_instance_params[i].model = model;

                const glm::mat4 modelView = view * model;
                // Assumption that our 3x3 submatrix is orthonormal (no skew/non-uniform scaling)
                draw_instance_params[i].modelViewInvT = modelView;

                // draw_instance_params[i].modelViewInvT = glm::transpose(glm::inverse(glm::mat3(modelView))); //
                // Assumption that our 3x3 submatrix is orthogonal

                cull_instance_params[i].ms_to_cs_matrix = draw_pass_params.viewProj * model;
            }

            upload_buffer_data(backend.device, drawPassConstantBuffer, &draw_pass_params, sizeof(DrawPassParams));
            upload_buffer_data(backend.device, drawInstanceConstantBuffer, draw_instance_params.data(),
                               draw_instance_params.size() * sizeof(DrawInstanceParams));
            upload_buffer_data(backend.device, cullInstanceParamsBuffer, cull_instance_params.data(),
                               cull_instance_params.size() * sizeof(CullInstanceParams));

            if (!freezeCulling)
            {
                std::array<u32, indirectDrawCountSize> zero = {};
                upload_buffer_data(backend.device, indirectDrawCountBuffer, zero.data(), zero.size() * sizeof(u32));
            }

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

            if (!freezeCulling)
            {
                vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipe.pipeline);

                vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipe.pipelineLayout,
                                        0, 1, &cullPassDescriptorSet, 0, nullptr);

                const u32 index_count = static_cast<u32>(mesh.indexes.size());
                Assert(index_count % 3 == 0);

                CullPushConstants consts;
                consts.triangleCount = index_count / 3;
                consts.firstIndex = 0;

                vkCmdPushConstants(resources.gfxCmdBuffer, cullPipe.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   sizeof(consts), &consts);
                vkCmdDispatch(resources.gfxCmdBuffer, group_count(consts.triangleCount, ComputeCullingGroupSize),
                              instanceCount, 1);
            }

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

            vkCmdBeginRenderPass(resources.gfxCmdBuffer, &blitRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blitPipe.pipeline);
            {
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
                vkCmdBindIndexBuffer(resources.gfxCmdBuffer, dynamicIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdBindVertexBuffers(resources.gfxCmdBuffer, 0, static_cast<u32>(vertexBuffers.size()),
                                       vertexBuffers.data(), vertexBufferOffsets.data());
                vkCmdBindDescriptorSets(resources.gfxCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        blitPipe.pipelineLayout, 0, 1, &pixelConstantsDescriptorSet, 0, nullptr);

                {
                    vkCmdDrawIndexedIndirectCountKHR(resources.gfxCmdBuffer, indirectDrawBuffer.buffer, 0,
                                                     indirectDrawCountBuffer.buffer, 0, maxIndirectDrawCount,
                                                     sizeof(VkDrawIndexedIndirectCommand));
                }
            }

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

            if (saveMyLaptop)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

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

    vkDestroyPipeline(backend.device, cullPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, cullPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, cullPipe.descSetLayout, nullptr);

    destroy_framebuffers(root, backend.device, framebuffers);

    vkDestroyRenderPass(backend.device, offscreenRenderPass, nullptr);

    vkDestroyImageView(backend.device, depthBufferView, nullptr);
    vkDestroyImage(backend.device, depthBuffer.handle, nullptr);

    vkDestroyBuffer(backend.device, cullInstanceParamsBuffer.buffer, nullptr);
    vkDestroyBuffer(backend.device, dynamicIndexBuffer.buffer, nullptr);
    vkDestroyBuffer(backend.device, indirectDrawCountBuffer.buffer, nullptr);
    vkDestroyBuffer(backend.device, indirectDrawBuffer.buffer, nullptr);

    vkDestroyBuffer(backend.device, staticIndexBuffer.buffer, nullptr);
    vkDestroyBuffer(backend.device, vertexBufferPosition.buffer, nullptr);
    vkDestroyBuffer(backend.device, vertexBufferNormal.buffer, nullptr);
    vkDestroyBuffer(backend.device, vertexBufferUV.buffer, nullptr);

    vkDestroyBuffer(backend.device, drawPassConstantBuffer.buffer, nullptr);
    vkDestroyBuffer(backend.device, drawInstanceConstantBuffer.buffer, nullptr);
}
} // namespace Reaper
