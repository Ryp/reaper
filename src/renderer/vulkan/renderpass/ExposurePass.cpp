////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2024 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ExposurePass.h"

#include "FrameGraphPass.h"

#include "renderer/format/PixelFormat.h"
#include "renderer/graph/FrameGraphBuilder.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/PipelineFactory.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "profiling/Scope.h"

#include "renderer/shader/reduce_exposure.share.hlsl"

namespace Reaper
{
namespace
{
    VkPipeline create_exposure_reduce_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                               VkPipelineLayout pipeline_layout)
    {
        // NOTE: You can leave that off when ENABLE_SHARED_FLOAT_ATOMICS is on.
        const VkPipelineShaderStageCreateInfo shader_stage = default_pipeline_shader_stage_create_info(
            VK_SHADER_STAGE_COMPUTE_BIT, shader_modules.reduce_exposure_cs, nullptr,
            VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT);

        return create_compute_pipeline(device, pipeline_layout, shader_stage);
    }

    VkPipeline create_exposure_reduce_tail_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                                    VkPipelineLayout pipeline_layout)
    {
        const VkPipelineShaderStageCreateInfo shader_stage = default_pipeline_shader_stage_create_info(
            VK_SHADER_STAGE_COMPUTE_BIT, shader_modules.reduce_exposure_tail_cs, nullptr,
            VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT);

        return create_compute_pipeline(device, pipeline_layout, shader_stage);
    }
} // namespace

ExposurePassResources create_exposure_pass_resources(VulkanBackend& backend, PipelineFactory& pipeline_factory)
{
    ExposurePassResources resources = {};

    {
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        const VkDescriptorSetLayout descriptor_set_layout = create_descriptor_set_layout(backend.device, bindings);

        const VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                         sizeof(ReduceExposurePassParams)};

        const VkPipelineLayout pipeline_layout = create_pipeline_layout(
            backend.device, std::span(&descriptor_set_layout, 1), std::span(&push_constant_range, 1));

        resources.reduce.descriptor_set_layout = descriptor_set_layout;
        resources.reduce.pipeline_layout = pipeline_layout;
        resources.reduce.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipeline_layout,
                                          .pipeline_creation_function = &create_exposure_reduce_pipeline,
                                      });
    }

    {
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        const VkDescriptorSetLayout descriptor_set_layout = create_descriptor_set_layout(backend.device, bindings);

        const VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                         sizeof(ReduceExposureTailPassParams)};

        const VkPipelineLayout pipeline_layout = create_pipeline_layout(
            backend.device, std::span(&descriptor_set_layout, 1), std::span(&push_constant_range, 1));

        resources.reduce_tail.descriptor_set_layout = descriptor_set_layout;
        resources.reduce_tail.pipeline_layout = pipeline_layout;
        resources.reduce_tail.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipeline_layout,
                                          .pipeline_creation_function = &create_exposure_reduce_tail_pipeline,
                                      });
    }

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.reduce.descriptor_set_layout, 1),
                             std::span(&resources.reduce.descriptor_set, 1));

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.reduce_tail.descriptor_set_layout, 1),
                             std::span(&resources.reduce_tail.descriptor_set, 1));

    return resources;
}

void destroy_exposure_pass_resources(VulkanBackend& backend, const ExposurePassResources& resources)
{
    vkDestroyPipelineLayout(backend.device, resources.reduce.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.reduce.descriptor_set_layout, nullptr);

    vkDestroyPipelineLayout(backend.device, resources.reduce_tail.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.reduce_tail.descriptor_set_layout, nullptr);
}

ExposureFrameGraphRecord create_exposure_pass_record(FrameGraph::Builder&            builder,
                                                     FrameGraph::ResourceUsageHandle scene_hdr_usage_handle,
                                                     VkExtent2D                      render_extent)
{
    ExposureFrameGraphRecord exposure;

    const u32 reduced_width = div_round_up(render_extent.width, ExposureThreadCountX * 2);
    const u32 reduced_height = div_round_up(render_extent.height, ExposureThreadCountY * 2);

    {
        ExposureFrameGraphRecord::Reduce& reduce = exposure.reduce;

        reduce.pass_handle = builder.create_render_pass("Exposure");

        reduce.scene_hdr =
            builder.read_texture(reduce.pass_handle, scene_hdr_usage_handle,
                                 GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

        const GPUTextureProperties average_luminance_properties =
            default_texture_properties(reduced_width, reduced_height, PixelFormat::R16G16_SFLOAT,
                                       GPUTextureUsage::Storage | GPUTextureUsage::Sampled);

        reduce.exposure_texture =
            builder.create_texture(reduce.pass_handle, "Average Log2 Luminance Texture", average_luminance_properties,
                                   GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                    VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL});

        const GPUBufferProperties counter_properties =
            DefaultGPUBufferProperties(1, sizeof(u32), GPUBufferUsage::TransferDst | GPUBufferUsage::StorageBuffer);

        reduce.tail_counter =
            builder.create_buffer(reduce.pass_handle, "Exposure Tail Counter", counter_properties,
                                  GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});
    }

    {
        ExposureFrameGraphRecord::ReduceTail& reduce_tail = exposure.reduce_tail;

        reduce_tail.pass_handle = builder.create_render_pass("Exposure Reduce Tail");

        reduce_tail.exposure_texture =
            builder.read_texture(reduce_tail.pass_handle, exposure.reduce.exposure_texture,
                                 GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

        const GPUBufferProperties average_exposure_properties =
            DefaultGPUBufferProperties(1, sizeof(u32), GPUBufferUsage::StorageBuffer);

        reduce_tail.average_exposure = builder.create_buffer(
            reduce_tail.pass_handle, "Average Log2 Luminance", average_exposure_properties,
            GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

        reduce_tail.tail_counter =
            builder.write_buffer(reduce_tail.pass_handle, exposure.reduce.tail_counter,
                                 GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                 VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT});

        const u32 reduced_tail_width = div_round_up(reduced_width, ExposureThreadCountX * 2);
        const u32 reduced_tail_height = div_round_up(reduced_height, ExposureThreadCountY * 2);

        const GPUTextureProperties exposure_tail_properties =
            default_texture_properties(reduced_tail_width, reduced_tail_height, PixelFormat::R16G16_SFLOAT,
                                       GPUTextureUsage::Storage | GPUTextureUsage::Sampled);

        reduce_tail.exposure_texture_tail = builder.create_texture(
            reduce_tail.pass_handle, "Average Log2 Luminance Texture Tail", exposure_tail_properties,
            GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL});
    }

    return exposure;
}

void update_exposure_pass_descriptor_set(const FrameGraph::FrameGraph&   frame_graph,
                                         const FrameGraphResources&      frame_graph_resources,
                                         const ExposureFrameGraphRecord& record, DescriptorWriteHelper& write_helper,
                                         const ExposurePassResources& resources,
                                         const SamplerResources&      sampler_resources)
{
    {
        const FrameGraphTexture scene_hdr =
            get_frame_graph_texture(frame_graph_resources, frame_graph, record.reduce.scene_hdr);
        const FrameGraphTexture exposure_texture =
            get_frame_graph_texture(frame_graph_resources, frame_graph, record.reduce.exposure_texture);

        write_helper.append(resources.reduce.descriptor_set, 0, sampler_resources.linear_clamp);
        write_helper.append(resources.reduce.descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                            scene_hdr.default_view_handle, scene_hdr.image_layout);
        write_helper.append(resources.reduce.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                            exposure_texture.default_view_handle, exposure_texture.image_layout);
    }

    {
        const FrameGraphTexture exposure_texture =
            get_frame_graph_texture(frame_graph_resources, frame_graph, record.reduce_tail.exposure_texture);
        const FrameGraphBuffer average_exposure =
            get_frame_graph_buffer(frame_graph_resources, frame_graph, record.reduce_tail.average_exposure);
        const FrameGraphBuffer counter =
            get_frame_graph_buffer(frame_graph_resources, frame_graph, record.reduce_tail.tail_counter);
        const FrameGraphTexture exposure_texture_tail =
            get_frame_graph_texture(frame_graph_resources, frame_graph, record.reduce_tail.exposure_texture_tail);

        write_helper.append(resources.reduce_tail.descriptor_set, 0, sampler_resources.linear_clamp);
        write_helper.append(resources.reduce_tail.descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                            exposure_texture.default_view_handle, exposure_texture.image_layout);
        write_helper.append(resources.reduce_tail.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            average_exposure.handle);
        write_helper.append(resources.reduce_tail.descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, counter.handle);
        write_helper.append(resources.reduce_tail.descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                            exposure_texture_tail.default_view_handle, exposure_texture_tail.image_layout);
    }
}

namespace
{
    void record_exposure_reduce_command_buffer(const FrameGraphHelper&                 frame_graph_helper,
                                               const ExposureFrameGraphRecord::Reduce& pass_record,
                                               CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                               const ExposurePassResources& pass_resources)
    {
        const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

        // NOTE: we could create it and clear it once at startup on the CPU,
        // then reset it with compute at the end of each pass, like AMD's SPD code does.
        const FrameGraphBuffer tail_counter = get_frame_graph_buffer(
            frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.tail_counter);

        const u32 clear_value = 0;
        vkCmdFillBuffer(cmdBuffer.handle, tail_counter.handle, 0, VK_WHOLE_SIZE, clear_value);

        const FrameGraphTexture scene_hdr = get_frame_graph_texture(
            frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.scene_hdr);

        const VkExtent2D scene_hdr_extent = {
            // FIXME
            .width = scene_hdr.properties.width,
            .height = scene_hdr.properties.height,
        };

        const ExposurePassResources::Reduce& pipe = pass_resources.reduce;

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                          get_pipeline(pipeline_factory, pipe.pipeline_index));

        ReduceExposurePassParams push_constants;
        push_constants.extent_ts = glm::uvec2(scene_hdr_extent.width, scene_hdr_extent.height);
        push_constants.extent_ts_inv = glm::fvec2(1.f / static_cast<float>(scene_hdr_extent.width),
                                                  1.f / static_cast<float>(scene_hdr_extent.height));

        vkCmdPushConstants(cmdBuffer.handle, pipe.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(push_constants), &push_constants);

        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.pipeline_layout, 0, 1,
                                &pipe.descriptor_set, 0, nullptr);

        vkCmdDispatch(cmdBuffer.handle,
                      div_round_up(scene_hdr_extent.width, ExposureThreadCountX * 2),
                      div_round_up(scene_hdr_extent.height, ExposureThreadCountY * 2),
                      1);
    }

    void record_exposure_reduce_tail_command_buffer(const FrameGraphHelper&                     frame_graph_helper,
                                                    const ExposureFrameGraphRecord::ReduceTail& pass_record,
                                                    CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                                    const ExposurePassResources& pass_resources)
    {
        const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

        const FrameGraphTexture exposure_texture = get_frame_graph_texture(
            frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.exposure_texture);

        const VkExtent2D exposure_extent = {
            .width = exposure_texture.properties.width,
            .height = exposure_texture.properties.height,
        };

        const ExposurePassResources::ReduceTail& pipe = pass_resources.reduce_tail;

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                          get_pipeline(pipeline_factory, pipe.pipeline_index));

        u32 group_size_x = div_round_up(exposure_extent.width, ExposureThreadCountX * 2);
        u32 group_size_y = div_round_up(exposure_extent.height, ExposureThreadCountY * 2);
        u32 group_count = group_size_x * group_size_y;

        ReduceExposureTailPassParams push_constants;
        push_constants.extent_ts = glm::uvec2(exposure_extent.width, exposure_extent.height);
        push_constants.extent_ts_inv = glm::fvec2(1.f / static_cast<float>(exposure_extent.width),
                                                  1.f / static_cast<float>(exposure_extent.height));
        push_constants.tail_extent_ts = glm::uvec2(group_size_x, group_size_y);
        push_constants.last_thread_group_index = group_count - 1;

        vkCmdPushConstants(cmdBuffer.handle, pipe.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(push_constants), &push_constants);

        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.pipeline_layout, 0, 1,
                                &pipe.descriptor_set, 0, nullptr);

        vkCmdDispatch(cmdBuffer.handle, group_size_x, group_size_y, 1);
    }
} // namespace

void record_exposure_command_buffer(const FrameGraphHelper& frame_graph_helper, const ExposureFrameGraphRecord& record,
                                    CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                    const ExposurePassResources& pass_resources)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Compute Exposure");

    record_exposure_reduce_command_buffer(frame_graph_helper, record.reduce, cmdBuffer, pipeline_factory,
                                          pass_resources);

    record_exposure_reduce_tail_command_buffer(frame_graph_helper, record.reduce_tail, cmdBuffer, pipeline_factory,
                                               pass_resources);
}
} // namespace Reaper
