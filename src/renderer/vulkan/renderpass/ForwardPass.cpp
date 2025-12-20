////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ForwardPass.h"

#include "FrameGraphPass.h"
#include "MeshletCulling.h"
#include "ShadowConstants.h"
#include "ShadowMap.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/graph/FrameGraphBuilder.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/MaterialResources.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/PipelineFactory.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"
#include "renderer/vulkan/StorageBufferAllocator.h"
#include "renderer/vulkan/renderpass/Constants.h"
#include "renderer/vulkan/renderpass/ForwardPassConstants.h"
#include "renderer/vulkan/renderpass/LightingPass.h"

#include "renderer/shader/forward.share.hlsl"
#include "renderer/shader/mesh_instance.share.hlsl"

namespace Reaper
{
namespace Forward::zero
{
    enum BindingIndex
    {
        pass_params,
        instance_params,
        visible_meshlets,
        buffer_position_ms,
        buffer_attributes,
        buffer_normal_ms,
        buffer_tangent_ms,
        buffer_uv,
        point_lights,
        shadow_map_sampler,
        shadow_map_array,
        _count,
    };

    std::array<DescriptorBinding, BindingIndex::_count> g_bindings = {
        DescriptorBinding{.slot = 0,
                          .count = 1,
                          .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                          .stage_mask = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {.slot = 1, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_VERTEX_BIT},
        {.slot = 2, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_VERTEX_BIT},
        {.slot = 3, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_VERTEX_BIT},
        {.slot = 4, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_VERTEX_BIT},
        {.slot = 5, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_VERTEX_BIT},
        {.slot = 6, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_VERTEX_BIT},
        {.slot = 7, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.slot = 8, .count = 1, .type = VK_DESCRIPTOR_TYPE_SAMPLER, .stage_mask = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.slot = 9,
         .count = ShadowMapMaxCount,
         .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         .stage_mask = VK_SHADER_STAGE_FRAGMENT_BIT},
    };
} // namespace Forward::zero

namespace Forward::one
{
    enum BindingIndex
    {
        diffuse_map_sampler,
        material_maps,
        _count,
    };

    std::array<DescriptorBinding, BindingIndex::_count> g_bindings = {
        DescriptorBinding{
            .slot = 0, .count = 1, .type = VK_DESCRIPTOR_TYPE_SAMPLER, .stage_mask = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.slot = 1,
         .count = MaterialTextureMaxCount,
         .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         .stage_mask = VK_SHADER_STAGE_FRAGMENT_BIT},
    };
} // namespace Forward::one

constexpr u32 MeshInstanceCountMax = 512;

namespace
{
    VkPipeline create_forward_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                       VkPipelineLayout pipeline_layout)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, shader_modules.forward_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, shader_modules.forward_fs)};

        const VkPipelineColorBlendAttachmentState blend_attachment_state =
            default_pipeline_color_blend_attachment_state();

        const VkFormat color_format = PixelFormatToVulkan(ForwardHDRColorFormat);
        const VkFormat depth_format = PixelFormatToVulkan(MainPassDepthFormat);

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
        pipeline_properties.input_assembly.primitiveRestartEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthWriteEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthCompareOp =
            MainPassUseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
        pipeline_properties.blend_state.attachmentCount = 1;
        pipeline_properties.blend_state.pAttachments = &blend_attachment_state;
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.colorAttachmentCount = 1;
        pipeline_properties.pipeline_rendering.pColorAttachmentFormats = &color_format;
        pipeline_properties.pipeline_rendering.depthAttachmentFormat = depth_format;

        std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipeline pipeline = create_graphics_pipeline(device, shader_stages, pipeline_properties, dynamic_states);

        return pipeline;
    }
} // namespace

ForwardPassResources create_forward_pass_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory)
{
    ForwardPassResources resources = {};

    using namespace Forward;

    std::vector<VkDescriptorSetLayoutBinding> bindings0(zero::g_bindings.size());
    fill_layout_bindings(bindings0, zero::g_bindings);

    std::vector<VkDescriptorBindingFlags> bindingFlags0(bindings0.size(), VK_FLAGS_NONE);
    bindingFlags0.back() = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    std::vector<VkDescriptorSetLayoutBinding> bindings1(one::g_bindings.size());
    fill_layout_bindings(bindings1, one::g_bindings);

    std::vector<VkDescriptorBindingFlags> bindingFlags1 = {VK_FLAGS_NONE, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
        create_descriptor_set_layout(backend.device, bindings0, bindingFlags0),
        create_descriptor_set_layout(backend.device, bindings1, bindingFlags1),
    };

    resources.pipe.desc_set_layout = descriptorSetLayouts[0];
    resources.pipe.desc_set_layout_material = descriptorSetLayouts[1];

    resources.pipe.pipeline_layout = create_pipeline_layout(backend.device, descriptorSetLayouts);

    resources.pipe.pipeline_index =
        register_pipeline_creator(pipeline_factory,
                                  PipelineCreator{
                                      .pipeline_layout = resources.pipe.pipeline_layout,
                                      .pipeline_creation_function = &create_forward_pipeline,
                                  });

    resources.pass_constant_buffer =
        create_buffer(backend.device, "Forward Pass Constant buffer",
                      DefaultGPUBufferProperties(1, sizeof(ForwardPassParams), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.instance_buffer = create_buffer(
        backend.device, "Forward Instance buffer",
        DefaultGPUBufferProperties(MeshInstanceCountMax, sizeof(MeshInstance), GPUBufferUsage::StorageBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    std::vector<VkDescriptorSet> dsets(descriptorSetLayouts.size());

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, descriptorSetLayouts, dsets);

    resources.descriptor_set = dsets[0];
    resources.material_descriptor_set = dsets[1];

    return resources;
}

void destroy_forward_pass_resources(VulkanBackend& backend, ForwardPassResources& resources)
{
    vkDestroyPipelineLayout(backend.device, resources.pipe.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.desc_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.desc_set_layout_material, nullptr);

    vmaDestroyBuffer(backend.vma_instance, resources.pass_constant_buffer.handle,
                     resources.pass_constant_buffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.instance_buffer.handle, resources.instance_buffer.allocation);
}

ForwardFrameGraphRecord create_forward_pass_record(FrameGraph::Builder&                builder,
                                                   const CullMeshletsFrameGraphRecord& meshlet_pass,
                                                   const ShadowFrameGraphRecord&       shadow,
                                                   FrameGraph::ResourceUsageHandle     depth_buffer_usage_handle,
                                                   VkExtent2D                          render_extent)
{
    ForwardFrameGraphRecord forward;
    forward.pass_handle = builder.create_render_pass("Forward");

    forward.scene_hdr = builder.create_texture(
        forward.pass_handle, "Scene HDR",
        default_texture_properties(render_extent.width, render_extent.height, ForwardHDRColorFormat,
                                   GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled),
        GPUTextureAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL});

    forward.depth = builder.write_texture(forward.pass_handle, depth_buffer_usage_handle,
                                          GPUTextureAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                           VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL});

    for (auto shadow_map_usage_handle : shadow.shadow_maps)
    {
        forward.shadow_maps.push_back(
            builder.read_texture(forward.pass_handle, shadow_map_usage_handle,
                                 GPUTextureAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL}));
    }

    forward.meshlet_counters = builder.read_buffer(
        forward.pass_handle, meshlet_pass.cull_triangles.meshlet_counters,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    forward.meshlet_indirect_draw_commands = builder.read_buffer(
        forward.pass_handle, meshlet_pass.cull_triangles.meshlet_indirect_draw_commands,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    forward.meshlet_visible_index_buffer =
        builder.read_buffer(forward.pass_handle, meshlet_pass.cull_triangles.meshlet_visible_index_buffer,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT});

    forward.visible_meshlet_buffer =
        builder.read_buffer(forward.pass_handle, meshlet_pass.cull_triangles.visible_meshlet_buffer,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    return forward;
}

void update_forward_pass_descriptor_sets(VulkanBackend& backend, const FrameGraph::FrameGraph& frame_graph,
                                         const FrameGraphResources&     frame_graph_resources,
                                         const ForwardFrameGraphRecord& record, DescriptorWriteHelper& write_helper,
                                         const PreparedData& prepared, const ForwardPassResources& resources,
                                         const SamplerResources&  sampler_resources,
                                         const MaterialResources& material_resources, const MeshCache& mesh_cache,
                                         const LightingPassResources& lighting_resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.mesh_instances.empty())
        return;

    upload_buffer_data_deprecated(backend.device, backend.vma_instance, resources.pass_constant_buffer,
                                  &prepared.forward_pass_constants, sizeof(ForwardPassParams));

    upload_buffer_data_deprecated(backend.device, backend.vma_instance, resources.instance_buffer,
                                  prepared.mesh_instances.data(),
                                  prepared.mesh_instances.size() * sizeof(MeshInstance));

    const FrameGraphBuffer visible_meshlet_buffer =
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.visible_meshlet_buffer);

    {
        using namespace Forward::zero;
        write_helper.append(resources.descriptor_set, g_bindings[pass_params], resources.pass_constant_buffer.handle);
        write_helper.append(resources.descriptor_set, g_bindings[instance_params], resources.instance_buffer.handle);
        write_helper.append(resources.descriptor_set, g_bindings[visible_meshlets], visible_meshlet_buffer.handle);
        write_helper.append(resources.descriptor_set, g_bindings[buffer_position_ms],
                            mesh_cache.vertexBufferPosition.handle);
        write_helper.append(resources.descriptor_set, g_bindings[buffer_attributes],
                            mesh_cache.vertexAttributesBuffer.handle);
        write_helper.append(resources.descriptor_set, g_bindings[buffer_normal_ms],
                            mesh_cache.vertexBufferNormal.handle);
        write_helper.append(resources.descriptor_set, g_bindings[buffer_tangent_ms],
                            mesh_cache.vertexBufferTangent.handle);
        write_helper.append(resources.descriptor_set, g_bindings[buffer_uv], mesh_cache.vertexBufferUV.handle);
        write_helper.append(resources.descriptor_set, g_bindings[point_lights],
                            lighting_resources.point_light_buffer_alloc.buffer,
                            lighting_resources.point_light_buffer_alloc.offset_bytes,
                            lighting_resources.point_light_buffer_alloc.size_bytes);
        write_helper.append(resources.descriptor_set, g_bindings[shadow_map_sampler],
                            sampler_resources.shadow_map_sampler);

        if (!record.shadow_maps.empty())
        {
            std::span<VkDescriptorImageInfo> shadow_map_image_infos =
                write_helper.new_image_infos(static_cast<u32>(record.shadow_maps.size()));

            for (u32 index = 0; index < record.shadow_maps.size(); index += 1)
            {
                const FrameGraphTexture shadow_map =
                    get_frame_graph_texture(frame_graph_resources, frame_graph, record.shadow_maps[index]);

                shadow_map_image_infos[index] =
                    create_descriptor_image_info(shadow_map.default_view_handle, shadow_map.image_layout);
            }

            write_helper.writes.push_back(
                create_image_descriptor_write(resources.descriptor_set, g_bindings[shadow_map_array].slot,
                                              g_bindings[shadow_map_array].type, shadow_map_image_infos));
        }
    }

    if (!material_resources.textures.empty())
    {
        using namespace Forward::one;

        write_helper.append(resources.material_descriptor_set, g_bindings[diffuse_map_sampler],
                            sampler_resources.diffuse_map_sampler);

        std::span<VkDescriptorImageInfo> albedo_image_infos =
            write_helper.new_image_infos(static_cast<u32>(material_resources.textures.size()));

        for (u32 index = 0; index < albedo_image_infos.size(); index += 1)
        {
            const auto& albedo_map = material_resources.textures[index];
            albedo_image_infos[index] =
                create_descriptor_image_info(albedo_map.default_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        write_helper.writes.push_back(
            create_image_descriptor_write(resources.material_descriptor_set, g_bindings[material_maps].slot,
                                          g_bindings[material_maps].type, albedo_image_infos));
    }
}

void record_forward_pass_command_buffer(const FrameGraphHelper&        frame_graph_helper,
                                        const ForwardFrameGraphRecord& pass_record, CommandBuffer& cmdBuffer,
                                        const PipelineFactory& pipeline_factory, const PreparedData& prepared,
                                        const ForwardPassResources& pass_resources)
{
    if (prepared.mesh_instances.empty())
        return;

    REAPER_GPU_SCOPE(cmdBuffer, "Forward");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const FrameGraphBuffer meshlet_counters = get_frame_graph_buffer(
        frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.meshlet_counters);
    const FrameGraphBuffer meshlet_indirect_draw_commands = get_frame_graph_buffer(
        frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.meshlet_indirect_draw_commands);
    const FrameGraphBuffer meshlet_visible_index_buffer = get_frame_graph_buffer(
        frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.meshlet_visible_index_buffer);
    const FrameGraphTexture hdr_buffer =
        get_frame_graph_texture(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.scene_hdr);
    const FrameGraphTexture depth_buffer =
        get_frame_graph_texture(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.depth);

    const VkExtent2D extent = {hdr_buffer.properties.width, hdr_buffer.properties.height};
    const VkRect2D   pass_rect = default_vk_rect(extent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      get_pipeline(pipeline_factory, pass_resources.pipe.pipeline_index));

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    VkRenderingAttachmentInfo color_attachment =
        default_rendering_attachment_info(hdr_buffer.default_view_handle, hdr_buffer.image_layout);
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.clearValue = VkClearColor({0.04f, 0.04f, 0.04f, 0.0f});

    VkRenderingAttachmentInfo depth_attachment =
        default_rendering_attachment_info(depth_buffer.default_view_handle, depth_buffer.image_layout);
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.clearValue = VkClearDepthStencil(MainPassUseReverseZ ? 0.f : 1.f, 0);

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, &color_attachment, &depth_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    const MeshletDrawParams meshlet_draw = get_meshlet_draw_params(prepared.main_culling_pass_index);

    vkCmdBindIndexBuffer(cmdBuffer.handle, meshlet_visible_index_buffer.handle, meshlet_draw.index_buffer_offset,
                         meshlet_draw.index_type);

    std::array<VkDescriptorSet, 2> pass_descriptors = {
        pass_resources.descriptor_set,
        pass_resources.material_descriptor_set,
    };

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipe.pipeline_layout, 0,
                            static_cast<u32>(pass_descriptors.size()), pass_descriptors.data(), 0, nullptr);

    vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, meshlet_indirect_draw_commands.handle,
                                  meshlet_draw.command_buffer_offset, meshlet_counters.handle,
                                  meshlet_draw.counter_buffer_offset, meshlet_draw.command_buffer_max_count,
                                  meshlet_indirect_draw_commands.properties.element_size_bytes);

    vkCmdEndRendering(cmdBuffer.handle);
}
} // namespace Reaper
