////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "SwapchainPass.h"

#include "FrameGraphPass.h"
#include "ShadowConstants.h"

#include "renderer/graph/FrameGraphBuilder.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"

#include "profiling/Scope.h"

#include "renderer/shader/swapchain_write.share.hlsl"

namespace Reaper
{
namespace
{
    struct TransferFunction
    {
        hlsl_uint function_index;
        hlsl_uint dynamic_range;
    };

    TransferFunction get_transfer_function(VkSurfaceFormatKHR surface_format, VkFormat view_format)
    {
        switch (surface_format.colorSpace)
        {
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: {
            switch (view_format)
            {
            case VK_FORMAT_B8G8R8A8_SRGB:
            case VK_FORMAT_R8G8B8A8_SRGB:
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
                return {TRANSFER_FUNC_LINEAR, DYNAMIC_RANGE_SDR}; // The EOTF is performed by the view format
            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
                // We have to manually apply the EOTF
                // Note that this shouldn't happen in practice since we usually force a sRGB view to get the conversion
                // for free
                return {TRANSFER_FUNC_SRGB, DYNAMIC_RANGE_SDR};
            default:
                break;
            }

            AssertUnreachable();
            break;
        }
        case VK_COLOR_SPACE_BT709_LINEAR_EXT:
            return {TRANSFER_FUNC_LINEAR, DYNAMIC_RANGE_SDR};
        case VK_COLOR_SPACE_BT709_NONLINEAR_EXT:
            return {TRANSFER_FUNC_REC709, DYNAMIC_RANGE_SDR};
        case VK_COLOR_SPACE_HDR10_ST2084_EXT:
            return {TRANSFER_FUNC_PQ, DYNAMIC_RANGE_HDR};
        case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
            return {TRANSFER_FUNC_WINDOWS_SCRGB, DYNAMIC_RANGE_HDR};
        default:
            break;
        }

        AssertUnreachable();
        return {};
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
        case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
            return COLOR_SPACE_SRGB;
        default:
            break;
        }

        AssertUnreachable();
        return 0;
    }

    struct SpecConstants
    {
        hlsl_uint transfer_function_index;
        hlsl_uint color_space_index;
        hlsl_uint dynamic_range;
        hlsl_uint tonemap_function_index;
    };

    SpecConstants fill_spec_constants(VkSurfaceFormatKHR surface_format, VkFormat view_format,
                                      VkColorSpaceKHR color_space)
    {
        TransferFunction transfer_function = get_transfer_function(surface_format, view_format);
        return SpecConstants{
            .transfer_function_index = transfer_function.function_index,
            .color_space_index = get_color_space(color_space),
            .dynamic_range = transfer_function.dynamic_range,
            .tonemap_function_index = TONEMAP_FUNC_ACES_APPROX,
        };
    }

    VkPipeline create_swapchain_pipeline(VulkanBackend& backend, const ShaderModules& shader_modules,
                                         VkPipelineLayout pipelineLayout, VkFormat swapchain_format)
    {
        SpecConstants spec_constants =
            fill_spec_constants(backend.presentInfo.surface_format, backend.presentInfo.view_format,
                                backend.presentInfo.surface_format.colorSpace);

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
                offsetof(SpecConstants, dynamic_range),
                sizeof(SpecConstants::dynamic_range),
            },
            VkSpecializationMapEntry{
                3,
                offsetof(SpecConstants, tonemap_function_index),
                sizeof(SpecConstants::tonemap_function_index),
            },
        };

        VkSpecializationInfo specialization = {
            static_cast<u32>(specialization_constants_entries.size()), // uint32_t mapEntryCount;
            specialization_constants_entries.data(), // const VkSpecializationMapEntry*    pMapEntries;
            sizeof(SpecConstants),                   // size_t                             dataSize;
            &spec_constants,                         // const void*                        pData;
        };

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                      shader_modules.fullscreen_triangle_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, shader_modules.swapchain_write_fs,
                                                      &specialization)};

        VkPipelineColorBlendAttachmentState blend_attachment_state = default_pipeline_color_blend_attachment_state();

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
        pipeline_properties.blend_state.attachmentCount = 1;
        pipeline_properties.blend_state.pAttachments = &blend_attachment_state;
        pipeline_properties.pipeline_layout = pipelineLayout;
        pipeline_properties.pipeline_rendering.colorAttachmentCount = 1;
        pipeline_properties.pipeline_rendering.pColorAttachmentFormats = &swapchain_format;

        std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipeline pipeline =
            create_graphics_pipeline(backend.device, shader_stages, pipeline_properties, dynamic_states);

        Assert(backend.physical_device.graphics_queue_family_index
               == backend.physical_device.present_queue_family_index);

        return pipeline;
    }
} // namespace

SwapchainPassResources create_swapchain_pass_resources(VulkanBackend& backend, const ShaderModules& shader_modules)
{
    SwapchainPassResources resources = {};

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
        {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {5, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };

    resources.descriptorSetLayout = create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

    const VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SwapchainWriteParams)};

    resources.pipelineLayout = create_pipeline_layout(backend.device, std::span(&resources.descriptorSetLayout, 1),
                                                      std::span(&push_constant_range, 1));
    resources.pipeline =
        create_swapchain_pipeline(backend, shader_modules, resources.pipelineLayout, backend.presentInfo.view_format);

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.descriptorSetLayout, 1), std::span(&resources.descriptor_set, 1));

    return resources;
}

void destroy_swapchain_pass_resources(VulkanBackend& backend, const SwapchainPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.descriptorSetLayout, nullptr);
}

void reload_swapchain_pipeline(VulkanBackend& backend, const ShaderModules& shader_modules,
                               SwapchainPassResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    vkDestroyPipeline(backend.device, resources.pipeline, nullptr);

    resources.pipeline =
        create_swapchain_pipeline(backend, shader_modules, resources.pipelineLayout, backend.presentInfo.view_format);
}

SwapchainFrameGraphRecord
create_swapchain_pass_record(FrameGraph::Builder&            builder,
                             FrameGraph::ResourceUsageHandle scene_hdr_usage_handle,
                             FrameGraph::ResourceUsageHandle split_tiled_lighting_hdr_usage_handle,
                             FrameGraph::ResourceUsageHandle gui_sdr_usage_handle,
                             FrameGraph::ResourceUsageHandle histogram_buffer_usage_handle,
                             FrameGraph::ResourceUsageHandle average_exposure_usage_handle,
                             FrameGraph::ResourceUsageHandle tiled_debug_texture_overlay_usage_handle)
{
    SwapchainFrameGraphRecord swapchain;
    swapchain.pass_handle = builder.create_render_pass("Swapchain", true);

    swapchain.scene_hdr = builder.read_texture(swapchain.pass_handle, scene_hdr_usage_handle,
                                               GPUTextureAccess{.stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                                .access_mask = VK_ACCESS_2_SHADER_READ_BIT,
                                                                .image_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

    swapchain.lighting_result =
        builder.read_texture(swapchain.pass_handle, split_tiled_lighting_hdr_usage_handle,
                             GPUTextureAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

    swapchain.gui =
        builder.read_texture(swapchain.pass_handle, gui_sdr_usage_handle,
                             GPUTextureAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

    // FIXME just to hook the pass to the render graph
    swapchain.histogram =
        builder.read_buffer(swapchain.pass_handle, histogram_buffer_usage_handle,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    swapchain.average_exposure =
        builder.read_buffer(swapchain.pass_handle, average_exposure_usage_handle,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    swapchain.tile_debug =
        builder.read_texture(swapchain.pass_handle, tiled_debug_texture_overlay_usage_handle,
                             GPUTextureAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

    return swapchain;
}

void update_swapchain_pass_descriptor_set(const FrameGraph::FrameGraph&    frame_graph,
                                          const FrameGraphResources&       frame_graph_resources,
                                          const SwapchainFrameGraphRecord& record, DescriptorWriteHelper& write_helper,
                                          const SwapchainPassResources& resources,
                                          const SamplerResources&       sampler_resources)
{
    const FrameGraphTexture hdr_scene_texture =
        get_frame_graph_texture(frame_graph_resources, frame_graph, record.scene_hdr);
    const FrameGraphTexture lighting_texture =
        get_frame_graph_texture(frame_graph_resources, frame_graph, record.lighting_result);
    const FrameGraphTexture gui_texture = get_frame_graph_texture(frame_graph_resources, frame_graph, record.gui);
    const FrameGraphBuffer  average_exposure =
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.average_exposure);
    const FrameGraphTexture tile_lighting_debug_texture =
        get_frame_graph_texture(frame_graph_resources, frame_graph, record.tile_debug);

    write_helper.append(resources.descriptor_set, 0, sampler_resources.linear_black_border);
    write_helper.append(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        hdr_scene_texture.default_view_handle, hdr_scene_texture.image_layout);
    write_helper.append(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        lighting_texture.default_view_handle, lighting_texture.image_layout);
    write_helper.append(resources.descriptor_set, 3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, gui_texture.default_view_handle,
                        gui_texture.image_layout);
    write_helper.append(resources.descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, average_exposure.handle);
    write_helper.append(resources.descriptor_set, 5, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        tile_lighting_debug_texture.default_view_handle, tile_lighting_debug_texture.image_layout);
}

void record_swapchain_command_buffer(const FrameGraphHelper&          frame_graph_helper,
                                     const SwapchainFrameGraphRecord& pass_record, CommandBuffer& cmdBuffer,
                                     const SwapchainPassResources& pass_resources, VkImageView swapchain_buffer_view,
                                     VkExtent2D swapchain_extent, float exposure_compensation_stops,
                                     float tonemap_min_nits, float tonemap_max_nits, float sdr_ui_max_brightness_nits,
                                     float sdr_peak_brightness_nits)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Swapchain");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const VkRect2D   pass_rect = default_vk_rect(swapchain_extent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipeline);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    VkRenderingAttachmentInfo color_attachment =
        default_rendering_attachment_info(swapchain_buffer_view, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, &color_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    SwapchainWriteParams push_constants;
    push_constants.exposure_compensation = exp2(exposure_compensation_stops);
    push_constants.tonemap_min_nits = tonemap_min_nits;
    push_constants.tonemap_max_nits = tonemap_max_nits;
    push_constants.sdr_ui_max_brightness_nits = sdr_ui_max_brightness_nits;
    push_constants.sdr_peak_brightness_nits = sdr_peak_brightness_nits;

    vkCmdPushConstants(cmdBuffer.handle, pass_resources.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipelineLayout, 0, 1,
                            &pass_resources.descriptor_set, 0, nullptr);

    vkCmdDraw(cmdBuffer.handle, 3, 1, 0, 0);

    vkCmdEndRendering(cmdBuffer.handle);
}
} // namespace Reaper
