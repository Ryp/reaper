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
#include "renderer/vulkan/PipelineFactory.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"

#include "renderer/PrepareBuckets.h"

#include "mesh/Mesh.h"
#include "mesh/ModelLoader.h"

#include "common/ReaperRoot.h"

#include "debug_geometry/debug_geometry_private.share.hlsl"

static_assert(sizeof(DebugGeometryUserCommand) == DebugGeometryUserCommandSizeBytes, "Invalid HLSL struct size");
static_assert(sizeof(DebugGeometryInstance) == DebugGeometryInstanceSizeBytes, "Invalid HLSL struct size");

namespace Reaper
{
constexpr u32 MaxCPUDebugCommandCount = 1024;
constexpr u32 MaxIndexCount = 1024;
constexpr u32 MaxVertexCount = MaxIndexCount * 3;

namespace
{
    VkPipeline create_build_cmds_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                          VkPipelineLayout pipeline_layout)
    {
        return create_compute_pipeline(device, pipeline_layout, shader_modules.debug_geometry_build_cmds_cs);
    }

    VkPipeline create_draw_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                    VkPipelineLayout pipeline_layout)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                      shader_modules.debug_geometry_draw_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                      shader_modules.debug_geometry_draw_fs),
        };

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

        std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        return create_graphics_pipeline(device, shader_stages, pipeline_properties, dynamic_states);
    }

    void upload_debug_meshes(VulkanBackend& backend, DebugGeometryPassResources& resources,
                             std::span<const Mesh> meshes, std::span<DebugMeshAlloc> debug_mesh_allocs)
    {
        Assert(meshes.size() == debug_mesh_allocs.size());

        for (u32 i = 0; i < static_cast<u32>(meshes.size()); i++)
        {
            const Mesh&     mesh = meshes[i];
            DebugMeshAlloc& debug_mesh_alloc = debug_mesh_allocs[i];

            debug_mesh_alloc.index_offset = resources.index_buffer_offset;
            debug_mesh_alloc.index_count = static_cast<u32>(mesh.indexes.size());
            debug_mesh_alloc.vertex_offset = resources.vertex_buffer_offset;
            debug_mesh_alloc.vertex_count = static_cast<u32>(mesh.positions.size());

            resources.vertex_buffer_offset += debug_mesh_alloc.vertex_count;
            resources.index_buffer_offset += debug_mesh_alloc.index_count;

            upload_buffer_data(backend.device, backend.vma_instance, resources.index_buffer,
                               resources.index_buffer_properties, mesh.indexes.data(),
                               mesh.indexes.size() * sizeof(mesh.indexes[0]), debug_mesh_alloc.index_offset);

            upload_buffer_data(backend.device, backend.vma_instance, resources.vertex_buffer_position,
                               resources.vertex_buffer_properties, mesh.positions.data(),
                               mesh.positions.size() * sizeof(mesh.positions.data()[0]),
                               debug_mesh_alloc.vertex_offset);
        }
    }
} // namespace

DebugGeometryPassResources create_debug_geometry_pass_resources(VulkanBackend&   backend,
                                                                PipelineFactory& pipeline_factory)
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

        resources.build_cmds.descriptor_set_layout = descriptor_set_layout;
        resources.build_cmds.pipeline_layout = pipeline_layout;
        resources.build_cmds.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipeline_layout,
                                          .pipeline_creation_function = &create_build_cmds_pipeline,
                                      });
    }

    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        VkPipelineLayout pipeline_layout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1));

        resources.draw.descriptor_set_layout = descriptor_set_layout;
        resources.draw.pipeline_layout = pipeline_layout;
        resources.draw.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipeline_layout,
                                          .pipeline_creation_function = &create_draw_pipeline,
                                      });
    }

    std::vector<VkDescriptorSetLayout> dset_layouts = {resources.build_cmds.descriptor_set_layout,
                                                       resources.draw.descriptor_set_layout};
    std::vector<VkDescriptorSet>       dsets(dset_layouts.size());

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, dset_layouts, dsets);

    resources.build_cmds_descriptor_set = dsets[0];
    resources.draw_descriptor_set = dsets[1];

    resources.cpu_commands_staging_buffer =
        create_buffer(backend.device, "Debug CPU Command Buffer",
                      DefaultGPUBufferProperties(MaxCPUDebugCommandCount, sizeof(DebugGeometryUserCommand),
                                                 GPUBufferUsage::TransferSrc),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.build_cmds_constants = create_buffer(
        backend.device, "Debug Build Cmds Uniform Buffer",
        DefaultGPUBufferProperties(1, sizeof(DebugGeometryBuildCmdsPassConstants), GPUBufferUsage::UniformBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    {
        resources.index_buffer_offset = 0;
        resources.index_buffer_properties = DefaultGPUBufferProperties(
            MaxIndexCount, sizeof(hlsl_uint), GPUBufferUsage::StorageBuffer | GPUBufferUsage::IndexBuffer);
        resources.index_buffer =
            create_buffer(backend.device, "Debug geometry index buffer", resources.index_buffer_properties,
                          backend.vma_instance, MemUsage::CPU_To_GPU);

        resources.vertex_buffer_offset = 0;
        resources.vertex_buffer_properties =
            DefaultGPUBufferProperties(MaxVertexCount, sizeof(hlsl_float3), GPUBufferUsage::StorageBuffer);
        resources.vertex_buffer_position =
            create_buffer(backend.device, "Debug geometry vertex buffer", resources.vertex_buffer_properties,
                          backend.vma_instance, MemUsage::CPU_To_GPU);

        // Upload mesh data
        std::vector<Mesh> meshes(DebugGeometryTypeCount);
        meshes[DebugGeometryType_Icosphere] = load_obj("res/model/icosahedron.obj");
        meshes[DebugGeometryType_Box] = load_obj("res/model/box.obj");

        resources.proxy_mesh_allocs.resize(meshes.size());

        upload_debug_meshes(backend, resources, meshes, resources.proxy_mesh_allocs);
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
    vmaDestroyBuffer(backend.vma_instance, resources.cpu_commands_staging_buffer.handle,
                     resources.cpu_commands_staging_buffer.allocation);

    vkDestroyPipelineLayout(backend.device, resources.draw.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.draw.descriptor_set_layout, nullptr);

    vkDestroyPipelineLayout(backend.device, resources.build_cmds.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.build_cmds.descriptor_set_layout, nullptr);
}

DebugGeometryStartFrameGraphRecord create_debug_geometry_start_pass_record(FrameGraph::Builder& builder)
{
    DebugGeometryStartFrameGraphRecord record;
    record.pass_handle = builder.create_render_pass("Debug Geometry Start");

    const GPUBufferProperties debug_geometry_counter_properties = DefaultGPUBufferProperties(
        1, sizeof(u32), GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst);

    record.draw_counter = builder.create_buffer(
        record.pass_handle, "Debug Indirect draw counter buffer", debug_geometry_counter_properties,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    const GPUBufferProperties debug_geometry_user_commands_properties =
        DefaultGPUBufferProperties(DebugGeometryCountMax, sizeof(DebugGeometryUserCommand),
                                   GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst);

    record.user_commands_buffer = builder.create_buffer(
        record.pass_handle, "Debug geometry user command buffer", debug_geometry_user_commands_properties,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    return record;
}

DebugGeometryComputeFrameGraphRecord
create_debug_geometry_compute_pass_record(FrameGraph::Builder&            builder,
                                          FrameGraph::ResourceUsageHandle draw_counter_handle,
                                          FrameGraph::ResourceUsageHandle user_commands_buffer_handle)
{
    DebugGeometryComputeFrameGraphRecord record;

    record.pass_handle = builder.create_render_pass("Debug Geometry Build Commands");

    record.draw_counter =
        builder.read_buffer(record.pass_handle, draw_counter_handle,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    record.user_commands_buffer =
        builder.read_buffer(record.pass_handle, user_commands_buffer_handle,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    const GPUBufferProperties debug_geometry_command_properties =
        DefaultGPUBufferProperties(DebugGeometryCountMax, sizeof(VkDrawIndexedIndirectCommand),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer);

    record.draw_commands = builder.create_buffer(
        record.pass_handle, "Debug Indirect draw command buffer", debug_geometry_command_properties,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    const GPUBufferProperties debug_geometry_instance_properties =
        DefaultGPUBufferProperties(DebugGeometryCountMax, sizeof(DebugGeometryInstance), GPUBufferUsage::StorageBuffer);

    record.instance_buffer =
        builder.create_buffer(record.pass_handle, "Debug geometry instance buffer", debug_geometry_instance_properties,
                              GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    return record;
}

DebugGeometryDrawFrameGraphRecord
create_debug_geometry_draw_pass_record(FrameGraph::Builder&                        builder,
                                       const DebugGeometryComputeFrameGraphRecord& debug_geometry_build_cmds,
                                       FrameGraph::ResourceUsageHandle             draw_counter_handle,
                                       FrameGraph::ResourceUsageHandle             scene_hdr_usage_handle,
                                       FrameGraph::ResourceUsageHandle             scene_depth_usage_handle)
{
    DebugGeometryDrawFrameGraphRecord record;
    record.pass_handle = builder.create_render_pass("Debug Geometry Draw");

    record.scene_hdr = builder.write_texture(record.pass_handle,
                                             scene_hdr_usage_handle,
                                             GPUTextureAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});

    record.scene_depth = builder.read_texture(
        record.pass_handle, scene_depth_usage_handle,
        GPUTextureAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                         VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL});

    record.draw_counter = builder.read_buffer(
        record.pass_handle, draw_counter_handle,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    record.draw_commands = builder.read_buffer(
        record.pass_handle, debug_geometry_build_cmds.draw_commands,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    record.instance_buffer =
        builder.read_buffer(record.pass_handle, debug_geometry_build_cmds.instance_buffer,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    return record;
}

void update_debug_geometry_start_resources(VulkanBackend& backend, const PreparedData& prepared,
                                           const DebugGeometryPassResources& resources)
{
    const u32 cpu_command_count = static_cast<u32>(prepared.debug_draw_commands.size());

    if (cpu_command_count > 0)
    {
        upload_buffer_data_deprecated(backend.device, backend.vma_instance, resources.cpu_commands_staging_buffer,
                                      prepared.debug_draw_commands.data(),
                                      cpu_command_count * sizeof(prepared.debug_draw_commands[0]));
    }
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

void record_debug_geometry_start_command_buffer(const FrameGraphHelper&                   frame_graph_helper,
                                                const DebugGeometryStartFrameGraphRecord& pass_record,
                                                CommandBuffer&                            cmdBuffer,
                                                const PreparedData&                       prepared,
                                                const DebugGeometryPassResources&         resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Debug Geometry Start");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const u32 cpu_command_count = static_cast<u32>(prepared.debug_draw_commands.size());

    FrameGraphBuffer draw_counter =
        get_frame_graph_buffer(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.draw_counter);

    vkCmdFillBuffer(cmdBuffer.handle, draw_counter.handle, draw_counter.default_view.offset_bytes,
                    draw_counter.default_view.size_bytes, cpu_command_count);

    FrameGraphBuffer user_commands_buffer = get_frame_graph_buffer(
        frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.user_commands_buffer);

    if (cpu_command_count > 0)
    {
        const VkBufferCopy2 region = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .pNext = nullptr,
            .srcOffset = 0,
            .dstOffset = 0,
            .size = cpu_command_count * resources.cpu_commands_staging_buffer.properties_deprecated.element_size_bytes,
        };

        const VkCopyBufferInfo2 copy = {
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext = nullptr,
            .srcBuffer = resources.cpu_commands_staging_buffer.handle,
            .dstBuffer = user_commands_buffer.handle,
            .regionCount = 1,
            .pRegions = &region,
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &copy);
    }
}

void record_debug_geometry_build_cmds_command_buffer(const FrameGraphHelper&                     frame_graph_helper,
                                                     const DebugGeometryComputeFrameGraphRecord& pass_record,
                                                     CommandBuffer&                              cmdBuffer,
                                                     const PipelineFactory&                      pipeline_factory,
                                                     const DebugGeometryPassResources&           resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Debug Geometry Build Commands");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      get_pipeline(pipeline_factory, resources.build_cmds.pipeline_index));

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.build_cmds.pipeline_layout, 0,
                            1, &resources.build_cmds_descriptor_set, 0, nullptr);

    // Since we know the actual debug geometry count on the GPU, we can use indirect dispatch instead.
    // Doing it this way wastes a bit of resources, but it's negligible to do this ATM for debug builds.
    vkCmdDispatch(cmdBuffer.handle, div_round_up(DebugGeometryCountMax, DebugGeometryBuildCmdsThreadCount), 1, 1);
}

void record_debug_geometry_draw_command_buffer(const FrameGraphHelper&                  frame_graph_helper,
                                               const DebugGeometryDrawFrameGraphRecord& pass_record,
                                               CommandBuffer&                           cmdBuffer,
                                               const PipelineFactory&                   pipeline_factory,
                                               const DebugGeometryPassResources&        resources)
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

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      get_pipeline(pipeline_factory, resources.draw.pipeline_index));

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    VkRenderingAttachmentInfo color_attachment =
        default_rendering_attachment_info(hdr_buffer.default_view_handle, hdr_buffer.image_layout);
    VkRenderingAttachmentInfo depth_attachment =
        default_rendering_attachment_info(depth_texture.default_view_handle, depth_texture.image_layout);

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, &color_attachment, &depth_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.draw.pipeline_layout, 0, 1,
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
