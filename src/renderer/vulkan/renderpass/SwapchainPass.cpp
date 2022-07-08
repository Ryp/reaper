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
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
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
            case VK_FORMAT_R16G16B16A16_UNORM:
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
                return TRANSFER_FUNC_SRGB; // We have to manually apply the EOTF
            default:
                break;
            }

            AssertUnreachable();
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

    VkPipeline create_swapchain_pipeline(VulkanBackend& backend, VkPipelineLayout pipelineLayout,
                                         VkFormat swapchain_format)
    {
        const char*    entryPoint = "main";
        VkShaderModule shaderVS =
            vulkan_create_shader_module(backend.device, "build/shader/fullscreen_triangle.vert.spv");
        VkShaderModule shaderFS = vulkan_create_shader_module(backend.device, "build/shader/swapchain_write.frag.spv");

        struct SpecConstants
        {
            hlsl_uint transfer_function_index;
            hlsl_uint color_space_index;
            hlsl_uint tonemap_function_index;
        };

        std::vector<VkSpecializationMapEntry> specialization_constants_entries = {
            VkSpecializationMapEntry{
                0,
                offsetof(SpecConstants, transfer_function_index),
                sizeof(SpecConstants::transfer_function_index),
            },
            VkSpecializationMapEntry{
                1,
                offsetof(SpecConstants, color_space_index),
                sizeof(SpecConstants::color_space_index),
            },
            VkSpecializationMapEntry{
                2,
                offsetof(SpecConstants, tonemap_function_index),
                sizeof(SpecConstants::tonemap_function_index),
            },
        };

        SpecConstants spec_constants;
        spec_constants.transfer_function_index =
            get_transfer_function(backend.presentInfo.surfaceFormat, backend.presentInfo.view_format);
        spec_constants.color_space_index = get_color_space(backend.presentInfo.surfaceFormat.colorSpace);
        spec_constants.tonemap_function_index = TONEMAP_FUNC_NONE;

        VkSpecializationInfo specialization = {
            static_cast<u32>(specialization_constants_entries.size()), // uint32_t mapEntryCount;
            specialization_constants_entries.data(), // const VkSpecializationMapEntry*    pMapEntries;
            sizeof(SpecConstants),                   // size_t                             dataSize;
            &spec_constants,                         // const void*                        pData;
        };

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, shaderVS,
             entryPoint, nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, shaderFS,
             entryPoint, &specialization}};

        VkPipelineColorBlendAttachmentState blend_attachment_state = default_pipeline_color_blend_attachment_state();

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
        pipeline_properties.blend_state.attachmentCount = 1;
        pipeline_properties.blend_state.pAttachments = &blend_attachment_state;
        pipeline_properties.pipeline_layout = pipelineLayout;
        pipeline_properties.pipeline_rendering.colorAttachmentCount = 1;
        pipeline_properties.pipeline_rendering.pColorAttachmentFormats = &swapchain_format;

        VkPipeline pipeline = create_graphics_pipeline(backend.device, shader_stages, pipeline_properties);

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
} // namespace

SwapchainPassResources create_swapchain_pass_resources(ReaperRoot& root, VulkanBackend& backend)
{
    SwapchainPassResources resources = {};

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };

    resources.descriptorSetLayout = create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

    resources.pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&resources.descriptorSetLayout, 1));
    resources.pipeline =
        create_swapchain_pipeline(backend, resources.pipelineLayout, backend.presentInfo.surfaceFormat.format);

    resources.passConstantBuffer =
        create_buffer(root, backend.device, "Swapchain Pass Constant buffer",
                      DefaultGPUBufferProperties(1, sizeof(SwapchainPassParams), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.descriptor_set = create_swapchain_pass_descriptor_set(root, backend, resources.descriptorSetLayout);

    return resources;
}

void destroy_swapchain_pass_resources(VulkanBackend& backend, const SwapchainPassResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.passConstantBuffer.handle,
                     resources.passConstantBuffer.allocation);

    vkDestroyPipeline(backend.device, resources.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.descriptorSetLayout, nullptr);
}

void reload_swapchain_pipeline(VulkanBackend& backend, SwapchainPassResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    vkDestroyPipeline(backend.device, resources.pipeline, nullptr);

    resources.pipeline =
        create_swapchain_pipeline(backend, resources.pipelineLayout, backend.presentInfo.surfaceFormat.format);
}

void update_swapchain_pass_descriptor_set(VulkanBackend& backend, const SwapchainPassResources& resources,
                                          const SamplerResources& sampler_resources, VkImageView hdr_scene_texture_view,
                                          VkImageView gui_texture_view)
{
    const VkDescriptorBufferInfo passParams = default_descriptor_buffer_info(resources.passConstantBuffer);
    const VkDescriptorImageInfo  sampler = {sampler_resources.linearBlackBorderSampler, VK_NULL_HANDLE,
                                           VK_IMAGE_LAYOUT_UNDEFINED};
    const VkDescriptorImageInfo  sceneTexture = {VK_NULL_HANDLE, hdr_scene_texture_view,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkDescriptorImageInfo  guiTextureInfo = {VK_NULL_HANDLE, gui_texture_view,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    std::vector<VkWriteDescriptorSet> writes = {
        create_buffer_descriptor_write(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &passParams),
        create_image_descriptor_write(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLER, &sampler),
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
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipeline);

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

    const VkViewport blitViewport = default_vk_viewport(blitPassRect);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &blitViewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &blitPassRect);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipelineLayout, 0, 1,
                            &pass_resources.descriptor_set, 0, nullptr);

    vkCmdDraw(cmdBuffer.handle, 3, 1, 0, 0);

    vkCmdEndRendering(cmdBuffer.handle);
}
} // namespace Reaper
