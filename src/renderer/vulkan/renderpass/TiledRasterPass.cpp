////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TiledRasterPass.h"

#include "Constants.h"
#include "TiledLightingCommon.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/buffer/GPUBufferView.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"

#include "common/ReaperRoot.h"
#include "mesh/Mesh.h"
#include "mesh/ModelLoader.h"
#include "profiling/Scope.h"

#include "copy_to_depth_from_hzb.share.hlsl"
#include "tiled_lighting/classify_volume.share.hlsl"
#include "tiled_lighting/tile_depth_downsample.share.hlsl"

namespace Reaper
{
constexpr u32 MaxVertexCount = 1024;

namespace RasterPass
{
    enum
    {
        Inner,
        Outer,
        Count
    };
}

TiledRasterResources create_tiled_raster_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                        const ShaderModules& shader_modules)
{
    TiledRasterResources resources = {};

    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(TileDepthConstants)};

        const VkPipelineLayout pipeline_layout = create_pipeline_layout(
            backend.device, std::span(&descriptor_set_layout, 1), std::span(&pushConstantRange, 1));

        const VkPipelineShaderStageCreateInfo shader_stage = default_pipeline_shader_stage_create_info(
            VK_SHADER_STAGE_COMPUTE_BIT, shader_modules.tile_depth_downsample_cs, nullptr,
            VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT);

        VkPipeline pipeline = create_compute_pipeline(backend.device, pipeline_layout, shader_stage);

        Assert(backend.physical_device.subgroup_properties.subgroupSize >= MinWaveLaneCount);

        resources.tile_depth_descriptor_set_layout = descriptor_set_layout;
        resources.tile_depth_pipeline_layout = pipeline_layout;
        resources.tile_depth_pipeline = pipeline;
    }

    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                      shader_modules.fullscreen_triangle_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                      shader_modules.copy_to_depth_from_hzb_fs),
        };

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange constant_range = {VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                                    sizeof(CopyDepthFromHZBPushConstants)};

        VkPipelineLayout pipeline_layout =
            create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1), std::span(&constant_range, 1));

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
        pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthWriteEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.depthAttachmentFormat = PixelFormatToVulkan(MainPassDepthFormat);

        VkPipeline pipeline = create_graphics_pipeline(backend.device, shader_stages, pipeline_properties);

        resources.depth_copy_descriptor_set_layout = descriptor_set_layout;
        resources.depth_copy_pipeline_layout = pipeline_layout;
        resources.depth_copy_pipeline = pipeline;
    }

    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                      shader_modules.rasterize_light_volume_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                      shader_modules.rasterize_light_volume_fs),
        };

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
             nullptr},
            {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
             nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
             nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                                       sizeof(TileLightRasterPushConstants)};

        VkPipelineLayout pipeline_layout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1),
                                                                  std::span(&pushConstantRange, 1));

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
        pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthWriteEnable = VK_FALSE;
        pipeline_properties.depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER; // dynamic
        pipeline_properties.raster.cullMode = VK_CULL_MODE_FRONT_AND_BACK;      // dynamic
        pipeline_properties.input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.depthAttachmentFormat = PixelFormatToVulkan(MainPassDepthFormat);

        const std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                                                            VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
                                                            VK_DYNAMIC_STATE_CULL_MODE};

        VkPipeline pipeline =
            create_graphics_pipeline(backend.device, shader_stages, pipeline_properties, dynamic_states);

        resources.light_raster_descriptor_set_layout = descriptor_set_layout;
        resources.light_raster_pipeline_layout = pipeline_layout;
        resources.light_raster_pipeline = pipeline;
    }

    {
        std::vector<VkDescriptorSetLayoutBinding> layout_bindings = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout = create_descriptor_set_layout(backend.device, layout_bindings);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(ClassifyVolumePushConstants)};

        VkPipelineLayout pipeline_layout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1),
                                                                  std::span(&pushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipeline_layout, shader_modules.classify_volume_cs);

        resources.classify_descriptor_set_layout = descriptor_set_layout;
        resources.classify_pipeline_layout = pipeline_layout;
        resources.classify_pipeline = pipeline;
    }

    std::vector<VkDescriptorSetLayout> dset_layouts = {
        resources.tile_depth_descriptor_set_layout, resources.depth_copy_descriptor_set_layout,
        resources.classify_descriptor_set_layout, resources.light_raster_descriptor_set_layout,
        resources.light_raster_descriptor_set_layout};
    std::vector<VkDescriptorSet> dsets(dset_layouts.size());

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, dset_layouts, dsets);

    resources.tile_depth_descriptor_set = dsets[0];
    resources.depth_copy_descriptor_set = dsets[1];
    resources.classify_descriptor_set = dsets[2];
    resources.light_raster_descriptor_sets[0] = dsets[3];
    resources.light_raster_descriptor_sets[1] = dsets[4];

    {
        const GPUBufferProperties properties = DefaultGPUBufferProperties(
            MaxVertexCount, sizeof(hlsl_float3), GPUBufferUsage::StorageBuffer | GPUBufferUsage::VertexBuffer);

        resources.vertex_buffer_offset = 0;
        resources.vertex_buffer_position = create_buffer(root, backend.device, "Lighting vertex buffer", properties,
                                                         backend.vma_instance, MemUsage::CPU_To_GPU);

        std::vector<Mesh> meshes;
        const Mesh&       icosahedron = meshes.emplace_back(ModelLoader::loadOBJ("res/model/icosahedron.obj"));

        ProxyMeshAlloc& icosahedron_alloc = resources.proxy_mesh_allocs.emplace_back();
        icosahedron_alloc.vertex_offset = resources.vertex_buffer_offset;
        icosahedron_alloc.vertex_count = icosahedron.positions.size();

        resources.vertex_buffer_offset += icosahedron_alloc.vertex_count;

        // FIXME It's assumed here that the mesh indices are flat
        upload_buffer_data(backend.device, backend.vma_instance, resources.vertex_buffer_position, properties,
                           icosahedron.positions.data(),
                           icosahedron.positions.size() * sizeof(icosahedron.positions[0]),
                           icosahedron_alloc.vertex_offset);
    }

    resources.light_volume_buffer = create_buffer(
        root, backend.device, "Light volume buffer",
        DefaultGPUBufferProperties(LightVolumeMax, sizeof(LightVolumeInstance), GPUBufferUsage::UniformBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.proxy_volume_buffer = create_buffer(
        root, backend.device, "Proxy volume buffer",
        DefaultGPUBufferProperties(LightVolumeMax, sizeof(ProxyVolumeInstance), GPUBufferUsage::StorageBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    return resources;
}

void destroy_tiled_raster_pass_resources(VulkanBackend& backend, TiledRasterResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.proxy_volume_buffer.handle,
                     resources.proxy_volume_buffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.light_volume_buffer.handle,
                     resources.light_volume_buffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.vertex_buffer_position.handle,
                     resources.vertex_buffer_position.allocation);

    vkDestroyPipeline(backend.device, resources.depth_copy_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.depth_copy_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.depth_copy_descriptor_set_layout, nullptr);

    vkDestroyPipeline(backend.device, resources.tile_depth_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.tile_depth_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.tile_depth_descriptor_set_layout, nullptr);

    vkDestroyPipeline(backend.device, resources.light_raster_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.light_raster_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.light_raster_descriptor_set_layout, nullptr);

    vkDestroyPipeline(backend.device, resources.classify_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.classify_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.classify_descriptor_set_layout, nullptr);
}

void update_lighting_depth_downsample_descriptor_set(DescriptorWriteHelper&      write_helper,
                                                     const TiledRasterResources& resources,
                                                     const SamplerResources&     sampler_resources,
                                                     const FrameGraphTexture&    scene_depth,
                                                     const FrameGraphTexture&    tile_depth_min,
                                                     const FrameGraphTexture&    tile_depth_max)
{
    write_helper.append(resources.tile_depth_descriptor_set, 0, sampler_resources.linear_clamp);
    write_helper.append(resources.tile_depth_descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        scene_depth.default_view_handle, scene_depth.image_layout);
    write_helper.append(resources.tile_depth_descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        tile_depth_min.default_view_handle, tile_depth_min.image_layout);
    write_helper.append(resources.tile_depth_descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        tile_depth_max.default_view_handle, tile_depth_max.image_layout);
}

void update_depth_copy_pass_descriptor_set(DescriptorWriteHelper&      write_helper,
                                           const TiledRasterResources& resources,
                                           const FrameGraphTexture&    hzb_texture)
{
    write_helper.append(resources.depth_copy_descriptor_set, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        hzb_texture.additional_views[0], hzb_texture.image_layout);
}

void update_classify_descriptor_set(DescriptorWriteHelper& write_helper, const TiledRasterResources& resources,
                                    const FrameGraphBuffer& classification_counters,
                                    const FrameGraphBuffer& draw_commands_inner,
                                    const FrameGraphBuffer& draw_commands_outer)
{
    write_helper.append(resources.classify_descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        resources.vertex_buffer_position.handle);
    write_helper.append(resources.classify_descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        classification_counters.handle);
    write_helper.append(resources.classify_descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        draw_commands_inner.handle);
    write_helper.append(resources.classify_descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        draw_commands_outer.handle);
    write_helper.append(resources.classify_descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        resources.proxy_volume_buffer.handle);
}

void update_light_raster_pass_descriptor_sets(DescriptorWriteHelper&      write_helper,
                                              const TiledRasterResources& resources,
                                              const FrameGraphTexture&    depth_min,
                                              const FrameGraphTexture&    depth_max,
                                              const FrameGraphBuffer&     light_list_buffer)
{
    write_helper.append(resources.light_raster_descriptor_sets[RasterPass::Inner], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        resources.light_volume_buffer.handle);
    write_helper.append(resources.light_raster_descriptor_sets[RasterPass::Inner], 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        depth_min.default_view_handle, depth_min.image_layout);
    write_helper.append(resources.light_raster_descriptor_sets[RasterPass::Inner], 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        light_list_buffer.handle);
    write_helper.append(resources.light_raster_descriptor_sets[RasterPass::Inner], 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        resources.vertex_buffer_position.handle);

    write_helper.append(resources.light_raster_descriptor_sets[RasterPass::Outer], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        resources.light_volume_buffer.handle);
    write_helper.append(resources.light_raster_descriptor_sets[RasterPass::Outer], 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        depth_max.default_view_handle, depth_max.image_layout);
    write_helper.append(resources.light_raster_descriptor_sets[RasterPass::Outer], 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        light_list_buffer.handle);
    write_helper.append(resources.light_raster_descriptor_sets[RasterPass::Outer], 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        resources.vertex_buffer_position.handle);
}

void upload_tiled_raster_pass_frame_resources(VulkanBackend&            backend,
                                              const TiledLightingFrame& tiled_lighting_frame,
                                              TiledRasterResources&     resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (tiled_lighting_frame.light_volumes.empty())
        return;

    upload_buffer_data_deprecated(backend.device, backend.vma_instance, resources.light_volume_buffer,
                                  tiled_lighting_frame.light_volumes.data(),
                                  tiled_lighting_frame.light_volumes.size() * sizeof(LightVolumeInstance));

    upload_buffer_data_deprecated(backend.device, backend.vma_instance, resources.proxy_volume_buffer,
                                  tiled_lighting_frame.proxy_volumes.data(),
                                  tiled_lighting_frame.proxy_volumes.size() * sizeof(ProxyVolumeInstance));
}

void record_tile_depth_pass_command_buffer(CommandBuffer& cmdBuffer, const TiledRasterResources& resources,
                                           VkExtent2D render_extent)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tile_depth_pipeline);

    TileDepthConstants push_constants;
    push_constants.extent_ts = glm::uvec2(render_extent.width, render_extent.height);
    push_constants.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(render_extent.width), 1.f / static_cast<float>(render_extent.height));

    vkCmdPushConstants(cmdBuffer.handle, resources.tile_depth_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tile_depth_pipeline_layout, 0,
                            1, &resources.tile_depth_descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(render_extent.width, TileDepthThreadCountX * 2),
                  div_round_up(render_extent.height, TileDepthThreadCountY * 2),
                  1);
}

void record_depth_copy(CommandBuffer& cmdBuffer, const TiledRasterResources& resources,
                       const FrameGraphTexture& depth_min_dst, const FrameGraphTexture& depth_max_dst)
{
    std::vector<FrameGraphTexture> depth_dsts = {depth_min_dst, depth_max_dst};
    const VkExtent2D               depth_extent = {depth_min_dst.properties.width, depth_min_dst.properties.height};
    const VkRect2D                 pass_rect = default_vk_rect(depth_extent);
    const VkViewport               viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.depth_copy_pipeline);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.depth_copy_pipeline_layout, 0,
                            1, &resources.depth_copy_descriptor_set, 0, nullptr);

    for (u32 depth_index = 0; depth_index < 2; depth_index++)
    {
        const FrameGraphTexture   depth_dst = depth_dsts[depth_index];
        VkRenderingAttachmentInfo depth_attachment =
            default_rendering_attachment_info(depth_dst.default_view_handle, depth_dst.image_layout);
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, nullptr, &depth_attachment);

        vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

        CopyDepthFromHZBPushConstants consts;
        consts.copy_min = (depth_index == 0) ? 1 : 0;

        vkCmdPushConstants(cmdBuffer.handle, resources.depth_copy_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(consts), &consts);

        vkCmdDraw(cmdBuffer.handle, 3, 1, 0, 0);

        vkCmdEndRendering(cmdBuffer.handle);
    }
}

void record_light_classify_command_buffer(CommandBuffer&              cmdBuffer,
                                          const TiledLightingFrame&   tiled_lighting_frame,
                                          const TiledRasterResources& resources)
{
    const ProxyMeshAlloc& icosahedron_alloc = resources.proxy_mesh_allocs.front(); // FIXME

    const float vertex_offset = icosahedron_alloc.vertex_offset;
    const float vertex_count = icosahedron_alloc.vertex_count;
    const float light_volumes_count = tiled_lighting_frame.light_volumes.size();

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.classify_pipeline);

    ClassifyVolumePushConstants push_constants;
    push_constants.vertex_offset = vertex_offset;
    push_constants.vertex_count = vertex_count;
    push_constants.instance_id_offset = 0;          // FIXME
    push_constants.near_clip_plane_depth_vs = 0.1f; // FIXME copied from near_plane_distance

    vkCmdPushConstants(cmdBuffer.handle, resources.classify_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.classify_pipeline_layout, 0, 1,
                            &resources.classify_descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle, div_round_up(vertex_count, ClassifyVolumeThreadCount), light_volumes_count, 1);
}

void record_light_raster_command_buffer(CommandBuffer& cmdBuffer, const TiledRasterResources& resources,
                                        const FrameGraphBuffer& command_counters,
                                        const FrameGraphBuffer& draw_commands_inner,
                                        const FrameGraphBuffer& draw_commands_outer, const FrameGraphTexture& depth_min,
                                        const FrameGraphTexture& depth_max)
{
    std::vector<FrameGraphTexture> depth_buffers = {depth_max, depth_min};
    std::vector<FrameGraphBuffer>  draw_commands = {draw_commands_inner, draw_commands_outer};
    const VkExtent2D               depth_extent = {depth_max.properties.width, depth_max.properties.height};
    const VkRect2D                 pass_rect = default_vk_rect(depth_extent);
    const VkViewport               viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.light_raster_pipeline);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    for (u32 pass_index = 0; pass_index < RasterPass::Count; pass_index++)
    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, pass_index == RasterPass::Inner ? "Inner" : "Outer");
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer, "Raster");

        const FrameGraphTexture   depth_buffer = depth_buffers[pass_index];
        VkRenderingAttachmentInfo depth_attachment =
            default_rendering_attachment_info(depth_buffer.default_view_handle, depth_buffer.image_layout);
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, nullptr, &depth_attachment);

        vkCmdSetCullMode(cmdBuffer.handle,
                         pass_index == RasterPass::Inner ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT);
        vkCmdSetDepthCompareOp(cmdBuffer.handle,
                               pass_index == RasterPass::Inner ? VK_COMPARE_OP_LESS_OR_EQUAL
                                                               : VK_COMPARE_OP_GREATER_OR_EQUAL);

        vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                resources.light_raster_pipeline_layout, 0, 1,
                                &resources.light_raster_descriptor_sets[pass_index], 0, nullptr);

        TileLightRasterPushConstants push_constants;
        push_constants.instance_id_offset = 0; // FIXME
        push_constants.tile_count_x = depth_extent.width;

        vkCmdPushConstants(cmdBuffer.handle, resources.light_raster_pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants),
                           &push_constants);

        const u32 command_buffer_offset = 0;
        const u32 counter_buffer_offset = pass_index * sizeof(u32);
        vkCmdDrawIndirectCount(cmdBuffer.handle, draw_commands[pass_index].handle, command_buffer_offset,
                               command_counters.handle, counter_buffer_offset, TiledRasterMaxIndirectCommandCount,
                               draw_commands[pass_index].properties.element_size_bytes);

        vkCmdEndRendering(cmdBuffer.handle);
    }
}
} // namespace Reaper
