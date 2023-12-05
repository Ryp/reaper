////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TiledRasterPass.h"

#include "Constants.h"
#include "FrameGraphPass.h"
#include "TiledLightingCommon.h"

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
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/PipelineFactory.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"
#include "renderer/vulkan/StorageBufferAllocator.h"

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

namespace Downsample
{
    enum BindingIndex
    {
        Sampler,
        SceneDepth,
        TileDepthMin,
        TileDepthMax,
        _count,
    };

    std::array<DescriptorBinding, BindingIndex::_count> g_bindings = {
        DescriptorBinding{
            .slot = 0, .count = 1, .type = VK_DESCRIPTOR_TYPE_SAMPLER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 1, .count = 1, .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 2, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 3, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
    };
} // namespace Downsample

namespace Classify
{
    enum BindingIndex
    {
        VertexPositionsMS,
        InnerOuterCounter,
        DrawCommandsInner,
        DrawCommandsOuter,
        ProxyVolumeBuffer,
        _count,
    };

    std::array<DescriptorBinding, BindingIndex::_count> g_bindings = {
        DescriptorBinding{.slot = 0,
                          .count = 1,
                          .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 1, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 2, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 3, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = 4, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
    };
} // namespace Classify

namespace Raster
{
    enum BindingIndex
    {
        LightVolumeInstances,
        TileDepth,
        TileVisibleLightIndices,
        VertexPositionsMS,
        _count,
    };

    std::array<DescriptorBinding, BindingIndex::_count> g_bindings = {
        DescriptorBinding{.slot = 0,
                          .count = 1,
                          .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .stage_mask = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {.slot = 1,
         .count = 1,
         .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         .stage_mask = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {
            .slot = 2,
            .count = 1,
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stage_mask = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {.slot = 3, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_VERTEX_BIT},
    };
} // namespace Raster

namespace
{
    VkPipeline create_tiled_raster_depth_copy_pipeline(VkDevice             device,
                                                       const ShaderModules& shader_modules,
                                                       VkPipelineLayout     pipeline_layout)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                      shader_modules.fullscreen_triangle_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                      shader_modules.copy_to_depth_from_hzb_fs),
        };

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
        pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthWriteEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.depthAttachmentFormat = PixelFormatToVulkan(MainPassDepthFormat);

        const std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipeline pipeline = create_graphics_pipeline(device, shader_stages, pipeline_properties, dynamic_states);
        return pipeline;
    }

    VkPipeline create_tiled_raster_classify_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                                     VkPipelineLayout pipeline_layout)
    {
        return create_compute_pipeline(device, pipeline_layout, shader_modules.classify_volume_cs);
    }

    VkPipeline create_tiled_raster_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                            VkPipelineLayout pipeline_layout)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                      shader_modules.rasterize_light_volume_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                      shader_modules.rasterize_light_volume_fs),
        };

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

        return create_graphics_pipeline(device, shader_stages, pipeline_properties, dynamic_states);
    }
} // namespace

TiledRasterResources create_tiled_raster_pass_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory)
{
    TiledRasterResources resources = {};

    {
        std::vector<VkDescriptorSetLayoutBinding> layout_bindings = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        const VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, layout_bindings);

        const VkPushConstantRange constant_range = {VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                                    sizeof(CopyDepthFromHZBPushConstants)};

        const VkPipelineLayout pipeline_layout =
            create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1), std::span(&constant_range, 1));

        resources.depth_copy.descriptor_set_layout = descriptor_set_layout;
        resources.depth_copy.pipeline_layout = pipeline_layout;
        resources.depth_copy.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipeline_layout,
                                          .pipeline_creation_function = &create_tiled_raster_depth_copy_pipeline,
                                      });
    }

    {
        using namespace Raster;

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings(g_bindings.size());
        fill_layout_bindings(layout_bindings, g_bindings);

        const VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, layout_bindings);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                                       sizeof(TileLightRasterPushConstants)};

        const VkPipelineLayout pipeline_layout = create_pipeline_layout(
            backend.device, std::span(&descriptor_set_layout, 1), std::span(&pushConstantRange, 1));

        resources.light_raster.descriptor_set_layout = descriptor_set_layout;
        resources.light_raster.pipeline_layout = pipeline_layout;
        resources.light_raster.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipeline_layout,
                                          .pipeline_creation_function = &create_tiled_raster_pipeline,
                                      });
    }

    {
        using namespace Classify;

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings(g_bindings.size());
        fill_layout_bindings(layout_bindings, g_bindings);

        const VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, layout_bindings);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(ClassifyVolumePushConstants)};

        const VkPipelineLayout pipeline_layout = create_pipeline_layout(
            backend.device, std::span(&descriptor_set_layout, 1), std::span(&pushConstantRange, 1));

        resources.classify.descriptor_set_layout = descriptor_set_layout;
        resources.classify.pipeline_layout = pipeline_layout;
        resources.classify.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipeline_layout,
                                          .pipeline_creation_function = &create_tiled_raster_classify_pipeline,
                                      });
    }

    std::vector<VkDescriptorSetLayout> dset_layouts = {
        resources.depth_copy.descriptor_set_layout, resources.classify.descriptor_set_layout,
        resources.light_raster.descriptor_set_layout, resources.light_raster.descriptor_set_layout};
    std::vector<VkDescriptorSet> dsets(dset_layouts.size());

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, dset_layouts, dsets);

    resources.depth_copy.descriptor_set = dsets[0];
    resources.classify.descriptor_set = dsets[1];
    resources.light_raster.descriptor_sets[0] = dsets[2];
    resources.light_raster.descriptor_sets[1] = dsets[3];

    {
        const GPUBufferProperties properties = DefaultGPUBufferProperties(
            MaxVertexCount, sizeof(hlsl_float3), GPUBufferUsage::StorageBuffer | GPUBufferUsage::VertexBuffer);

        resources.vertex_buffer_offset = 0;
        resources.vertex_buffer_position = create_buffer(backend.device, "Lighting vertex buffer", properties,
                                                         backend.vma_instance, MemUsage::CPU_To_GPU);

        std::vector<Mesh> meshes;
        const Mesh&       icosahedron = meshes.emplace_back(load_obj("res/model/icosahedron.obj"));

        ProxyMeshAlloc& icosahedron_alloc = resources.proxy_mesh_allocs.emplace_back();
        icosahedron_alloc.vertex_offset = resources.vertex_buffer_offset;
        icosahedron_alloc.vertex_count = static_cast<u32>(icosahedron.positions.size());

        resources.vertex_buffer_offset += icosahedron_alloc.vertex_count;

        // FIXME It's assumed here that the mesh indices are flat
        upload_buffer_data(backend.device, backend.vma_instance, resources.vertex_buffer_position, properties,
                           icosahedron.positions.data(),
                           icosahedron.positions.size() * sizeof(icosahedron.positions[0]),
                           icosahedron_alloc.vertex_offset);
    }

    return resources;
}

void destroy_tiled_raster_pass_resources(VulkanBackend& backend, TiledRasterResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.vertex_buffer_position.handle,
                     resources.vertex_buffer_position.allocation);

    vkDestroyPipelineLayout(backend.device, resources.depth_copy.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.depth_copy.descriptor_set_layout, nullptr);

    vkDestroyPipelineLayout(backend.device, resources.light_raster.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.light_raster.descriptor_set_layout, nullptr);

    vkDestroyPipelineLayout(backend.device, resources.classify.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.classify.descriptor_set_layout, nullptr);
}

LightRasterFrameGraphRecord create_tiled_lighting_raster_pass_record(FrameGraph::Builder&        builder,
                                                                     const TiledLightingFrame&   tiled_lighting_frame,
                                                                     const GPUTextureProperties& hzb_properties,
                                                                     FrameGraph::ResourceUsageHandle hzb_usage_handle)
{
    LightRasterFrameGraphRecord                 record;
    LightRasterFrameGraphRecord::TileDepthCopy& tile_depth_copy = record.tile_depth_copy;

    tile_depth_copy.pass_handle = builder.create_render_pass("Tile Depth Copy");

    const GPUTextureProperties tile_depth_properties = default_texture_properties(
        tiled_lighting_frame.tile_count_x, tiled_lighting_frame.tile_count_y, MainPassDepthFormat,
        GPUTextureUsage::DepthStencilAttachment | GPUTextureUsage::Sampled);

    record.tile_depth_properties = tile_depth_properties;

    const GPUTextureAccess tile_depth_copy_dst_access = {
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL};

    tile_depth_copy.depth_min = builder.create_texture(tile_depth_copy.pass_handle, "Tile Depth Min",
                                                       tile_depth_properties, tile_depth_copy_dst_access);
    tile_depth_copy.depth_max = builder.create_texture(tile_depth_copy.pass_handle, "Tile Depth Max",
                                                       tile_depth_properties, tile_depth_copy_dst_access);

    {
        GPUTextureView hzb_view = default_texture_view(hzb_properties);
        hzb_view.subresource.mip_count = 1;
        hzb_view.subresource.mip_offset = 3;

        Assert(tile_depth_properties.width == hzb_properties.width >> hzb_view.subresource.mip_offset);
        Assert(tile_depth_properties.height == hzb_properties.height >> hzb_view.subresource.mip_offset);

        tile_depth_copy.hzb_texture = builder.read_texture(
            tile_depth_copy.pass_handle, hzb_usage_handle,
            {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL},
            std::span(&hzb_view, 1));
    }

    tile_depth_copy.light_list_clear = builder.create_buffer(
        tile_depth_copy.pass_handle, "Light lists",
        DefaultGPUBufferProperties(ElementsPerTile * tile_depth_properties.width * tile_depth_properties.height,
                                   sizeof(u32), GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst),
        GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    const GPUBufferProperties classification_counters_properties = DefaultGPUBufferProperties(
        2, sizeof(u32), GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst | GPUBufferUsage::IndirectBuffer);

    tile_depth_copy.classification_counters_clear = builder.create_buffer(
        tile_depth_copy.pass_handle, "Classification counters", classification_counters_properties,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    LightRasterFrameGraphRecord::Classify& light_classify = record.light_classify;

    light_classify.pass_handle = builder.create_render_pass("Classify Light Volumes");

    light_classify.classification_counters =
        builder.write_buffer(light_classify.pass_handle, tile_depth_copy.classification_counters_clear,
                             GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                             VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT});

    const GPUBufferProperties draw_command_classify_properties =
        DefaultGPUBufferProperties(TiledRasterMaxIndirectCommandCount, 4 * sizeof(u32), // FIXME
                                   GPUBufferUsage::StorageBuffer | GPUBufferUsage::IndirectBuffer);

    const GPUBufferAccess draw_command_classify_access = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                          VK_ACCESS_2_SHADER_WRITE_BIT};

    light_classify.draw_commands_inner =
        builder.create_buffer(light_classify.pass_handle, "Draw Commands Inner", draw_command_classify_properties,
                              draw_command_classify_access);
    light_classify.draw_commands_outer =
        builder.create_buffer(light_classify.pass_handle, "Draw Commands Outer", draw_command_classify_properties,
                              draw_command_classify_access);

    LightRasterFrameGraphRecord::Raster& light_raster = record.light_raster;

    light_raster.pass_handle = builder.create_render_pass("Rasterize Light Volumes");

    light_raster.command_counters = builder.read_buffer(
        light_raster.pass_handle, light_classify.classification_counters,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    {
        const GPUBufferAccess draw_command_raster_read_access = {VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                                                                 VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT};

        light_raster.draw_commands_inner = builder.read_buffer(
            light_raster.pass_handle, light_classify.draw_commands_inner, draw_command_raster_read_access);
        light_raster.draw_commands_outer = builder.read_buffer(
            light_raster.pass_handle, light_classify.draw_commands_outer, draw_command_raster_read_access);
    }

    {
        GPUTextureAccess tile_depth_access = {
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL};

        light_raster.tile_depth_min =
            builder.read_texture(light_raster.pass_handle, tile_depth_copy.depth_min, tile_depth_access);
        light_raster.tile_depth_max =
            builder.read_texture(light_raster.pass_handle, tile_depth_copy.depth_max, tile_depth_access);
    }

    light_raster.light_list =
        builder.write_buffer(light_raster.pass_handle,
                             tile_depth_copy.light_list_clear,
                             GPUBufferAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                             VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT});

    return record;
}

namespace
{
    void update_depth_copy_pass_descriptor_set(DescriptorWriteHelper&      write_helper,
                                               const TiledRasterResources& resources,
                                               const FrameGraphTexture&    hzb_texture)
    {
        write_helper.append(resources.depth_copy.descriptor_set, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                            hzb_texture.additional_views[0], hzb_texture.image_layout);
    }

    void update_classify_descriptor_set(DescriptorWriteHelper&      write_helper,
                                        const TiledRasterResources& resources,
                                        const FrameGraphBuffer&     classification_counters,
                                        const FrameGraphBuffer&     draw_commands_inner,
                                        const FrameGraphBuffer&     draw_commands_outer,
                                        const StorageBufferAlloc&   proxy_volumes_alloc)
    {
        using namespace Classify;

        write_helper.append(resources.classify.descriptor_set, g_bindings[VertexPositionsMS],
                            resources.vertex_buffer_position.handle);
        write_helper.append(resources.classify.descriptor_set, g_bindings[InnerOuterCounter],
                            classification_counters.handle);
        write_helper.append(resources.classify.descriptor_set, g_bindings[DrawCommandsInner],
                            draw_commands_inner.handle);
        write_helper.append(resources.classify.descriptor_set, g_bindings[DrawCommandsOuter],
                            draw_commands_outer.handle);
        write_helper.append(resources.classify.descriptor_set, g_bindings[ProxyVolumeBuffer],
                            proxy_volumes_alloc.buffer, proxy_volumes_alloc.offset_bytes,
                            proxy_volumes_alloc.size_bytes);
    }

    void update_light_raster_pass_descriptor_sets(DescriptorWriteHelper&      write_helper,
                                                  const TiledRasterResources& resources,
                                                  const FrameGraphTexture&    depth_min,
                                                  const FrameGraphTexture&    depth_max,
                                                  const FrameGraphBuffer&     light_list_buffer,
                                                  const StorageBufferAlloc&   light_volumes_alloc)
    {
        using namespace Raster;

        write_helper.append(resources.light_raster.descriptor_sets[RasterPass::Inner], g_bindings[LightVolumeInstances],
                            light_volumes_alloc.buffer, light_volumes_alloc.offset_bytes,
                            light_volumes_alloc.size_bytes);
        write_helper.append(resources.light_raster.descriptor_sets[RasterPass::Inner], g_bindings[TileDepth],
                            depth_min.default_view_handle, depth_min.image_layout);
        write_helper.append(resources.light_raster.descriptor_sets[RasterPass::Inner],
                            g_bindings[TileVisibleLightIndices], light_list_buffer.handle);
        write_helper.append(resources.light_raster.descriptor_sets[RasterPass::Inner], g_bindings[VertexPositionsMS],
                            resources.vertex_buffer_position.handle);

        write_helper.append(resources.light_raster.descriptor_sets[RasterPass::Outer], g_bindings[LightVolumeInstances],
                            light_volumes_alloc.buffer, light_volumes_alloc.offset_bytes,
                            light_volumes_alloc.size_bytes);
        write_helper.append(resources.light_raster.descriptor_sets[RasterPass::Outer], g_bindings[TileDepth],
                            depth_max.default_view_handle, depth_max.image_layout);
        write_helper.append(resources.light_raster.descriptor_sets[RasterPass::Outer],
                            g_bindings[TileVisibleLightIndices], light_list_buffer.handle);
        write_helper.append(resources.light_raster.descriptor_sets[RasterPass::Outer], g_bindings[VertexPositionsMS],
                            resources.vertex_buffer_position.handle);
    }
} // namespace

void update_tiled_lighting_raster_pass_resources(const FrameGraph::FrameGraph&      frame_graph,
                                                 const FrameGraphResources&         frame_graph_resources,
                                                 const LightRasterFrameGraphRecord& record,
                                                 DescriptorWriteHelper&             write_helper,
                                                 StorageBufferAllocator&            frame_storage_allocator,
                                                 const TiledRasterResources&        resources,
                                                 const TiledLightingFrame&          tiled_lighting_frame)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (tiled_lighting_frame.light_volumes.empty())
        return;

    update_depth_copy_pass_descriptor_set(
        write_helper, resources,
        get_frame_graph_texture(frame_graph_resources, frame_graph, record.tile_depth_copy.hzb_texture));

    const StorageBufferAlloc proxy_volumes_alloc = allocate_storage(
        frame_storage_allocator, tiled_lighting_frame.proxy_volumes.size() * sizeof(ProxyVolumeInstance));

    upload_storage_buffer(frame_storage_allocator, proxy_volumes_alloc, tiled_lighting_frame.proxy_volumes.data());

    update_classify_descriptor_set(
        write_helper, resources,
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.light_classify.classification_counters),
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.light_classify.draw_commands_inner),
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.light_classify.draw_commands_outer),
        proxy_volumes_alloc);

    const StorageBufferAlloc light_volumes_alloc = allocate_storage(
        frame_storage_allocator, tiled_lighting_frame.light_volumes.size() * sizeof(LightVolumeInstance));

    upload_storage_buffer(frame_storage_allocator, light_volumes_alloc, tiled_lighting_frame.light_volumes.data());

    update_light_raster_pass_descriptor_sets(
        write_helper, resources,
        get_frame_graph_texture(frame_graph_resources, frame_graph, record.light_raster.tile_depth_min),
        get_frame_graph_texture(frame_graph_resources, frame_graph, record.light_raster.tile_depth_max),
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.light_raster.light_list),
        light_volumes_alloc);
}

void record_depth_copy(const FrameGraphHelper&                           frame_graph_helper,
                       const LightRasterFrameGraphRecord::TileDepthCopy& pass_record, CommandBuffer& cmdBuffer,
                       const PipelineFactory& pipeline_factory, const TiledRasterResources& resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Tile Depth Copy");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    std::vector<FrameGraphTexture> depth_dsts = {
        get_frame_graph_texture(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.depth_min),
        get_frame_graph_texture(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.depth_max),
    };
    const VkExtent2D depth_extent = {depth_dsts[0].properties.width, depth_dsts[0].properties.height};
    const VkRect2D   pass_rect = default_vk_rect(depth_extent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      get_pipeline(pipeline_factory, resources.depth_copy.pipeline_index));

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.depth_copy.pipeline_layout, 0,
                            1, &resources.depth_copy.descriptor_set, 0, nullptr);

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

        vkCmdPushConstants(cmdBuffer.handle, resources.depth_copy.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(consts), &consts);

        vkCmdDraw(cmdBuffer.handle, 3, 1, 0, 0);

        vkCmdEndRendering(cmdBuffer.handle);
    }

    // Clear buffers
    {
        const u32              clear_value = 0;
        const FrameGraphBuffer light_lists = get_frame_graph_buffer(
            frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.light_list_clear);

        vkCmdFillBuffer(cmdBuffer.handle, light_lists.handle, light_lists.default_view.offset_bytes,
                        light_lists.default_view.size_bytes, clear_value);

        const FrameGraphBuffer counters = get_frame_graph_buffer(
            frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.classification_counters_clear);

        vkCmdFillBuffer(cmdBuffer.handle, counters.handle, counters.default_view.offset_bytes,
                        counters.default_view.size_bytes, clear_value);
    }
}

void record_light_classify_command_buffer(const FrameGraphHelper&                      frame_graph_helper,
                                          const LightRasterFrameGraphRecord::Classify& pass_record,
                                          CommandBuffer&                               cmdBuffer,
                                          const PipelineFactory&                       pipeline_factory,
                                          const TiledLightingFrame&                    tiled_lighting_frame,
                                          const TiledRasterResources&                  resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Classify Light Volumes");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const ProxyMeshAlloc& icosahedron_alloc = resources.proxy_mesh_allocs.front(); // FIXME

    const u32 vertex_offset = icosahedron_alloc.vertex_offset;
    const u32 vertex_count = icosahedron_alloc.vertex_count;
    const u32 light_volumes_count = static_cast<u32>(tiled_lighting_frame.light_volumes.size());

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      get_pipeline(pipeline_factory, resources.classify.pipeline_index));

    ClassifyVolumePushConstants push_constants;
    push_constants.vertex_offset = vertex_offset;
    push_constants.vertex_count = vertex_count;
    push_constants.instance_id_offset = 0;          // FIXME
    push_constants.near_clip_plane_depth_vs = 0.1f; // FIXME copied from near_plane_distance

    vkCmdPushConstants(cmdBuffer.handle, resources.classify.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.classify.pipeline_layout, 0, 1,
                            &resources.classify.descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle, div_round_up(vertex_count, ClassifyVolumeThreadCount), light_volumes_count, 1);
}

void record_light_raster_command_buffer(const FrameGraphHelper&                    frame_graph_helper,
                                        const LightRasterFrameGraphRecord::Raster& pass_record,
                                        CommandBuffer&                             cmdBuffer,
                                        const PipelineFactory&                     pipeline_factory,
                                        const TiledRasterResources::Raster&        resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Rasterize Light Volumes");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const FrameGraphBuffer command_counters = get_frame_graph_buffer(
        frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.command_counters);

    std::array<FrameGraphTexture, RasterPass::Count> depth_buffers = {
        get_frame_graph_texture(frame_graph_helper.resources, frame_graph_helper.frame_graph,
                                pass_record.tile_depth_max),
        get_frame_graph_texture(frame_graph_helper.resources, frame_graph_helper.frame_graph,
                                pass_record.tile_depth_min),
    };
    std::array<FrameGraphBuffer, RasterPass::Count> draw_commands = {
        get_frame_graph_buffer(frame_graph_helper.resources, frame_graph_helper.frame_graph,
                               pass_record.draw_commands_inner),
        get_frame_graph_buffer(frame_graph_helper.resources, frame_graph_helper.frame_graph,
                               pass_record.draw_commands_outer),
    };

    const VkExtent2D depth_extent = {depth_buffers[0].properties.width, depth_buffers[0].properties.height};
    const VkRect2D   pass_rect = default_vk_rect(depth_extent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      get_pipeline(pipeline_factory, resources.pipeline_index));

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

        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipeline_layout, 0, 1,
                                &resources.descriptor_sets[pass_index], 0, nullptr);

        TileLightRasterPushConstants push_constants;
        push_constants.instance_id_offset = 0; // FIXME
        push_constants.tile_count_x = depth_extent.width;

        vkCmdPushConstants(cmdBuffer.handle, resources.pipeline_layout,
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
