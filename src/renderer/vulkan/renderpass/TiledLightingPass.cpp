////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TiledLightingPass.h"

#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/buffer/GPUBufferView.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/MaterialResources.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/ShaderModules.h"
#include "renderer/vulkan/renderpass/LightingPass.h"

#include "common/ReaperRoot.h"
#include "mesh/Mesh.h"
#include "mesh/ModelLoader.h"
#include "profiling/Scope.h"

#include "renderer/shader/share/tile_depth.hlsl"
#include "renderer/shader/share/tiled_lighting.hlsl"

namespace Reaper
{
constexpr u32 CubeStripVertexCount = 14; // FIXME
constexpr u32 MAX_VERTEX_COUNT = 1024;

namespace RasterPass
{
    enum
    {
        Inner,
        Outer
    };
}

TiledLightingPassResources create_tiled_lighting_pass_resources(ReaperRoot& root, VulkanBackend& backend,
                                                                const ShaderModules& shader_modules)
{
    TiledLightingPassResources resources = {};

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

        VkPipelineLayout pipeline_layout = create_pipeline_layout(
            backend.device, nonstd::span(&descriptor_set_layout, 1), nonstd::span(&pushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipeline_layout, shader_modules.tile_depth_downsample_cs);

        resources.tile_depth_descriptor_set_layout = descriptor_set_layout;
        resources.tile_depth_pipeline_layout = pipeline_layout;
        resources.tile_depth_pipeline = pipeline;
    }

    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT,
             shader_modules.fullscreen_triangle_vs, default_entry_point(), nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
             shader_modules.copy_to_depth_fs, default_entry_point(), nullptr}};

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        VkPipelineLayout pipeline_layout =
            create_pipeline_layout(backend.device, nonstd::span(&descriptor_set_layout, 1));

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
        pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthWriteEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.depthAttachmentFormat = PixelFormatToVulkan(PixelFormat::D16_UNORM);

        VkPipeline pipeline = create_graphics_pipeline(backend.device, shader_stages, pipeline_properties);

        resources.depth_copy_descriptor_set_layout = descriptor_set_layout;
        resources.depth_copy_pipeline_layout = pipeline_layout;
        resources.depth_copy_pipeline = pipeline;
    }

    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT,
             shader_modules.rasterize_light_volume_vs, default_entry_point(), nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
             shader_modules.rasterize_light_volume_fs, default_entry_point(), nullptr}};

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
             nullptr},
            {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
             nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
             nullptr},
        };

        VkDescriptorSetLayout descriptor_set_layout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                                       sizeof(TileLightRasterPushConstants)};

        VkPipelineLayout pipeline_layout = create_pipeline_layout(
            backend.device, nonstd::span(&descriptor_set_layout, 1), nonstd::span(&pushConstantRange, 1));

        const VkVertexInputBindingDescription   vertex_binding = {0, sizeof(hlsl_float3), VK_VERTEX_INPUT_RATE_VERTEX};
        const VkVertexInputAttributeDescription vertex_input_attribute = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};

        GraphicsPipelineProperties pipeline_properties = default_graphics_pipeline_properties();
        pipeline_properties.vertex_input.vertexBindingDescriptionCount = 1;
        pipeline_properties.vertex_input.pVertexBindingDescriptions = &vertex_binding;
        pipeline_properties.vertex_input.vertexAttributeDescriptionCount = 1;
        pipeline_properties.vertex_input.pVertexAttributeDescriptions = &vertex_input_attribute;
        pipeline_properties.depth_stencil.depthTestEnable = VK_TRUE;
        pipeline_properties.depth_stencil.depthWriteEnable = VK_FALSE;
        pipeline_properties.depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER; // dynamic
        pipeline_properties.raster.cullMode = VK_CULL_MODE_FRONT_AND_BACK;      // dynamic
        pipeline_properties.input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipeline_properties.pipeline_layout = pipeline_layout;
        pipeline_properties.pipeline_rendering.depthAttachmentFormat =
            PixelFormatToVulkan(PixelFormat::D16_UNORM); // FIXME

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
        std::vector<VkDescriptorSetLayoutBinding> bindings0 = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, ShadowMapMaxCount, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        std::vector<VkDescriptorBindingFlags> bindingFlags0(bindings0.size(), VK_FLAGS_NONE);
        bindingFlags0[6] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        std::vector<VkDescriptorSetLayoutBinding> bindings1 = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, DiffuseMapMaxCount, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        std::vector<VkDescriptorBindingFlags> bindingFlags1 = {VK_FLAGS_NONE,
                                                               VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

        std::vector<VkDescriptorSetLayout> descriptor_set_layouts = {
            create_descriptor_set_layout(backend.device, bindings0, bindingFlags0),
            create_descriptor_set_layout(backend.device, bindings1, bindingFlags1)};

        const VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                       sizeof(TiledLightingPushConstants)};

        VkPipelineLayout pipeline_layout =
            create_pipeline_layout(backend.device, descriptor_set_layouts, nonstd::span(&pushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipeline_layout, shader_modules.tiled_lighting_cs);

        resources.tiled_lighting_descriptor_set_layout = descriptor_set_layouts[0];
        resources.tiled_lighting_descriptor_set_layout_material = descriptor_set_layouts[1];
        resources.tiled_lighting_pipeline_layout = pipeline_layout;
        resources.tiled_lighting_pipeline = pipeline;
    }

    std::vector<VkDescriptorSetLayout> dset_layouts = {resources.tile_depth_descriptor_set_layout,
                                                       resources.depth_copy_descriptor_set_layout,
                                                       resources.depth_copy_descriptor_set_layout,
                                                       resources.light_raster_descriptor_set_layout,
                                                       resources.light_raster_descriptor_set_layout,
                                                       resources.tiled_lighting_descriptor_set_layout,
                                                       resources.tiled_lighting_descriptor_set_layout_material};
    std::vector<VkDescriptorSet>       dsets(dset_layouts.size());

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, dset_layouts, dsets);

    resources.tile_depth_descriptor_set = dsets[0];
    resources.depth_copy_descriptor_sets[0] = dsets[1];
    resources.depth_copy_descriptor_sets[1] = dsets[2];
    resources.light_raster_descriptor_sets[0] = dsets[3];
    resources.light_raster_descriptor_sets[1] = dsets[4];
    resources.tiled_lighting_descriptor_set = dsets[5];
    resources.tiled_lighting_descriptor_set_material = dsets[6];

    {
        resources.vertex_buffer_offset = 0;
        resources.vertex_buffer_position = create_buffer(
            root, backend.device, "Lighting vertex buffer",
            DefaultGPUBufferProperties(MAX_VERTEX_COUNT, sizeof(hlsl_float3), GPUBufferUsage::VertexBuffer),
            backend.vma_instance, MemUsage::CPU_To_GPU);

        std::vector<Mesh> meshes;
        const Mesh&       icosahedron = meshes.emplace_back(ModelLoader::loadOBJ("res/model/icosahedron.obj"));

        ProxyMeshAlloc& icosahedron_alloc = resources.proxy_allocs.emplace_back();
        icosahedron_alloc.vertex_offset = resources.vertex_buffer_offset;
        icosahedron_alloc.vertex_count = icosahedron.positions.size();

        resources.vertex_buffer_offset += icosahedron_alloc.vertex_count;

        upload_buffer_data(
            backend.device, backend.vma_instance, resources.vertex_buffer_position, icosahedron.positions.data(),
            icosahedron.positions.size() * sizeof(icosahedron.positions[0]), icosahedron_alloc.vertex_offset);
    }

    resources.lightVolumeBuffer = create_buffer(
        root, backend.device, "Light volume buffer",
        DefaultGPUBufferProperties(LightVolumeMax, sizeof(LightVolumeInstance), GPUBufferUsage::UniformBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.tiledLightingConstantBuffer =
        create_buffer(root, backend.device, "Tiled lighting constants",
                      DefaultGPUBufferProperties(1, sizeof(TiledLightingConstants), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    return resources;
}

void destroy_tiled_lighting_pass_resources(VulkanBackend& backend, TiledLightingPassResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.tiledLightingConstantBuffer.handle,
                     resources.tiledLightingConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.lightVolumeBuffer.handle, resources.lightVolumeBuffer.allocation);
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

    vkDestroyPipeline(backend.device, resources.tiled_lighting_pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.tiled_lighting_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.tiled_lighting_descriptor_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.tiled_lighting_descriptor_set_layout_material, nullptr);
}

void update_lighting_depth_downsample_descriptor_set(DescriptorWriteHelper&            write_helper,
                                                     const TiledLightingPassResources& resources,
                                                     const SamplerResources&           sampler_resources,
                                                     const FrameGraphTexture&          scene_depth,
                                                     const FrameGraphTexture&          tile_depth_min,
                                                     const FrameGraphTexture&          tile_depth_max)
{
    append_write(write_helper, resources.tile_depth_descriptor_set, 0, sampler_resources.linearClampSampler);
    append_write(write_helper, resources.tile_depth_descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                 scene_depth.view_handle, scene_depth.image_layout);
    append_write(write_helper, resources.tile_depth_descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                 tile_depth_min.view_handle, tile_depth_min.image_layout);
    append_write(write_helper, resources.tile_depth_descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                 tile_depth_max.view_handle, tile_depth_max.image_layout);
}

void update_depth_copy_pass_descriptor_set(DescriptorWriteHelper&            write_helper,
                                           const TiledLightingPassResources& resources,
                                           const FrameGraphTexture&          depth_min_src,
                                           const FrameGraphTexture&          depth_max_src)
{
    append_write(write_helper, resources.depth_copy_descriptor_sets[0], 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                 depth_min_src.view_handle, depth_min_src.image_layout);
    append_write(write_helper, resources.depth_copy_descriptor_sets[1], 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                 depth_max_src.view_handle, depth_max_src.image_layout);
}

void update_light_raster_pass_descriptor_sets(DescriptorWriteHelper&            write_helper,
                                              const TiledLightingPassResources& resources,
                                              const FrameGraphTexture&          depth_min,
                                              const FrameGraphTexture&          depth_max,
                                              const FrameGraphBuffer&           light_list_buffer)
{
    append_write(write_helper, resources.light_raster_descriptor_sets[RasterPass::Inner], 0,
                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, resources.lightVolumeBuffer.handle);
    append_write(write_helper, resources.light_raster_descriptor_sets[RasterPass::Inner], 1,
                 VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, depth_min.view_handle, depth_min.image_layout);
    append_write(write_helper, resources.light_raster_descriptor_sets[RasterPass::Inner], 2,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, light_list_buffer.handle);

    append_write(write_helper, resources.light_raster_descriptor_sets[RasterPass::Outer], 0,
                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, resources.lightVolumeBuffer.handle);
    append_write(write_helper, resources.light_raster_descriptor_sets[RasterPass::Outer], 1,
                 VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, depth_max.view_handle, depth_max.image_layout);
    append_write(write_helper, resources.light_raster_descriptor_sets[RasterPass::Outer], 2,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, light_list_buffer.handle);
}

void update_tiled_lighting_pass_descriptor_sets(DescriptorWriteHelper&            write_helper,
                                                const LightingPassResources&      lighting_resources,
                                                const TiledLightingPassResources& resources,
                                                const SamplerResources&           sampler_resources,
                                                const FrameGraphBuffer&           light_list_buffer,
                                                const FrameGraphTexture&          main_view_depth,
                                                const FrameGraphTexture&          lighting_output,
                                                nonstd::span<const FrameGraphTexture>
                                                                         shadow_maps,
                                                const MaterialResources& material_resources)
{
    VkDescriptorSet dset0 = resources.tiled_lighting_descriptor_set;
    append_write(write_helper, dset0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 resources.tiledLightingConstantBuffer.handle);
    append_write(write_helper, dset0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, light_list_buffer.handle);
    append_write(write_helper, dset0, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, main_view_depth.view_handle,
                 main_view_depth.image_layout);
    append_write(write_helper, dset0, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lighting_resources.pointLightBuffer.handle);
    append_write(write_helper, dset0, 4, sampler_resources.shadowMapSampler);
    append_write(write_helper, dset0, 5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, lighting_output.view_handle,
                 lighting_output.image_layout);

    if (!shadow_maps.empty())
    {
        const VkDescriptorImageInfo* image_info_ptr = write_helper.images.data() + write_helper.images.size();
        for (auto shadow_map : shadow_maps)
        {
            write_helper.images.push_back({VK_NULL_HANDLE, shadow_map.view_handle, shadow_map.image_layout});
        }

        nonstd::span<const VkDescriptorImageInfo> shadow_map_image_infos(image_info_ptr, shadow_maps.size());

        write_helper.writes.push_back(
            create_image_descriptor_write(dset0, 6, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shadow_map_image_infos));
    }

    if (!material_resources.texture_handles.empty())
    {
        VkDescriptorSet dset1 = resources.tiled_lighting_descriptor_set_material;

        append_write(write_helper, dset1, 0, sampler_resources.diffuseMapSampler);

        const VkDescriptorImageInfo* image_info_ptr = write_helper.images.data() + write_helper.images.size();

        for (auto handle : material_resources.texture_handles)
        {
            write_helper.images.push_back({VK_NULL_HANDLE, material_resources.textures[handle].default_view,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        }

        nonstd::span<const VkDescriptorImageInfo> albedo_image_infos(image_info_ptr,
                                                                     material_resources.texture_handles.size());

        Assert(albedo_image_infos.size() <= DiffuseMapMaxCount);

        write_helper.writes.push_back(
            create_image_descriptor_write(dset1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, albedo_image_infos));
    }
}

void upload_tiled_lighting_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                                TiledLightingPassResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.point_lights.empty())
        return;

    upload_buffer_data(backend.device, backend.vma_instance, resources.lightVolumeBuffer, prepared.light_volumes.data(),
                       prepared.light_volumes.size() * sizeof(LightVolumeInstance));

    upload_buffer_data(backend.device, backend.vma_instance, resources.tiledLightingConstantBuffer,
                       &prepared.tiled_light_constants, sizeof(TiledLightingConstants));
}

void record_tile_depth_pass_command_buffer(CommandBuffer& cmdBuffer, const TiledLightingPassResources& resources,
                                           VkExtent2D backbufferExtent)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tile_depth_pipeline);

    TileDepthConstants push_constants;
    push_constants.extent_ts = glm::uvec2(backbufferExtent.width, backbufferExtent.height);
    push_constants.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(backbufferExtent.width), 1.f / static_cast<float>(backbufferExtent.height));

    vkCmdPushConstants(cmdBuffer.handle, resources.tile_depth_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tile_depth_pipeline_layout, 0,
                            1, &resources.tile_depth_descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(backbufferExtent.width, TileDepthThreadCountX * 2),
                  div_round_up(backbufferExtent.height, TileDepthThreadCountY * 2),
                  1);
}

void record_depth_copy(CommandBuffer& cmdBuffer, const TiledLightingPassResources& resources,
                       const FrameGraphTexture& depth_min_dst, const FrameGraphTexture& depth_max_dst)
{
    std::vector<FrameGraphTexture> depth_dsts = {depth_min_dst, depth_max_dst};
    const VkExtent2D               depth_extent = {depth_min_dst.properties.width, depth_min_dst.properties.height};
    const VkRect2D                 pass_rect = default_vk_rect(depth_extent);
    const VkViewport               viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.depth_copy_pipeline);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    for (u32 depth_index = 0; depth_index < 2; depth_index++)
    {
        const FrameGraphTexture   depth_dst = depth_dsts[depth_index];
        VkRenderingAttachmentInfo depth_attachment =
            default_rendering_attachment_info(depth_dst.view_handle, depth_dst.image_layout);
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        const VkRenderingInfo rendering_info = default_rendering_info(pass_rect, nullptr, &depth_attachment);

        vkCmdBeginRendering(cmdBuffer.handle, &rendering_info);

        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.depth_copy_pipeline_layout,
                                0, 1, &resources.depth_copy_descriptor_sets[depth_index], 0, nullptr);

        vkCmdDraw(cmdBuffer.handle, 3, 1, 0, 0);

        vkCmdEndRendering(cmdBuffer.handle);
    }
}

void record_light_raster_command_buffer(CommandBuffer& cmdBuffer, const TiledLightingPassResources& resources,
                                        const PreparedData& prepared, const FrameGraphTexture& depth_min,
                                        const FrameGraphTexture& depth_max)
{
    std::vector<FrameGraphTexture> depth_buffers = {depth_max, depth_min};
    const VkExtent2D               depth_extent = {depth_max.properties.width, depth_max.properties.height};
    const VkRect2D                 pass_rect = default_vk_rect(depth_extent);
    const VkViewport               viewport = default_vk_viewport(pass_rect);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.light_raster_pipeline);

    const VkDeviceSize vertex_buffer_offset = 0;
    vkCmdBindVertexBuffers(cmdBuffer.handle, 0, 1, &resources.vertex_buffer_position.handle, &vertex_buffer_offset);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

    for (u32 pass_index = 0; pass_index < 2; pass_index++)
    {
        const FrameGraphTexture   depth_buffer = depth_buffers[pass_index];
        VkRenderingAttachmentInfo depth_attachment =
            default_rendering_attachment_info(depth_buffer.view_handle, depth_buffer.image_layout);
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

        const ProxyMeshAlloc& icosahedron_alloc = resources.proxy_allocs.front();
        vkCmdDraw(cmdBuffer.handle, icosahedron_alloc.vertex_count, prepared.light_volumes.size(),
                  icosahedron_alloc.vertex_offset, 0);

        vkCmdEndRendering(cmdBuffer.handle);
    }
}

void record_tiled_lighting_command_buffer(CommandBuffer& cmdBuffer, const TiledLightingPassResources& resources,
                                          VkExtent2D backbufferExtent, VkExtent2D tile_extent)
{
    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tiled_lighting_pipeline);

    TiledLightingPushConstants push_constants;
    push_constants.extent_ts = glm::uvec2(backbufferExtent.width, backbufferExtent.height);
    push_constants.extent_ts_inv =
        glm::fvec2(1.f / static_cast<float>(backbufferExtent.width), 1.f / static_cast<float>(backbufferExtent.height));
    push_constants.tile_count_x = tile_extent.width;

    vkCmdPushConstants(cmdBuffer.handle, resources.tiled_lighting_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push_constants), &push_constants);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.tiled_lighting_pipeline_layout,
                            0, 1, &resources.tiled_lighting_descriptor_set, 0, nullptr);

    vkCmdDispatch(cmdBuffer.handle,
                  div_round_up(backbufferExtent.width, TiledLightingThreadCountX),
                  div_round_up(backbufferExtent.height, TiledLightingThreadCountY),
                  1);
}
} // namespace Reaper
