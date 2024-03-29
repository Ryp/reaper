////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TiledLightingPass.h"

#include "FrameGraphPass.h"
#include "ShadowConstants.h"
#include "ShadowMap.h"
#include "TiledRasterPass.h"
#include "VisibilityBufferPass.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/buffer/GPUBufferView.h"
#include "renderer/graph/FrameGraphBuilder.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/PipelineFactory.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"
#include "renderer/vulkan/StorageBufferAllocator.h"
#include "renderer/vulkan/renderpass/LightingPass.h"

#include "profiling/Scope.h"

#include "renderer/shader/tiled_lighting/tiled_lighting.share.hlsl"

namespace Reaper
{
namespace
{
    VkPipeline create_lighting_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                        VkPipelineLayout pipeline_layout)
    {
        return create_compute_pipeline(device, pipeline_layout, shader_modules.tiled_lighting_cs);
    }

    VkPipeline create_lighting_debug_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                              VkPipelineLayout pipeline_layout)
    {
        return create_compute_pipeline(device, pipeline_layout, shader_modules.tiled_lighting_debug_cs);
    }
} // namespace

TiledLightingPassResources create_tiled_lighting_pass_resources(VulkanBackend&   backend,
                                                                PipelineFactory& pipeline_factory)
{
    TiledLightingPassResources resources = {};

    {
        std::vector<VkDescriptorSetLayoutBinding> bindings0 = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {9, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, ShadowMapMaxCount, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        std::vector<VkDescriptorBindingFlags> bindingFlags0(bindings0.size(), VK_FLAGS_NONE);
        bindingFlags0[9] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        std::vector<VkDescriptorSetLayout> descriptor_set_layouts = {
            create_descriptor_set_layout(backend.device, bindings0, bindingFlags0)};

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(TiledLightingPushConstants)};

        VkPipelineLayout pipeline_layout =
            create_pipeline_layout(backend.device, descriptor_set_layouts, std::span(&pushConstantRange, 1));

        resources.lighting.descriptor_set_layout = descriptor_set_layouts[0];
        resources.lighting.pipeline_layout = pipeline_layout;
        resources.lighting.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipeline_layout,
                                          .pipeline_creation_function = &create_lighting_pipeline,
                                      });
    }

    {
        std::vector<VkDescriptorSetLayout> dset_layouts = {resources.lighting.descriptor_set_layout};
        std::vector<VkDescriptorSet>       dsets(dset_layouts.size());

        allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, dset_layouts, dsets);

        resources.tiled_lighting_descriptor_set = dsets[0];
    }

    resources.tiled_lighting_constant_buffer =
        create_buffer(backend.device, "Tiled lighting constants",
                      DefaultGPUBufferProperties(1, sizeof(TiledLightingConstants), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    // Debug
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout = create_descriptor_set_layout(backend.device, bindings);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(TiledLightingDebugPushConstants)};

        VkPipelineLayout pipeline_layout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1),
                                                                  std::span(&pushConstantRange, 1));

        resources.debug.descriptor_set_layout = descriptor_set_layout;
        resources.debug.pipeline_layout = pipeline_layout;
        resources.debug.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipeline_layout,
                                          .pipeline_creation_function = &create_lighting_debug_pipeline,
                                      });
    }

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.debug.descriptor_set_layout, 1),
                             std::span(&resources.debug_descriptor_set, 1));

    return resources;
}

void destroy_tiled_lighting_pass_resources(VulkanBackend& backend, TiledLightingPassResources& resources)
{
    vkDestroyPipelineLayout(backend.device, resources.debug.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.debug.descriptor_set_layout, nullptr);

    vmaDestroyBuffer(backend.vma_instance, resources.tiled_lighting_constant_buffer.handle,
                     resources.tiled_lighting_constant_buffer.allocation);

    vkDestroyPipelineLayout(backend.device, resources.lighting.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.lighting.descriptor_set_layout, nullptr);
}

TiledLightingFrameGraphRecord create_tiled_lighting_pass_record(FrameGraph::Builder&               builder,
                                                                const VisBufferFrameGraphRecord&   vis_buffer_record,
                                                                const ShadowFrameGraphRecord&      shadow,
                                                                const LightRasterFrameGraphRecord& light_raster_record)
{
    TiledLightingFrameGraphRecord tiled_lighting;
    tiled_lighting.pass_handle = builder.create_render_pass("Tiled Lighting");

    for (auto shadow_map_usage_handle : shadow.shadow_maps)
    {
        tiled_lighting.shadow_maps.push_back(
            builder.read_texture(tiled_lighting.pass_handle, shadow_map_usage_handle,
                                 GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL}));
    }

    tiled_lighting.light_list =
        builder.read_buffer(tiled_lighting.pass_handle, light_raster_record.light_raster.light_list,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    tiled_lighting.gbuffer_rt0 =
        builder.read_texture(tiled_lighting.pass_handle, vis_buffer_record.fill_gbuffer.gbuffer_rt0,
                             GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

    tiled_lighting.gbuffer_rt1 =
        builder.read_texture(tiled_lighting.pass_handle, vis_buffer_record.fill_gbuffer.gbuffer_rt1,
                             GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

    tiled_lighting.depth =
        builder.read_texture(tiled_lighting.pass_handle, vis_buffer_record.depth,
                             GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL});

    tiled_lighting.lighting = builder.create_texture(
        tiled_lighting.pass_handle, "Lighting",
        default_texture_properties(
            vis_buffer_record.scene_depth_properties.width, vis_buffer_record.scene_depth_properties.height,
            PixelFormat::B10G11R11_UFLOAT_PACK32,
            GPUTextureUsage::Storage | GPUTextureUsage::Sampled | GPUTextureUsage::ColorAttachment),
        GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                         VK_IMAGE_LAYOUT_GENERAL});

    const GPUBufferProperties tile_debug_properties = DefaultGPUBufferProperties(
        light_raster_record.tile_depth_properties.width * light_raster_record.tile_depth_properties.height,
        sizeof(TileDebug), GPUBufferUsage::StorageBuffer);

    tiled_lighting.tile_debug_texture =
        builder.create_buffer(tiled_lighting.pass_handle, "Tile debug", tile_debug_properties,
                              GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    return tiled_lighting;
}

TiledLightingDebugFrameGraphRecord create_tiled_lighting_debug_pass_record(
    FrameGraph::Builder& builder, const TiledLightingFrameGraphRecord& tiled_lighting_record, VkExtent2D render_extent)
{
    TiledLightingDebugFrameGraphRecord tiled_lighting_debug;

    tiled_lighting_debug.pass_handle = builder.create_render_pass("Tiled Lighting Debug");

    tiled_lighting_debug.tile_debug =
        builder.read_buffer(tiled_lighting_debug.pass_handle, tiled_lighting_record.tile_debug_texture,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    tiled_lighting_debug.output = builder.create_texture(
        tiled_lighting_debug.pass_handle, "Tiled Lighting Debug Texture",
        default_texture_properties(render_extent.width, render_extent.height, PixelFormat::R8G8B8A8_UNORM,
                                   GPUTextureUsage::Storage | GPUTextureUsage::Sampled),
        GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                         VK_IMAGE_LAYOUT_GENERAL});

    return tiled_lighting_debug;
}

void update_tiled_lighting_pass_resources(VulkanBackend& backend, const FrameGraph::FrameGraph& frame_graph,
                                          const FrameGraphResources&           frame_graph_resources,
                                          const TiledLightingFrameGraphRecord& record,
                                          DescriptorWriteHelper& write_helper, const PreparedData& prepared,
                                          const LightingPassResources&      lighting_resources,
                                          const TiledLightingPassResources& resources,
                                          const SamplerResources&           sampler_resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.point_lights.empty())
        return;

    upload_buffer_data_deprecated(backend.device, backend.vma_instance, resources.tiled_lighting_constant_buffer,
                                  &prepared.tiled_light_constants, sizeof(TiledLightingConstants));

    const FrameGraphBuffer light_list_buffer =
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.light_list);
    const FrameGraphTexture gbuffer_rt0 =
        get_frame_graph_texture(frame_graph_resources, frame_graph, record.gbuffer_rt0);
    const FrameGraphTexture gbuffer_rt1 =
        get_frame_graph_texture(frame_graph_resources, frame_graph, record.gbuffer_rt1);
    const FrameGraphTexture main_view_depth = get_frame_graph_texture(frame_graph_resources, frame_graph, record.depth);
    const FrameGraphTexture lighting_output =
        get_frame_graph_texture(frame_graph_resources, frame_graph, record.lighting);
    const FrameGraphBuffer tile_debug_buffer =
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.tile_debug_texture);

    VkDescriptorSet dset = resources.tiled_lighting_descriptor_set;
    write_helper.append(dset, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, resources.tiled_lighting_constant_buffer.handle);
    write_helper.append(dset, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, light_list_buffer.handle);
    write_helper.append(dset, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, gbuffer_rt0.default_view_handle,
                        gbuffer_rt0.image_layout);
    write_helper.append(dset, 3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, gbuffer_rt1.default_view_handle,
                        gbuffer_rt1.image_layout);
    write_helper.append(dset, 4, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, main_view_depth.default_view_handle,
                        main_view_depth.image_layout);
    write_helper.append(dset, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lighting_resources.point_light_buffer_alloc.buffer,
                        lighting_resources.point_light_buffer_alloc.offset_bytes,
                        lighting_resources.point_light_buffer_alloc.size_bytes);
    write_helper.append(dset, 6, sampler_resources.shadow_map_sampler);
    write_helper.append(dset, 7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, lighting_output.default_view_handle,
                        lighting_output.image_layout);
    write_helper.append(dset, 8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, tile_debug_buffer.handle);

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
            create_image_descriptor_write(dset, 9, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shadow_map_image_infos));
    }
}

void record_tiled_lighting_command_buffer(const FrameGraphHelper&              frame_graph_helper,
                                          const TiledLightingFrameGraphRecord& pass_record, CommandBuffer& cmdBuffer,
                                          const PipelineFactory&            pipeline_factory,
                                          const TiledLightingPassResources& resources, VkExtent2D render_extent,
                                          VkExtent2D tile_extent)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Tiled Lighting");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      get_pipeline(pipeline_factory, resources.lighting.pipeline_index));

    TiledLightingPushConstants push_constants;
    push_constants.extent_ts = glm::uvec2(render_extent.width, render_extent.height);
    push_constants.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(render_extent.width), 1.f / static_cast<float>(render_extent.height));
    push_constants.tile_count_x = tile_extent.width;

    vkCmdPushConstants(cmdBuffer.handle, resources.lighting.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.lighting.pipeline_layout, 0, 1,
                            &resources.tiled_lighting_descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(render_extent.width, TiledLightingThreadCountX),
                  div_round_up(render_extent.height, TiledLightingThreadCountY),
                  1);
}

void update_tiled_lighting_debug_pass_resources(const FrameGraph::FrameGraph&             frame_graph,
                                                const FrameGraphResources&                frame_graph_resources,
                                                const TiledLightingDebugFrameGraphRecord& record,
                                                DescriptorWriteHelper&                    write_helper,
                                                const TiledLightingPassResources&         resources)
{
    const FrameGraphBuffer tile_debug_buffer =
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.tile_debug);
    const FrameGraphTexture tile_debug_texture =
        get_frame_graph_texture(frame_graph_resources, frame_graph, record.output);

    write_helper.append(resources.debug_descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, tile_debug_buffer.handle);
    write_helper.append(resources.debug_descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        tile_debug_texture.default_view_handle, tile_debug_texture.image_layout);
}

void record_tiled_lighting_debug_command_buffer(const FrameGraphHelper&                   frame_graph_helper,
                                                const TiledLightingDebugFrameGraphRecord& pass_record,
                                                CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                                const TiledLightingPassResources& resources, VkExtent2D render_extent,
                                                VkExtent2D tile_extent)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Tiled Lighting Debug");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      get_pipeline(pipeline_factory, resources.debug.pipeline_index));

    TiledLightingDebugPushConstants push_constants;
    push_constants.extent_ts = glm::uvec2(render_extent.width, render_extent.height);
    push_constants.tile_count_x = tile_extent.width;

    vkCmdPushConstants(cmdBuffer.handle, resources.debug.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.debug.pipeline_layout, 0, 1,
                            &resources.debug_descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(render_extent.width, TiledLightingThreadCountX),
                  div_round_up(render_extent.height, TiledLightingThreadCountY),
                  1);
}
} // namespace Reaper
