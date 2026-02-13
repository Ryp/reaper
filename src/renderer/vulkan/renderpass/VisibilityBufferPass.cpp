////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "VisibilityBufferPass.h"

#include "FrameGraphPass.h"
#include "GBufferPassConstants.h"
#include "MeshletCulling.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/graph/FrameGraphBuilder.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
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
#include "renderer/vulkan/renderpass/VisibilityBufferConstants.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "renderer/shader/vis_buffer/fill_gbuffer.share.hlsl"

#include <vector>

namespace Reaper
{
static const u32 MSAASamples = 4;

// NOTE: origin is in the top-left corner
static const std::array<VkSampleLocationEXT, MSAASamples> MSAA4XGridSampleLocations = {
    VkSampleLocationEXT{0.25f, 0.25f},
    VkSampleLocationEXT{0.75f, 0.25f},
    VkSampleLocationEXT{0.25f, 0.75f},
    VkSampleLocationEXT{0.75f, 0.75f},
};

namespace Render
{
    enum BindingIndex
    {
        instance_params,
        visible_meshlets,
        buffer_position_ms,
        _count,
    };

    std::array<DescriptorBinding, BindingIndex::_count> g_bindings = {
        DescriptorBinding{
            .slot = 0, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_VERTEX_BIT},
        {.slot = 1, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_VERTEX_BIT},
        {.slot = 2, .count = 1, .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stage_mask = VK_SHADER_STAGE_VERTEX_BIT},
    };
} // namespace Render

namespace FillGBuffer
{
    enum BindingIndex
    {
        VisBuffer,
        VisBufferDepthMS, // MSAA-only
        ResolvedDepth,    // MSAA-only
        GBuffer0,
        GBuffer1,
        instance_params,
        visible_index_buffer,
        buffer_position_ms,
        buffer_attributes,
        visible_meshlets,
        mesh_materials,
        diffuse_map_sampler,
        material_maps,
        _count,
    };

    std::array<DescriptorBinding, BindingIndex::_count> g_bindings = {
        DescriptorBinding{.slot = Slot_VisBuffer,
                          .count = 1,
                          .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                          .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = Slot_VisBufferDepthMS,
         .count = 1,
         .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = Slot_ResolvedDepth,
         .count = 1,
         .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = Slot_GBuffer0,
         .count = 1,
         .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = Slot_GBuffer1,
         .count = 1,
         .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = Slot_instance_params,
         .count = 1,
         .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = Slot_visible_index_buffer,
         .count = 1,
         .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = Slot_buffer_position_ms,
         .count = 1,
         .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = Slot_buffer_attributes,
         .count = 1,
         .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = Slot_visible_meshlets,
         .count = 1,
         .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = Slot_mesh_materials,
         .count = 1,
         .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = Slot_diffuse_map_sampler,
         .count = 1,
         .type = VK_DESCRIPTOR_TYPE_SAMPLER,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
        {.slot = Slot_material_maps,
         .count = MaterialTextureMaxCount,
         .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         .stage_mask = VK_SHADER_STAGE_COMPUTE_BIT},
    };
} // namespace FillGBuffer

namespace
{
    VkPipeline create_vis_buffer_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                          VkPipelineLayout pipeline_layout, bool enable_msaa)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, shader_modules.vis_buffer_raster_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                      shader_modules.vis_buffer_raster_fs),
        };

        std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_state = {
            default_pipeline_color_blend_attachment_state()};

        std::vector<VkFormat> color_formats = {PixelFormatToVulkan(VisibilityBufferFormat)};
        const VkFormat        depth_format = PixelFormatToVulkan(MainPassDepthFormat);

        VkPipelineCreationFeedback              feedback = {};
        std::vector<VkPipelineCreationFeedback> feedback_stages(shader_stages.size());
        VkPipelineCreationFeedbackCreateInfo    feedback_info = {
               .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO,
               .pNext = nullptr,
               .pPipelineCreationFeedback = &feedback,
               .pipelineStageCreationFeedbackCount = static_cast<u32>(feedback_stages.size()),
               .pPipelineStageCreationFeedbacks = feedback_stages.data(),
        };

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties(&feedback_info);
        pipeline_properties.input_assembly.primitiveRestartEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthWriteEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthCompareOp =
            MainPassUseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
        pipeline_properties.blend_state.attachmentCount = static_cast<u32>(blend_attachment_state.size());
        pipeline_properties.blend_state.pAttachments = blend_attachment_state.data();
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.colorAttachmentCount = static_cast<u32>(color_formats.size());
        pipeline_properties.pipeline_rendering.pColorAttachmentFormats = color_formats.data();
        pipeline_properties.pipeline_rendering.depthAttachmentFormat = depth_format;

        const VkPipelineSampleLocationsStateCreateInfoEXT sample_location_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT,
            .pNext = nullptr,
            .sampleLocationsEnable = true,
            .sampleLocationsInfo =
                {
                    .sType = VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT,
                    .pNext = nullptr,
                    .sampleLocationsPerPixel = SampleCountToVulkan(MSAASamples),
                    .sampleLocationGridSize = VkExtent2D{1, 1},
                    .sampleLocationsCount = static_cast<u32>(MSAA4XGridSampleLocations.size()),
                    .pSampleLocations = MSAA4XGridSampleLocations.data(),
                },
        };

        if (enable_msaa)
        {
            pipeline_properties.multisample.rasterizationSamples = SampleCountToVulkan(MSAASamples);
            pipeline_properties.multisample.pNext = &sample_location_info;
        }

        std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipeline pipeline = create_graphics_pipeline(device, shader_stages, pipeline_properties, dynamic_states);

        // log_debug(root, "- total time = {}ms, vs = {}ms, fs = {}ms", feedback.duration / 1000,
        //           feedback_stages[0].duration / 1000, feedback_stages[1].duration / 1000);

        return pipeline;
    }

    VkPipeline create_vis_buffer_pipeline_non_msaa(VkDevice device, const ShaderModules& shader_modules,
                                                   VkPipelineLayout pipeline_layout)
    {
        return create_vis_buffer_pipeline(device, shader_modules, pipeline_layout, false);
    }

    VkPipeline create_vis_buffer_pipeline_msaa(VkDevice device, const ShaderModules& shader_modules,
                                               VkPipelineLayout pipeline_layout)
    {
        return create_vis_buffer_pipeline(device, shader_modules, pipeline_layout, true);
    }

    VkPipeline create_vis_buffer_fill_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                               VkPipelineLayout pipeline_layout)
    {
        return create_compute_pipeline(device, pipeline_layout, shader_modules.vis_fill_gbuffer_cs);
    }

    VkPipeline create_vis_buffer_fill_msaa_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                                    VkPipelineLayout pipeline_layout)
    {
        return create_compute_pipeline(device, pipeline_layout, shader_modules.vis_fill_gbuffer_msaa_cs);
    }

    VkPipeline create_vis_buffer_fill_msaa_with_resolve_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                                                 VkPipelineLayout pipeline_layout)
    {
        return create_compute_pipeline(device, pipeline_layout,
                                       shader_modules.vis_fill_gbuffer_msaa_with_depth_resolve_cs);
    }

    VkPipeline create_legacy_depth_resolve_pipeline(VkDevice device, const ShaderModules& shader_modules,
                                                    VkPipelineLayout pipeline_layout)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                      shader_modules.fullscreen_triangle_vs),
            default_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                      shader_modules.vis_resolve_depth_legacy_fs),
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
} // namespace

VisibilityBufferPassResources create_vis_buffer_pass_resources(VulkanBackend&   backend,
                                                               PipelineFactory& pipeline_factory)
{
    VisibilityBufferPassResources resources = {};

    {
        using namespace Render;

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings(g_bindings.size());
        fill_layout_bindings(layout_bindings, g_bindings);

        const VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, layout_bindings);

        resources.pipe.desc_set_layout = descriptor_set_layout;

        resources.pipe.pipelineLayout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1));

        Assert(backend.physical_device.graphics_queue_family_index
               == backend.physical_device.present_queue_family_index);

        resources.pipe.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = resources.pipe.pipelineLayout,
                                          .pipeline_creation_function = &create_vis_buffer_pipeline_non_msaa,
                                      });
    }

    {
        using namespace Render;

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings(g_bindings.size());
        fill_layout_bindings(layout_bindings, g_bindings);

        const VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, layout_bindings);

        resources.pipe_msaa.desc_set_layout = descriptor_set_layout;

        resources.pipe_msaa.pipelineLayout =
            create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1));

        Assert(backend.physical_device.graphics_queue_family_index
               == backend.physical_device.present_queue_family_index);

        resources.pipe_msaa.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = resources.pipe_msaa.pipelineLayout,
                                          .pipeline_creation_function = &create_vis_buffer_pipeline_msaa,
                                      });
    }

    {
        using namespace FillGBuffer;

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings(g_bindings.size());
        fill_layout_bindings(layout_bindings, g_bindings);

        std::vector<VkDescriptorBindingFlags> binding_flags(layout_bindings.size(), VK_FLAGS_NONE);
        binding_flags.back() = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, layout_bindings, binding_flags);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(FillGBufferPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1),
                                                                 std::span(&pushConstantRange, 1));

        resources.fill_pipe.desc_set_layout = descriptor_set_layout;
        resources.fill_pipe.pipelineLayout = pipelineLayout;
        resources.fill_pipe.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipelineLayout,
                                          .pipeline_creation_function = &create_vis_buffer_fill_pipeline,
                                      });
    }

    {
        using namespace FillGBuffer;

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings(g_bindings.size());
        fill_layout_bindings(layout_bindings, g_bindings);

        std::vector<VkDescriptorBindingFlags> binding_flags(layout_bindings.size(), VK_FLAGS_NONE);
        binding_flags.back() = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, layout_bindings, binding_flags);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(FillGBufferPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1),
                                                                 std::span(&pushConstantRange, 1));

        resources.fill_pipe_msaa.desc_set_layout = descriptor_set_layout;
        resources.fill_pipe_msaa.pipelineLayout = pipelineLayout;
        resources.fill_pipe_msaa.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipelineLayout,
                                          .pipeline_creation_function = &create_vis_buffer_fill_msaa_pipeline,
                                      });
    }

    {
        using namespace FillGBuffer;

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings(g_bindings.size());
        fill_layout_bindings(layout_bindings, g_bindings);

        std::vector<VkDescriptorBindingFlags> binding_flags(layout_bindings.size(), VK_FLAGS_NONE);
        binding_flags.back() = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, layout_bindings, binding_flags);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(FillGBufferPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1),
                                                                 std::span(&pushConstantRange, 1));

        resources.fill_pipe_msaa_with_resolve.desc_set_layout = descriptor_set_layout;
        resources.fill_pipe_msaa_with_resolve.pipelineLayout = pipelineLayout;
        resources.fill_pipe_msaa_with_resolve.pipeline_index = register_pipeline_creator(
            pipeline_factory,
            PipelineCreator{
                .pipeline_layout = pipelineLayout,
                .pipeline_creation_function = &create_vis_buffer_fill_msaa_with_resolve_pipeline,
            });
    }

    {
        std::vector<VkDescriptorSetLayoutBinding> layout_bindings = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        std::vector<VkDescriptorBindingFlags> binding_flags(layout_bindings.size());

        VkDescriptorSetLayout descriptor_set_layout = create_descriptor_set_layout(backend.device, layout_bindings);

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, std::span(&descriptor_set_layout, 1));

        resources.legacy_resolve_pipe.desc_set_layout = descriptor_set_layout;
        resources.legacy_resolve_pipe.pipelineLayout = pipelineLayout;
        resources.legacy_resolve_pipe.pipeline_index =
            register_pipeline_creator(pipeline_factory,
                                      PipelineCreator{
                                          .pipeline_layout = pipelineLayout,
                                          .pipeline_creation_function = &create_legacy_depth_resolve_pipeline,
                                      });
    }

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.pipe.desc_set_layout, 1), std::span(&resources.descriptor_set, 1));

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.fill_pipe.desc_set_layout, 1),
                             std::span(&resources.descriptor_set_fill, 1));

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             std::span(&resources.legacy_resolve_pipe.desc_set_layout, 1),
                             std::span(&resources.descriptor_set_legacy_resolve, 1));

    return resources;
}

void destroy_vis_buffer_pass_resources(VulkanBackend& backend, VisibilityBufferPassResources& resources)
{
    vkDestroyPipelineLayout(backend.device, resources.pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.desc_set_layout, nullptr);

    vkDestroyPipelineLayout(backend.device, resources.pipe_msaa.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe_msaa.desc_set_layout, nullptr);

    vkDestroyPipelineLayout(backend.device, resources.fill_pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.fill_pipe.desc_set_layout, nullptr);

    vkDestroyPipelineLayout(backend.device, resources.fill_pipe_msaa.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.fill_pipe_msaa.desc_set_layout, nullptr);

    vkDestroyPipelineLayout(backend.device, resources.fill_pipe_msaa_with_resolve.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.fill_pipe_msaa_with_resolve.desc_set_layout, nullptr);

    vkDestroyPipelineLayout(backend.device, resources.legacy_resolve_pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.legacy_resolve_pipe.desc_set_layout, nullptr);
}

VisBufferFrameGraphRecord create_vis_buffer_pass_record(FrameGraph::Builder&                builder,
                                                        const CullMeshletsFrameGraphRecord& meshlet_pass,
                                                        VkExtent2D render_extent, bool enable_msaa,
                                                        bool support_shader_stores_to_depth)
{
    VisBufferFrameGraphRecord vis_buffer_record;

    GPUTextureProperties scene_depth_properties =
        default_texture_properties(render_extent.width, render_extent.height, MainPassDepthFormat,
                                   GPUTextureUsage::DepthStencilAttachment | GPUTextureUsage::Sampled);

    {
        VisBufferFrameGraphRecord::Render& visibility = vis_buffer_record.render;

        visibility.pass_handle = builder.create_render_pass("Visibility");

        GPUTextureProperties vis_buffer_properties =
            default_texture_properties(render_extent.width, render_extent.height, VisibilityBufferFormat,
                                       GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled);

        if (enable_msaa)
        {
            // FIXME dragons be hiding when using even resolutions!
            GPUTextureProperties vis_buffer_msaa_properties = vis_buffer_properties;
            vis_buffer_msaa_properties.width /= 2;
            vis_buffer_msaa_properties.height /= 2;
            vis_buffer_msaa_properties.sample_count = MSAASamples;

            visibility.vis_buffer = builder.create_texture(
                visibility.pass_handle, "Visibility Buffer MSAA", vis_buffer_msaa_properties,
                GPUTextureAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL});
        }
        else
        {
            visibility.vis_buffer = builder.create_texture(
                visibility.pass_handle, "Visibility Buffer", vis_buffer_properties,
                GPUTextureAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL});
        }

        if (enable_msaa)
        {
            // FIXME dragons be hiding when using even resolutions!
            GPUTextureProperties vis_depth_msaa_properties = scene_depth_properties;
            vis_depth_msaa_properties.width /= 2;
            vis_depth_msaa_properties.height /= 2;
            vis_depth_msaa_properties.sample_count = MSAASamples;
            vis_depth_msaa_properties.misc_flags |= GPUTextureMisc::SampleLocationCompatible;

            visibility.depth = builder.create_texture(
                visibility.pass_handle, "Visibility Depth MSAA", vis_depth_msaa_properties,
                GPUTextureAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                     | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL});
        }
        else
        {
            visibility.depth = builder.create_texture(
                visibility.pass_handle, "Main Depth", scene_depth_properties,
                GPUTextureAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                     | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL});

            vis_buffer_record.depth = visibility.depth; // FIXME
        }

        visibility.meshlet_counters = builder.read_buffer(
            visibility.pass_handle, meshlet_pass.cull_triangles.meshlet_counters,
            GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

        visibility.meshlet_indirect_draw_commands = builder.read_buffer(
            visibility.pass_handle, meshlet_pass.cull_triangles.meshlet_indirect_draw_commands,
            GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

        visibility.meshlet_visible_index_buffer =
            builder.read_buffer(visibility.pass_handle, meshlet_pass.cull_triangles.meshlet_visible_index_buffer,
                                GPUBufferAccess{VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT});

        visibility.visible_meshlet_buffer =
            builder.read_buffer(visibility.pass_handle, meshlet_pass.cull_triangles.visible_meshlet_buffer,
                                GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});
    }

    {
        VisBufferFrameGraphRecord::FillGBuffer& visibility_gbuffer = vis_buffer_record.fill_gbuffer;

        visibility_gbuffer.pass_handle = builder.create_render_pass("Visibility Fill GBuffer");

        visibility_gbuffer.vis_buffer =
            builder.read_texture(visibility_gbuffer.pass_handle, vis_buffer_record.render.vis_buffer,
                                 GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

        if (enable_msaa && support_shader_stores_to_depth)
        {
            visibility_gbuffer.vis_buffer_depth_msaa =
                builder.read_texture(visibility_gbuffer.pass_handle, vis_buffer_record.render.depth,
                                     GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                      VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

            scene_depth_properties.usage_flags |= GPUTextureUsage::Storage;

            visibility_gbuffer.resolved_depth =
                builder.create_texture(visibility_gbuffer.pass_handle, "Main Depth", scene_depth_properties,
                                       GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                        VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL});

            vis_buffer_record.depth = visibility_gbuffer.resolved_depth; // FIXME
        }
        else
        {
            visibility_gbuffer.vis_buffer_depth_msaa = FrameGraph::InvalidResourceUsageHandle;
            visibility_gbuffer.resolved_depth = FrameGraph::InvalidResourceUsageHandle;
        }

        visibility_gbuffer.gbuffer_rt0 = builder.create_texture(
            visibility_gbuffer.pass_handle, "GBuffer RT0",
            default_texture_properties(render_extent.width, render_extent.height, GBufferRT0Format,
                                       GPUTextureUsage::Sampled | GPUTextureUsage::Storage),
            GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                             VK_IMAGE_LAYOUT_GENERAL});

        visibility_gbuffer.gbuffer_rt1 = builder.create_texture(
            visibility_gbuffer.pass_handle, "GBuffer RT1",
            default_texture_properties(render_extent.width, render_extent.height, GBufferRT1Format,
                                       GPUTextureUsage::Sampled | GPUTextureUsage::Storage),
            GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                             VK_IMAGE_LAYOUT_GENERAL});

        visibility_gbuffer.meshlet_visible_index_buffer = builder.read_buffer(
            visibility_gbuffer.pass_handle, meshlet_pass.cull_triangles.meshlet_visible_index_buffer,
            GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

        visibility_gbuffer.visible_meshlet_buffer =
            builder.read_buffer(visibility_gbuffer.pass_handle, meshlet_pass.cull_triangles.visible_meshlet_buffer,
                                GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});
    }

    if (enable_msaa && !support_shader_stores_to_depth)
    {
        VisBufferFrameGraphRecord::LegacyDepthResolve& legacy_depth_resolve = vis_buffer_record.legacy_depth_resolve;

        legacy_depth_resolve.pass_handle = builder.create_render_pass("Legacy Depth Resolve");

        legacy_depth_resolve.vis_buffer_depth_msaa =
            builder.read_texture(legacy_depth_resolve.pass_handle, vis_buffer_record.render.depth,
                                 GPUTextureAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

        legacy_depth_resolve.resolved_depth = builder.create_texture(
            legacy_depth_resolve.pass_handle, "Main Depth", scene_depth_properties,
            GPUTextureAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                             VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL});

        vis_buffer_record.depth = legacy_depth_resolve.resolved_depth; // FIXME
    }

    vis_buffer_record.scene_depth_properties = scene_depth_properties;

    return vis_buffer_record;
}

void update_vis_buffer_pass_resources(const FrameGraph::FrameGraph&    frame_graph,
                                      const FrameGraphResources&       frame_graph_resources,
                                      const VisBufferFrameGraphRecord& record, DescriptorWriteHelper& write_helper,
                                      StorageBufferAllocator&              frame_storage_allocator,
                                      const VisibilityBufferPassResources& resources, const PreparedData& prepared,
                                      const SamplerResources&  sampler_resources,
                                      const MaterialResources& material_resources, const MeshCache& mesh_cache,
                                      bool enable_msaa, bool support_shader_stores_to_depth)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.mesh_instances.empty())
        return;

    Assert(!prepared.mesh_instances.empty());

    StorageBufferAlloc mesh_instance_alloc =
        allocate_storage(frame_storage_allocator, prepared.mesh_instances.size() * sizeof(MeshInstance));

    upload_storage_buffer(frame_storage_allocator, mesh_instance_alloc, prepared.mesh_instances.data());

    StorageBufferAlloc mesh_material_alloc =
        allocate_storage(frame_storage_allocator, prepared.mesh_materials.size() * sizeof(MeshMaterial));

    upload_storage_buffer(frame_storage_allocator, mesh_material_alloc, prepared.mesh_materials.data());

    {
        const FrameGraphBuffer visible_meshlet_buffer =
            get_frame_graph_buffer(frame_graph_resources, frame_graph, record.render.visible_meshlet_buffer);

        using namespace Render;

        write_helper.append(resources.descriptor_set, g_bindings[instance_params], mesh_instance_alloc.buffer,
                            mesh_instance_alloc.offset_bytes, mesh_instance_alloc.size_bytes);
        write_helper.append(resources.descriptor_set, g_bindings[visible_meshlets], visible_meshlet_buffer.handle);
        write_helper.append(resources.descriptor_set, g_bindings[buffer_position_ms],
                            mesh_cache.vertexBufferPosition.handle);
    }

    {
        const FrameGraphBuffer meshlet_visible_index_buffer = get_frame_graph_buffer(
            frame_graph_resources, frame_graph, record.fill_gbuffer.meshlet_visible_index_buffer);
        const FrameGraphBuffer visible_meshlet_buffer =
            get_frame_graph_buffer(frame_graph_resources, frame_graph, record.fill_gbuffer.visible_meshlet_buffer);
        const FrameGraphTexture vis_buffer =
            get_frame_graph_texture(frame_graph_resources, frame_graph, record.fill_gbuffer.vis_buffer);
        const FrameGraphTexture gbuffer_rt0 =
            get_frame_graph_texture(frame_graph_resources, frame_graph, record.fill_gbuffer.gbuffer_rt0);
        const FrameGraphTexture gbuffer_rt1 =
            get_frame_graph_texture(frame_graph_resources, frame_graph, record.fill_gbuffer.gbuffer_rt1);

        const GPUBufferView visible_index_buffer_view =
            get_buffer_view(meshlet_visible_index_buffer.properties,
                            get_meshlet_visible_index_buffer_pass(prepared.main_culling_pass_index));

        using namespace FillGBuffer;

        write_helper.append(resources.descriptor_set_fill, g_bindings[VisBuffer], vis_buffer.default_view_handle,
                            vis_buffer.image_layout);

        if (enable_msaa && support_shader_stores_to_depth)
        {
            const FrameGraphTexture vis_buffer_depth_msaa =
                get_frame_graph_texture(frame_graph_resources, frame_graph, record.fill_gbuffer.vis_buffer_depth_msaa);
            const FrameGraphTexture resolved_depth =
                get_frame_graph_texture(frame_graph_resources, frame_graph, record.fill_gbuffer.resolved_depth);

            write_helper.append(resources.descriptor_set_fill, g_bindings[VisBufferDepthMS],
                                vis_buffer_depth_msaa.default_view_handle, vis_buffer_depth_msaa.image_layout);
            write_helper.append(resources.descriptor_set_fill, g_bindings[ResolvedDepth],
                                resolved_depth.default_view_handle, resolved_depth.image_layout);
        }

        write_helper.append(resources.descriptor_set_fill, g_bindings[GBuffer0], gbuffer_rt0.default_view_handle,
                            gbuffer_rt0.image_layout);
        write_helper.append(resources.descriptor_set_fill, g_bindings[GBuffer1], gbuffer_rt1.default_view_handle,
                            gbuffer_rt1.image_layout);
        write_helper.append(resources.descriptor_set_fill, g_bindings[instance_params], mesh_instance_alloc.buffer,
                            mesh_instance_alloc.offset_bytes, mesh_instance_alloc.size_bytes);
        write_helper.append(resources.descriptor_set_fill, g_bindings[visible_index_buffer],
                            meshlet_visible_index_buffer.handle, visible_index_buffer_view.offset_bytes,
                            visible_index_buffer_view.size_bytes);
        write_helper.append(resources.descriptor_set_fill, g_bindings[buffer_position_ms],
                            mesh_cache.vertexBufferPosition.handle);
        write_helper.append(resources.descriptor_set_fill, g_bindings[buffer_attributes],
                            mesh_cache.vertexAttributesBuffer.handle);
        write_helper.append(resources.descriptor_set_fill, g_bindings[visible_meshlets], visible_meshlet_buffer.handle);
        write_helper.append(resources.descriptor_set_fill, g_bindings[mesh_materials], mesh_material_alloc.buffer,
                            mesh_material_alloc.offset_bytes, mesh_material_alloc.size_bytes);
        write_helper.append(resources.descriptor_set_fill, g_bindings[diffuse_map_sampler],
                            sampler_resources.diffuse_map_sampler);

        if (!material_resources.textures.empty())
        {
            std::span<VkDescriptorImageInfo> albedo_image_infos =
                write_helper.new_image_infos(static_cast<u32>(material_resources.textures.size()));

            for (u32 index = 0; index < albedo_image_infos.size(); index += 1)
            {
                const auto& albedo_map = material_resources.textures[index];
                albedo_image_infos[index] =
                    create_descriptor_image_info(albedo_map.default_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            write_helper.writes.push_back(
                create_image_descriptor_write(resources.descriptor_set_fill, g_bindings[material_maps].slot,
                                              g_bindings[material_maps].type, albedo_image_infos));
        }
    }

    if (enable_msaa && !support_shader_stores_to_depth)
    {
        const FrameGraphTexture vis_buffer_depth_msaa = get_frame_graph_texture(
            frame_graph_resources, frame_graph, record.legacy_depth_resolve.vis_buffer_depth_msaa);

        write_helper.append(resources.descriptor_set_legacy_resolve, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                            vis_buffer_depth_msaa.default_view_handle, vis_buffer_depth_msaa.image_layout);
    }
}

void record_vis_buffer_pass_command_buffer(const FrameGraphHelper&                  frame_graph_helper,
                                           const VisBufferFrameGraphRecord::Render& pass_record,
                                           CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                           const PreparedData&                  prepared,
                                           const VisibilityBufferPassResources& pass_resources, bool enable_msaa)
{
    // FIXME should be moved out
    if (prepared.mesh_instances.empty())
        return;

    REAPER_GPU_SCOPE(cmdBuffer, "Visibility");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const VisibilityBufferPipelineInfo& pipe = enable_msaa ? pass_resources.pipe_msaa : pass_resources.pipe;

    const FrameGraphBuffer meshlet_counters = get_frame_graph_buffer(
        frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.meshlet_counters);
    const FrameGraphBuffer meshlet_indirect_draw_commands = get_frame_graph_buffer(
        frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.meshlet_indirect_draw_commands);
    const FrameGraphBuffer meshlet_visible_index_buffer = get_frame_graph_buffer(
        frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.meshlet_visible_index_buffer);
    const FrameGraphTexture vis_buffer =
        get_frame_graph_texture(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.vis_buffer);
    const FrameGraphTexture depth_buffer =
        get_frame_graph_texture(frame_graph_helper.resources, frame_graph_helper.frame_graph, pass_record.depth);

    const VkExtent2D extent = {depth_buffer.properties.width, depth_buffer.properties.height};
    const VkRect2D   pass_rect = default_vk_rect(extent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      get_pipeline(pipeline_factory, pipe.pipeline_index));

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    VkRenderingAttachmentInfo vis_buffer_attachment =
        default_rendering_attachment_info(vis_buffer.default_view_handle, vis_buffer.image_layout);
    vis_buffer_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    vis_buffer_attachment.clearValue = VkClearValue{.color{.uint32 = {0, 0}}};

    std::vector<VkRenderingAttachmentInfo> color_attachments = {vis_buffer_attachment};

    VkRenderingAttachmentInfo depth_attachment =
        default_rendering_attachment_info(depth_buffer.default_view_handle, depth_buffer.image_layout);
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.clearValue = VkClearDepthStencil(MainPassUseReverseZ ? 0.f : 1.f, 0);

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, color_attachments, &depth_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    const MeshletDrawParams meshlet_draw = get_meshlet_draw_params(prepared.main_culling_pass_index);

    vkCmdBindIndexBuffer2(cmdBuffer.handle, meshlet_visible_index_buffer.handle, meshlet_draw.index_buffer_offset,
                          VK_WHOLE_SIZE, meshlet_draw.index_type);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipelineLayout, 0, 1,
                            &pass_resources.descriptor_set, 0, nullptr);

    vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, meshlet_indirect_draw_commands.handle,
                                  meshlet_draw.command_buffer_offset, meshlet_counters.handle,
                                  meshlet_draw.counter_buffer_offset, meshlet_draw.command_buffer_max_count,
                                  meshlet_indirect_draw_commands.properties.element_size_bytes);

    vkCmdEndRendering(cmdBuffer.handle);
}

void record_fill_gbuffer_pass_command_buffer(const FrameGraphHelper&                       frame_graph_helper,
                                             const VisBufferFrameGraphRecord::FillGBuffer& pass_record,
                                             CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                             const VisibilityBufferPassResources& resources, VkExtent2D render_extent,
                                             bool enable_msaa, bool support_shader_stores_to_depth)
{
    REAPER_GPU_SCOPE(cmdBuffer, "Visibility Fill GBuffer");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    const VisibilityBufferPipelineInfo& pipe =
        enable_msaa
            ? (support_shader_stores_to_depth ? resources.fill_pipe_msaa_with_resolve : resources.fill_pipe_msaa)
            : resources.fill_pipe;

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                      get_pipeline(pipeline_factory, pipe.pipeline_index));

    FillGBufferPushConstants push_constants;
    push_constants.extent_ts = glm::uvec2(render_extent.width, render_extent.height);
    push_constants.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(render_extent.width), 1.f / static_cast<float>(render_extent.height));

    vkCmdPushConstants(cmdBuffer.handle, pipe.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants),
                       &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.pipelineLayout, 0, 1,
                            &resources.descriptor_set_fill, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(render_extent.width, GBufferFillThreadCountX),
                  div_round_up(render_extent.height, GBufferFillThreadCountY),
                  1);
}

void record_legacy_depth_resolve_pass_command_buffer(const FrameGraphHelper& frame_graph_helper,
                                                     const VisBufferFrameGraphRecord::LegacyDepthResolve& pass_record,
                                                     CommandBuffer& cmdBuffer, const PipelineFactory& pipeline_factory,
                                                     const VisibilityBufferPassResources& resources, bool enable_msaa,
                                                     bool support_shader_stores_to_depth)
{
    bool is_pass_active = enable_msaa && !support_shader_stores_to_depth;

    if (!is_pass_active)
        return;

    REAPER_GPU_SCOPE(cmdBuffer, "Legacy Depth Resolve");

    const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper, pass_record.pass_handle);

    FrameGraphTexture depth_dst = get_frame_graph_texture(frame_graph_helper.resources, frame_graph_helper.frame_graph,
                                                          pass_record.resolved_depth);

    const VkExtent2D depth_extent = {depth_dst.properties.width, depth_dst.properties.height};
    const VkRect2D   pass_rect = default_vk_rect(depth_extent);
    const VkViewport viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      get_pipeline(pipeline_factory, resources.legacy_resolve_pipe.pipeline_index));

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            resources.legacy_resolve_pipe.pipelineLayout, 0, 1,
                            &resources.descriptor_set_legacy_resolve, 0, nullptr);

    VkRenderingAttachmentInfo depth_attachment =
        default_rendering_attachment_info(depth_dst.default_view_handle, depth_dst.image_layout);
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, nullptr, &depth_attachment);

    vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

    vkCmdDraw(cmdBuffer.handle, 3, 1, 0, 0);

    vkCmdEndRendering(cmdBuffer.handle);
}
} // namespace Reaper
