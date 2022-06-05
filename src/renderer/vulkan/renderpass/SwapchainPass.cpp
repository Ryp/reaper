////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "SwapchainPass.h"

#include "Frame.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/Debug.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/Shader.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "profiling/Scope.h"

#include "renderer/shader/share/color_space.hlsl"
#include "renderer/shader/share/swapchain.hlsl"

namespace Reaper
{
namespace
{
    hlsl_uint get_transfer_function(VkSurfaceFormatKHR surface_format, VkFormat view_format)
    {
        switch (surface_format.colorSpace)
        {
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: {
            switch (view_format)
            {
            case VK_FORMAT_B8G8R8A8_SRGB:
            case VK_FORMAT_R8G8B8A8_SRGB:
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
                return TRANSFER_FUNC_LINEAR; // The EOTF is performed by the view format
            default:
                break; // This shouldn't happen
            }
            break;
        }
        case VK_COLOR_SPACE_BT709_LINEAR_EXT:
            return TRANSFER_FUNC_LINEAR;
        case VK_COLOR_SPACE_BT709_NONLINEAR_EXT:
            return TRANSFER_FUNC_REC709;
        case VK_COLOR_SPACE_HDR10_ST2084_EXT:
            return TRANSFER_FUNC_PQ;
        case VK_COLOR_SPACE_BT2020_LINEAR_EXT:
            return TRANSFER_FUNC_LINEAR;
        default:
            break;
        }

        AssertUnreachable();
        return 0;
    }

    hlsl_uint get_color_space(VkColorSpaceKHR color_space)
    {
        switch (color_space)
        {
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
            return COLOR_SPACE_SRGB;
        case VK_COLOR_SPACE_BT709_LINEAR_EXT:
        case VK_COLOR_SPACE_BT709_NONLINEAR_EXT:
            return COLOR_SPACE_REC709;
        case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT:
            return COLOR_SPACE_DISPLAY_P3;
        case VK_COLOR_SPACE_BT2020_LINEAR_EXT:
        case VK_COLOR_SPACE_HDR10_ST2084_EXT:
        case VK_COLOR_SPACE_HDR10_HLG_EXT:
            return COLOR_SPACE_REC2020;
        default:
            break;
        }

        AssertUnreachable();
        return 0;
    }

    VkDescriptorSetLayout create_swapchain_descriptor_set_layout(ReaperRoot& root, VulkanBackend& backend)
    {
        std::array<VkDescriptorSetLayoutBinding, 4> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
                                         nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayout));

        return descriptorSetLayout;
    }

    VkPipeline create_swapchain_pipeline(ReaperRoot& root, VulkanBackend& backend, VkPipelineLayout pipelineLayout,
                                         VkFormat swapchain_format)
    {
        const char*    fileNameVS = "./build/shader/fullscreen_triangle.vert.spv";
        const char*    fileNameFS = "./build/shader/swapchain_write.frag.spv";
        const char*    entryPoint = "main";
        VkShaderModule shaderVS = vulkan_create_shader_module(backend.device, fileNameVS);
        VkShaderModule shaderFS = vulkan_create_shader_module(backend.device, fileNameFS);

        std::array<hlsl_uint, 3> constants = {
            get_transfer_function(backend.presentInfo.surfaceFormat, backend.presentInfo.view_format),
            get_color_space(backend.presentInfo.surfaceFormat.colorSpace),
            TONEMAP_FUNC_NONE,
        };

        std::array<VkSpecializationMapEntry, 3> specialization_constants_entries = {
            VkSpecializationMapEntry{
                0,
                0 * sizeof(hlsl_uint),
                sizeof(hlsl_uint),
            },
            VkSpecializationMapEntry{
                1,
                1 * sizeof(hlsl_uint),
                sizeof(hlsl_uint),
            },
            VkSpecializationMapEntry{
                2,
                2 * sizeof(hlsl_uint),
                sizeof(hlsl_uint),
            },
        };

        VkSpecializationInfo specialization = {
            specialization_constants_entries.size(), // uint32_t                           mapEntryCount;
            specialization_constants_entries.data(), // const VkSpecializationMapEntry*    pMapEntries;
            constants.size() * sizeof(hlsl_uint),    // size_t                             dataSize;
            constants.data(),                        // const void*                        pData;
        };

        std::vector<VkPipelineShaderStageCreateInfo> blitShaderStages = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, shaderVS,
             entryPoint, nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, shaderFS,
             entryPoint, &specialization}};

        VkPipelineVertexInputStateCreateInfo blitVertexInputStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, VK_FLAGS_NONE, 0, nullptr, 0, nullptr};

        VkPipelineInputAssemblyStateCreateInfo blitInputAssemblyInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, VK_FLAGS_NONE,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};

        VkPipelineViewportStateCreateInfo blitViewportStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            1,
            nullptr, // dynamic viewport
            1,
            nullptr}; // dynamic scissors

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
            false,
            false,
            VK_COMPARE_OP_GREATER,
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

        VkPipelineCache cache = VK_NULL_HANDLE;

        const std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo    blitDynamicState = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,
            0,
            dynamicStates.size(),
            dynamicStates.data(),
        };

        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            nullptr,
            0, // viewMask;
            1,
            &swapchain_format,
            VK_FORMAT_UNDEFINED,
            VK_FORMAT_UNDEFINED,
        };

        VkGraphicsPipelineCreateInfo blitPipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                               &pipelineRenderingCreateInfo,
                                                               VK_FLAGS_NONE,
                                                               static_cast<u32>(blitShaderStages.size()),
                                                               blitShaderStages.data(),
                                                               &blitVertexInputStateInfo,
                                                               &blitInputAssemblyInfo,
                                                               nullptr,
                                                               &blitViewportStateInfo,
                                                               &blitRasterStateInfo,
                                                               &blitMSStateInfo,
                                                               &blitDepthStencilInfo,
                                                               &blitBlendStateInfo,
                                                               &blitDynamicState,
                                                               pipelineLayout,
                                                               VK_NULL_HANDLE,
                                                               0,
                                                               VK_NULL_HANDLE,
                                                               -1};

        VkPipeline pipeline = VK_NULL_HANDLE;
        Assert(vkCreateGraphicsPipelines(backend.device, cache, 1, &blitPipelineCreateInfo, nullptr, &pipeline)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created blit pipeline with handle: {}", static_cast<void*>(pipeline));

        Assert(backend.physicalDeviceInfo.graphicsQueueFamilyIndex
               == backend.physicalDeviceInfo.presentQueueFamilyIndex);

        vkDestroyShaderModule(backend.device, shaderVS, nullptr);
        vkDestroyShaderModule(backend.device, shaderFS, nullptr);

        return pipeline;
    }

    VkDescriptorSet create_swapchain_pass_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
                                                         VkDescriptorSetLayout layout)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              backend.global_descriptor_pool, 1, &layout};

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptor_set) == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptor_set));

        return descriptor_set;
    }

    VkPipelineLayout create_swapchain_pipeline_layout(ReaperRoot& root, VulkanBackend& backend,
                                                      VkDescriptorSetLayout descriptorSetLayout)
    {
        VkPipelineLayoutCreateInfo blitPipelineLayoutInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, VK_FLAGS_NONE, 1, &descriptorSetLayout, 0, nullptr};

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Assert(vkCreatePipelineLayout(backend.device, &blitPipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);
        log_debug(root, "vulkan: created blit pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

        return pipelineLayout;
    }
} // namespace

SwapchainPassResources create_swapchain_pass_resources(ReaperRoot& root, VulkanBackend& backend)
{
    SwapchainPassResources resources = {};

    resources.descriptorSetLayout = create_swapchain_descriptor_set_layout(root, backend);
    resources.pipelineLayout = create_swapchain_pipeline_layout(root, backend, resources.descriptorSetLayout);
    resources.pipeline =
        create_swapchain_pipeline(root, backend, resources.pipelineLayout, backend.presentInfo.surfaceFormat.format);

    resources.passConstantBuffer =
        create_buffer(root, backend.device, "Swapchain Pass Constant buffer",
                      DefaultGPUBufferProperties(1, sizeof(SwapchainPassParams), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.descriptor_set = create_swapchain_pass_descriptor_set(root, backend, resources.descriptorSetLayout);

    VkSamplerCreateInfo linearBlackBorderSamplerCreateInfo = {};
    linearBlackBorderSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    linearBlackBorderSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    linearBlackBorderSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    linearBlackBorderSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    linearBlackBorderSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    linearBlackBorderSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    linearBlackBorderSamplerCreateInfo.anisotropyEnable = VK_FALSE;
    linearBlackBorderSamplerCreateInfo.maxAnisotropy = 16;
    linearBlackBorderSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    linearBlackBorderSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    linearBlackBorderSamplerCreateInfo.compareEnable = VK_FALSE;
    linearBlackBorderSamplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    linearBlackBorderSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    linearBlackBorderSamplerCreateInfo.mipLodBias = 0.f;
    linearBlackBorderSamplerCreateInfo.minLod = 0.f;
    linearBlackBorderSamplerCreateInfo.maxLod = FLT_MAX;

    Assert(vkCreateSampler(backend.device, &linearBlackBorderSamplerCreateInfo, nullptr,
                           &resources.linearBlackBorderSampler)
           == VK_SUCCESS);

    log_debug(root, "vulkan: created sampler with handle: {}", static_cast<void*>(resources.linearBlackBorderSampler));

    VulkanSetDebugName(backend.device, resources.linearBlackBorderSampler, "LinearBlackBorderSampler");

    return resources;
}

void destroy_swapchain_pass_resources(VulkanBackend& backend, const SwapchainPassResources& resources)
{
    vkDestroySampler(backend.device, resources.linearBlackBorderSampler, nullptr);

    vmaDestroyBuffer(backend.vma_instance, resources.passConstantBuffer.handle,
                     resources.passConstantBuffer.allocation);

    vkDestroyPipeline(backend.device, resources.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.descriptorSetLayout, nullptr);
}

void reload_swapchain_pipeline(ReaperRoot& root, VulkanBackend& backend, SwapchainPassResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    vkDestroyPipeline(backend.device, resources.pipeline, nullptr);

    resources.pipeline =
        create_swapchain_pipeline(root, backend, resources.pipelineLayout, backend.presentInfo.surfaceFormat.format);
}

void update_swapchain_pass_descriptor_set(VulkanBackend& backend, const SwapchainPassResources& resources,
                                          VkImageView hdr_scene_texture_view, VkImageView gui_texture_view)
{
    const VkDescriptorBufferInfo passParams = default_descriptor_buffer_info(resources.passConstantBuffer);
    const VkDescriptorImageInfo  linearBlackBorderSampler = {resources.linearBlackBorderSampler, VK_NULL_HANDLE,
                                                            VK_IMAGE_LAYOUT_UNDEFINED};
    const VkDescriptorImageInfo  sceneTexture = {VK_NULL_HANDLE, hdr_scene_texture_view,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkDescriptorImageInfo  guiTextureInfo = {VK_NULL_HANDLE, gui_texture_view,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    std::vector<VkWriteDescriptorSet> writes = {
        create_buffer_descriptor_write(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &passParams),
        create_image_descriptor_write(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLER,
                                      &linearBlackBorderSampler),
        create_image_descriptor_write(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &sceneTexture),
        create_image_descriptor_write(resources.descriptor_set, 3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &guiTextureInfo),
    };

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}

void upload_swapchain_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                      const SwapchainPassResources& pass_resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    upload_buffer_data(backend.device, backend.vma_instance, pass_resources.passConstantBuffer,
                       &prepared.swapchain_pass_params, sizeof(SwapchainPassParams));
}

void record_swapchain_command_buffer(CommandBuffer& cmdBuffer, const FrameData& frame_data,
                                     const SwapchainPassResources& pass_resources, VkImageView swapchain_buffer_view)
{
    const VkRect2D blitPassRect = default_vk_rect(frame_data.backbufferExtent);

    const VkRenderingAttachmentInfo color_attachment = {
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        nullptr,
        swapchain_buffer_view,
        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        VK_RESOLVE_MODE_NONE,
        VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_STORE,
        VkClearValue{},
    };

    VkRenderingInfo renderingInfo = {
        VK_STRUCTURE_TYPE_RENDERING_INFO,
        nullptr,
        VK_FLAGS_NONE,
        blitPassRect,
        1, // layerCount
        0, // viewMask
        1,
        &color_attachment,
        nullptr,
        nullptr,
    };

    vkCmdBeginRendering(cmdBuffer.handle, &renderingInfo);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipeline);

    const VkViewport blitViewport = default_vk_viewport(blitPassRect);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &blitViewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &blitPassRect);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipelineLayout, 0, 1,
                            &pass_resources.descriptor_set, 0, nullptr);

    vkCmdDraw(cmdBuffer.handle, 3, 1, 0, 0);

    vkCmdEndRendering(cmdBuffer.handle);
}
} // namespace Reaper
