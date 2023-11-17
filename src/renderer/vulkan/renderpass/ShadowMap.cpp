////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ShadowMap.h"

#include "MeshletCulling.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/buffer/GPUBufferView.h"
#include "renderer/graph/FrameGraphBuilder.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/ShaderModules.h"
#include "renderer/vulkan/StorageBufferAllocator.h"

#include <array>
#include <fmt/format.h>

#include "renderer/shader/shadow/shadow_map_pass.share.hlsl"

namespace Reaper
{
constexpr u32 ShadowInstanceCountMax = 512;

ShadowMapResources create_shadow_map_resources(VulkanBackend& backend, const ShaderModules& shader_modules)
{
    ShadowMapResources resources = {};

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
        default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, shader_modules.render_shadow_vs)};

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
    };

    resources.pipe.descSetLayout = create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);
    resources.pipe.pipelineLayout = create_pipeline_layout(backend.device, std::span(&resources.pipe.descSetLayout, 1));

    GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
    pipeline_properties.input_assembly.primitiveRestartEnable = VK_TRUE;
    pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
    pipeline_properties.depth_stencil.depthWriteEnable = VK_TRUE;
    pipeline_properties.depth_stencil.depthCompareOp = ShadowUseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
    pipeline_properties.pipeline_layout = resources.pipe.pipelineLayout;
    pipeline_properties.pipeline_rendering.depthAttachmentFormat = PixelFormatToVulkan(ShadowMapFormat);

    resources.pipe.pipeline = create_graphics_pipeline(backend.device, shader_stages, pipeline_properties);

    resources.descriptor_sets.resize(3); // FIXME

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, resources.pipe.descSetLayout,
                             resources.descriptor_sets);

    return resources;
}

void destroy_shadow_map_resources(VulkanBackend& backend, ShadowMapResources& resources)
{
    vkDestroyPipeline(backend.device, resources.pipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.descSetLayout, nullptr);
}

ShadowFrameGraphRecord create_shadow_map_pass_record(FrameGraph::Builder&                builder,
                                                     const CullMeshletsFrameGraphRecord& meshlet_pass,
                                                     const PreparedData&                 prepared)
{
    ShadowFrameGraphRecord record;
    record.pass_handle = builder.create_render_pass("Shadow");

    for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
    {
        const GPUTextureProperties properties = default_texture_properties(
            shadow_pass.shadow_map_size.x, shadow_pass.shadow_map_size.y, ShadowMapFormat,
            GPUTextureUsage::DepthStencilAttachment | GPUTextureUsage::InputAttachment | GPUTextureUsage::Sampled);

        record.shadow_maps.push_back(builder.create_texture(
            record.pass_handle, "Shadow map", properties,
            GPUTextureAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                             VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL}));
    }

    record.meshlet_counters = builder.read_buffer(
        record.pass_handle, meshlet_pass.cull_triangles.meshlet_counters,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    record.meshlet_indirect_draw_commands = builder.read_buffer(
        record.pass_handle, meshlet_pass.cull_triangles.meshlet_indirect_draw_commands,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    record.meshlet_visible_index_buffer =
        builder.read_buffer(record.pass_handle, meshlet_pass.cull_triangles.meshlet_visible_index_buffer,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT});

    return record;
}

void update_shadow_map_resources(DescriptorWriteHelper& write_helper, StorageBufferAllocator& frame_storage_allocator,
                                 const PreparedData& prepared, ShadowMapResources& resources,
                                 GPUBuffer& vertex_position_buffer)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.shadow_instance_params.empty())
        return;

    const StorageBufferAlloc alloc = allocate_storage(frame_storage_allocator, prepared.shadow_instance_params.size()
                                                                                   * sizeof(ShadowMapInstanceParams));

    upload_storage_buffer(frame_storage_allocator, alloc, prepared.shadow_instance_params.data());

    for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
    {
        if (shadow_pass.instance_count > 0)
        {
            Assert(shadow_pass.pass_index < resources.descriptor_sets.size());

            VkDescriptorSet descriptor_set = resources.descriptor_sets[shadow_pass.pass_index];

            const GPUBufferView instances_view = {
                .offset_bytes = alloc.offset_bytes + shadow_pass.instance_offset * sizeof(ShadowMapInstanceParams),
                .size_bytes = shadow_pass.instance_count * sizeof(ShadowMapInstanceParams),
            };

            write_helper.append(descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, alloc.buffer,
                                instances_view.offset_bytes, instances_view.size_bytes);
            write_helper.append(descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertex_position_buffer.handle);
        }
    }
}

void record_shadow_map_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                      ShadowMapResources& resources, std::span<const FrameGraphTexture> shadow_maps,
                                      const FrameGraphBuffer& meshlet_counters,
                                      const FrameGraphBuffer& meshlet_indirect_draw_commands,
                                      const FrameGraphBuffer& meshlet_visible_index_buffer)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipe.pipeline);

    for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
    {
        if (shadow_pass.instance_count == 0)
            continue;

        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle,
                                         fmt::format("Shadow Pass {}", shadow_pass.pass_index).c_str());
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer, "Shadow Pass");

        const FrameGraphTexture& shadow_map = shadow_maps[shadow_pass.pass_index];
        const VkExtent2D         output_extent = {shadow_pass.shadow_map_size.x, shadow_pass.shadow_map_size.y};
        const VkRect2D           pass_rect = default_vk_rect(output_extent);
        const VkViewport         viewport = default_vk_viewport(pass_rect);

        Assert(output_extent.width == shadow_map.properties.width);
        Assert(output_extent.height == shadow_map.properties.height);

        vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
        vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

        VkRenderingAttachmentInfo depth_attachment =
            default_rendering_attachment_info(shadow_map.default_view_handle, shadow_map.image_layout);
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.clearValue = VkClearDepthStencil(ShadowUseReverseZ ? 0.f : 1.f, 0);

        const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, nullptr, &depth_attachment);

        vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

        const MeshletDrawParams meshlet_draw = get_meshlet_draw_params(shadow_pass.pass_index);

        vkCmdBindIndexBuffer(cmdBuffer.handle, meshlet_visible_index_buffer.handle, meshlet_draw.index_buffer_offset,
                             meshlet_draw.index_type);

        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipe.pipelineLayout, 0, 1,
                                &resources.descriptor_sets[shadow_pass.pass_index], 0, nullptr);

        vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, meshlet_indirect_draw_commands.handle,
                                      meshlet_draw.command_buffer_offset, meshlet_counters.handle,
                                      meshlet_draw.counter_buffer_offset, meshlet_draw.command_buffer_max_count,
                                      meshlet_indirect_draw_commands.properties.element_size_bytes);

        vkCmdEndRendering(cmdBuffer.handle);
    }
}
} // namespace Reaper
