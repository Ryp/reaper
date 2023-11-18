////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "DebugGeometryRenderPass.h"

#include "Constants.h"
#include "ForwardPassConstants.h"
#include "FrameGraphPass.h"

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
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"

#include "renderer/PrepareBuckets.h"

#include "mesh/Mesh.h"
#include "mesh/ModelLoader.h"

#include "common/ReaperRoot.h"

#include "debug_geometry/debug_geometry_private.share.hlsl"

static_assert(sizeof(DebugGeometryUserCommand) == DebugGeometryUserCommandSizeBytes, "Invalid HLSL struct size");
static_assert(sizeof(DebugGeometryCommand) == DebugGeometryCommandSizeBytes, "Invalid HLSL struct size");
static_assert(sizeof(DebugGeometryInstance) == DebugGeometryInstanceSizeBytes, "Invalid HLSL struct size");

namespace Reaper
{
constexpr u32 MaxIndexCount = 1024;
constexpr u32 MaxVertexCount = MaxIndexCount * 3;

DebugGeometryPassResources create_debug_geometry_pass_resources(VulkanBackend&       backend,
                                                                const ShaderModules& shader_modules)
{
    DebugGeometryPassResources resources = {};

    {
        std::vector<VkDescriptorSetLayoutBinding> layout_bindings = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout = create_descriptor_set_layout(backend.device, layout_bindings);

        VkPipelineLayout pipeline_layout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipeline_layout, shader_modules.debug_geometry_build_cmds_cs);

        resources.build_cmds_descriptor_set_layout = descriptor_set_layout;
        resources.build_cmds_pipeline_layout = pipeline_layout;
        resources.build_cmds_pipeline = pipeline;
    }

    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                      shader_modules.debug_geometry_draw_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                      shader_modules.debug_geometry_draw_fs),
        };

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        VkPipelineLayout pipeline_layout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1));

        VkPipelineColorBlendAttachmentState blend_attachment_state = default_pipeline_color_blend_attachment_state();

        const VkFormat color_format = PixelFormatToVulkan(ForwardHDRColorFormat);
        const VkFormat depth_format = PixelFormatToVulkan(MainPassDepthFormat);

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
        pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthWriteEnable = VK_FALSE;
        pipeline_properties.depth_stencil.depthCompareOp =
            MainPassUseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
        pipeline_properties.raster.polygonMode = VK_POLYGON_MODE_LINE;
        pipeline_properties.raster.cullMode = VK_CULL_MODE_NONE;
        pipeline_properties.input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipeline_properties.blend_state.attachmentCount = 1;
        pipeline_properties.blend_state.pAttachments = &blend_attachment_state;
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.colorAttachmentCount = 1;
        pipeline_properties.pipeline_rendering.pColorAttachmentFormats = &color_format;
        pipeline_properties.pipeline_rendering.depthAttachmentFormat = depth_format;

        VkPipeline pipeline = create_graphics_pipeline(backend.device, shader_stages, pipeline_properties);

        resources.draw_descriptor_set_layout = descriptor_set_layout;
        resources.draw_pipeline_layout = pipeline_layout;
        resources.draw_pipeline = pipeline;
    }

    std::vector<VkDescriptorSetLayout> dset_layouts = {resources.build_cmds_descriptor_set_layout,
                                                       resources.draw_descriptor_set_layout};
    std::vector<VkDescriptorSet>       dsets(dset_layouts.size());

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, dset_layouts, dsets);

    resources.build_cmds_descriptor_set = dsets[0];
    resources.draw_descriptor_set = dsets[1];

    resources.build_cmds_constants = create_buffer(
        backend.device, "Debug Build Cmds Uniform Buffer",
        DefaultGPUBufferProperties(1, sizeof(DebugGeometryBuildCmdsPassConstants), GPUBufferUsage::UniformBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    {
        resources.index_buffer_offset = 0;
        resources.vertex_buffer_offset = 0;

        const GPUBufferProperties index_properties = DefaultGPUBufferProperties(
            MaxIndexCount, sizeof(hlsl_uint), GPUBufferUsage::StorageBuffer | GPUBufferUsage::IndexBuffer);

        const GPUBufferProperties vertex_properties =
            DefaultGPUBufferProperties(MaxVertexCount, sizeof(hlsl_float3), GPUBufferUsage::StorageBuffer);

        resources.index_buffer = create_buffer(backend.device, "Debug geometry index buffer", index_properties,
                                               backend.vma_instance, MemUsage::CPU_To_GPU);

        resources.vertex_buffer_position = create_buffer(backend.device, "Debug geometry vertex buffer",
                                                         vertex_properties, backend.vma_instance, MemUsage::CPU_To_GPU);

        std::vector<Mesh> meshes;
        const Mesh&       icosahedron = meshes.emplace_back(load_obj("res/model/icosahedron.obj"));

        DebugMeshAlloc& icosahedron_alloc = resources.proxy_mesh_allocs.emplace_back();
        icosahedron_alloc.index_offset = resources.index_buffer_offset;
        icosahedron_alloc.index_count = static_cast<u32>(icosahedron.indexes.size());
        icosahedron_alloc.vertex_offset = resources.vertex_buffer_offset;
        icosahedron_alloc.vertex_count = static_cast<u32>(icosahedron.positions.size());

        resources.vertex_buffer_offset += icosahedron_alloc.vertex_count;
        resources.index_buffer_offset += icosahedron_alloc.index_count;

        upload_buffer_data(backend.device, backend.vma_instance, resources.vertex_buffer_position, vertex_properties,
                           icosahedron.positions.data(),
                           icosahedron.positions.size() * sizeof(icosahedron.positions.data()[0]),
                           icosahedron_alloc.vertex_offset);

        upload_buffer_data(backend.device, backend.vma_instance, resources.index_buffer, index_properties,
                           icosahedron.indexes.data(), icosahedron.indexes.size() * sizeof(icosahedron.indexes[0]),
                           icosahedron_alloc.index_offset);
    }

    return resources;
}

void destroy_debug_geometry_pass_resources(VulkanBackend& backend, DebugGeometryPassResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.vertex_buffer_position.handle,
                     resources.vertex_buffer_position.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.index_buffer.handle, resources.index_buffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.build_cmds_constants.handle,
                     resources.build_cmds_constants.allocation);

    vkDestroyPipeline(backend.device, resources.draw_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.draw_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.draw_descriptor_set_layout, nullptr);

    vkDestroyPipeline(backend.device, resources.build_cmds_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.build_cmds_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.build_cmds_descriptor_set_layout, nullptr);
}

DebugGeometryClearFrameGraphRecord create_debug_geometry_clear_pass_record(FrameGraph::Builder& builder)
{
    DebugGeometryClearFrameGraphRecord debug_geometry_clear;
    debug_geometry_clear.pass_handle = builder.create_render_pass("Debug Geometry Clear");

    const GPUBufferProperties debug_geometry_counter_properties = DefaultGPUBufferProperties(
        1, sizeof(u32), GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst);

    debug_geometry_clear.draw_counter = builder.create_buffer(
        debug_geometry_clear.pass_handle, "Debug Indirect draw counter buffer", debug_geometry_counter_properties,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    const GPUBufferProperties debug_geometry_user_commands_properties = DefaultGPUBufferProperties(
        DebugGeometryCountMax, sizeof(DebugGeometryUserCommand), GPUBufferUsage::StorageBuffer);

    // Technically we shouldn't create an usage here, the first client of the debug geometry API should call
    // create_buffer() with the right data. But it makes it slightly simpler this way for the user API so I'm taking
    // the trade-off and paying for an extra useless barrier.
    debug_geometry_clear.user_commands_buffer = builder.create_buffer(
        debug_geometry_clear.pass_handle, "Debug geometry user command buffer", debug_geometry_user_commands_properties,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    return debug_geometry_clear;
}

DebugGeometryComputeFrameGraphRecord
create_debug_geometry_compute_pass_record(FrameGraph::Builder&                      builder,
                                          const DebugGeometryClearFrameGraphRecord& debug_geometry_clear)
{
    DebugGeometryComputeFrameGraphRecord debug_geometry_build_cmds;

    debug_geometry_build_cmds.pass_handle = builder.create_render_pass("Debug Geometry Build Commands");

    debug_geometry_build_cmds.draw_counter =
        builder.read_buffer(debug_geometry_build_cmds.pass_handle, debug_geometry_clear.draw_counter,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    debug_geometry_build_cmds.user_commands_buffer =
        builder.read_buffer(debug_geometry_build_cmds.pass_handle, debug_geometry_clear.user_commands_buffer,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    const GPUBufferProperties debug_geometry_command_properties =
        DefaultGPUBufferProperties(DebugGeometryCountMax, sizeof(VkDrawIndexedIndirectCommand),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer);

    debug_geometry_build_cmds.draw_commands = builder.create_buffer(
        debug_geometry_build_cmds.pass_handle, "Debug Indirect draw command buffer", debug_geometry_command_properties,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    const GPUBufferProperties debug_geometry_instance_properties =
        DefaultGPUBufferProperties(DebugGeometryCountMax, sizeof(DebugGeometryInstance), GPUBufferUsage::StorageBuffer);

    debug_geometry_build_cmds.instance_buffer = builder.create_buffer(
        debug_geometry_build_cmds.pass_handle, "Debug geometry instance buffer", debug_geometry_instance_properties,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    return debug_geometry_build_cmds;
}

DebugGeometryDrawFrameGraphRecord
create_debug_geometry_draw_pass_record(FrameGraph::Builder&                        builder,
                                       const DebugGeometryClearFrameGraphRecord&   debug_geometry_clear,
                                       const DebugGeometryComputeFrameGraphRecord& debug_geometry_build_cmds,
                                       FrameGraph::ResourceUsageHandle             scene_hdr_usage_handle,
                                       FrameGraph::ResourceUsageHandle             scene_depth_usage_handle)
{
    DebugGeometryDrawFrameGraphRecord debug_geometry_draw;
    debug_geometry_draw.pass_handle = builder.create_render_pass("Debug Geometry Draw");

    debug_geometry_draw.scene_hdr = builder.write_texture(
        debug_geometry_draw.pass_handle,
        scene_hdr_usage_handle,
        GPUTextureAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});

    debug_geometry_draw.scene_depth = builder.read_texture(
        debug_geometry_draw.pass_handle, scene_depth_usage_handle,
        GPUTextureAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                         VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL});

    debug_geometry_draw.draw_counter = builder.read_buffer(
        debug_geometry_draw.pass_handle, debug_geometry_clear.draw_counter,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    debug_geometry_draw.draw_commands = builder.read_buffer(
        debug_geometry_draw.pass_handle, debug_geometry_build_cmds.draw_commands,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    debug_geometry_draw.instance_buffer =
        builder.read_buffer(debug_geometry_draw.pass_handle, debug_geometry_build_cmds.instance_buffer,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    return debug_geometry_draw;
}

void update_debug_geometry_build_cmds_pass_resources(VulkanBackend& backend, const FrameGraph::FrameGraph& frame_graph,
                                                     const FrameGraphResources&                  frame_graph_resources,
                                                     const DebugGeometryComputeFrameGraphRecord& record,
                                                     DescriptorWriteHelper& write_helper, const PreparedData& prepared,
                                                     const DebugGeometryPassResources& resources)
{
    DebugGeometryBuildCmdsPassConstants constants;
    constants.main_camera_ws_to_cs = prepared.forward_pass_constants.ws_to_cs_matrix;

    Assert(resources.proxy_mesh_allocs.size() == DebugGeometryTypeCount);

    for (u32 i = 0; i < DebugGeometryTypeCount; i++)
    {
        const DebugMeshAlloc&   in_alloc = resources.proxy_mesh_allocs[i];
        DebugGeometryMeshAlloc& out_alloc = constants.debug_geometry_allocs[i];

        out_alloc.index_offset = in_alloc.index_offset;
        out_alloc.index_count = in_alloc.index_count;
        out_alloc.vertex_offset = in_alloc.vertex_offset;
    }

    upload_buffer_data_deprecated(backend.device, backend.vma_instance, resources.build_cmds_constants, &constants,
                                  sizeof(constants));

    const FrameGraphBuffer draw_counter =
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.draw_counter);
    const FrameGraphBuffer user_commands =
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.user_commands_buffer);
    const FrameGraphBuffer draw_commands =
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.draw_commands);
    const FrameGraphBuffer instance_buffer =
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.instance_buffer);

    write_helper.append(resources.build_cmds_descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        resources.build_cmds_constants.handle);
    write_helper.append(resources.build_cmds_descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, draw_counter.handle);
    write_helper.append(resources.build_cmds_descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        user_commands.handle);
    write_helper.append(resources.build_cmds_descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        draw_commands.handle);
    write_helper.append(resources.build_cmds_descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        instance_buffer.handle);
}

void record_debug_geometry_build_cmds_command_buffer(const FrameGraphHelper&                     frame_graph_helper,
                                                     const DebugGeometryComputeFrameGraphRecord& pass_record,
                                                     CommandBuffer&                              cmdBuffer,
                                                     const DebugGeometryPassResources&           resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Debug Geometry Build Commands");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.build_cmds_pipeline);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.build_cmds_pipeline_layout, 0,
                            1, &resources.build_cmds_descriptor_set, 0, nullptr);

    // Since we know the actual debug geometry count on the GPU, we can use indirect dispatch instead.
    // Doing it this way wastes a bit of resources, but it's negligible to do this ATM for debug builds.
    vkCmdDispatch(cmdBuffer.handle, div_round_up(DebugGeometryCountMax, DebugGeometryBuildCmdsThreadCount), 1, 1);
}

void update_debug_geometry_draw_pass_descriptor_sets(const FrameGraph::FrameGraph&            frame_graph,
                                                     const FrameGraphResources&               frame_graph_resources,
                                                     const DebugGeometryDrawFrameGraphRecord& record,
                                                     DescriptorWriteHelper&                   write_helper,
                                                     const DebugGeometryPassResources&        resources)
{
    const FrameGraphBuffer instance_buffer =
        get_frame_graph_buffer(frame_graph_resources, frame_graph, record.instance_buffer);

    write_helper.append(resources.draw_descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        resources.vertex_buffer_position.handle);
    write_helper.append(resources.draw_descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, instance_buffer.handle);
}

void record_debug_geometry_draw_command_buffer(const FrameGraphHelper&                  frame_graph_helper,
                                               const DebugGeometryDrawFrameGraphRecord& pass_record,
                                               CommandBuffer& cmdBuffer, const DebugGeometryPassResources& resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Debug Geometry Draw");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const FrameGraphTexture hdr_buffer =
        get_frame_graph_texture(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.scene_hdr);
    const FrameGraphTexture depth_texture =
        get_frame_graph_texture(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.scene_depth);
    const FrameGraphBuffer draw_counter =
        get_frame_graph_buffer(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.draw_counter);
    const FrameGraphBuffer draw_commands =
        get_frame_graph_buffer(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.draw_commands);

    const VkExtent2D depth_extent = {depth_texture.properties.width, depth_texture.properties.height};
    const VkRect2D   pass_rect = default_vk_rect(depth_extent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.draw_pipeline);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    VkRenderingAttachmentInfo color_attachment =
        default_rendering_attachment_info(hdr_buffer.default_view_handle, hdr_buffer.image_layout);
    VkRenderingAttachmentInfo depth_attachment =
        default_rendering_attachment_info(depth_texture.default_view_handle, depth_texture.image_layout);

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, &color_attachment, &depth_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.draw_pipeline_layout, 0, 1,
                            &resources.draw_descriptor_set, 0, nullptr);

    const u64 index_buffer_offset = 0;
    vkCmdBindIndexBuffer(cmdBuffer.handle, resources.index_buffer.handle, index_buffer_offset, VK_INDEX_TYPE_UINT32);

    const u32 command_buffer_offset = 0;
    const u32 counter_buffer_offset = 0;
    vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, draw_commands.handle, command_buffer_offset, draw_counter.handle,
                                  counter_buffer_offset, DebugGeometryCountMax,
                                  draw_commands.properties.element_size_bytes);

    vkCmdEndRendering(cmdBuffer.handle);
}
} // namespace Reaper
