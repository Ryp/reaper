////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ShadowMap.h"

#include "Culling.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/Shader.h"
#include "renderer/vulkan/api/Vulkan.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/Profile.h"

#include <array>

#include "renderer/shader/share/shadow_map_pass.hlsl"

namespace Reaper
{
constexpr u32 ShadowInstanceCountMax = 512;
constexpr u32 MaxShadowPassCount = 4;

namespace
{
    ShadowMapPipelineInfo create_shadow_map_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        VkShaderModule        blitShaderFS = VK_NULL_HANDLE;
        VkShaderModule        blitShaderVS = VK_NULL_HANDLE;
        const char*           fileNameVS = "./build/shader/render_shadow.vert.spv";
        const char*           fileNameFS = "./build/shader/render_shadow.frag.spv";
        const char*           entryPoint = "main";
        VkSpecializationInfo* specialization = nullptr;

        vulkan_create_shader_module(blitShaderFS, backend.device, fileNameFS);
        vulkan_create_shader_module(blitShaderVS, backend.device, fileNameVS);

        std::vector<VkVertexInputBindingDescription> vertexInfoShaderBinding = {
            {
                0,                          // binding
                sizeof(hlsl_float3),        // stride
                VK_VERTEX_INPUT_RATE_VERTEX // input rate
            },
        };

        std::vector<VkVertexInputAttributeDescription> vertexAttributes = {
            {
                0,                          // location
                0,                          // binding
                VK_FORMAT_R32G32B32_SFLOAT, // format
                0                           // offset
            },
        };

        std::vector<VkPipelineShaderStageCreateInfo> blitShaderStages = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, blitShaderVS,
             entryPoint, specialization},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
             blitShaderFS, entryPoint, specialization}};

        VkPipelineVertexInputStateCreateInfo blitVertexInputStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            static_cast<u32>(vertexInfoShaderBinding.size()),
            vertexInfoShaderBinding.data(),
            static_cast<u32>(vertexAttributes.size()),
            vertexAttributes.data()};

        VkPipelineInputAssemblyStateCreateInfo blitInputAssemblyInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, VK_FLAGS_NONE,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};

        VkPipelineViewportStateCreateInfo blitViewportStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            1,
            nullptr, // dynamic viewport
            1,
            nullptr}; // dynamic scissors

        VkPipelineRasterizationStateCreateInfo blitRasterStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            VK_FALSE,
            VK_FALSE,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_COUNTER_CLOCKWISE,
            VK_FALSE,
            0.0f,
            0.0f,
            0.0f,
            1.0f};

        VkPipelineMultisampleStateCreateInfo blitMSStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FALSE,
            1.0f,
            nullptr,
            VK_FALSE,
            VK_FALSE};

        const VkPipelineDepthStencilStateCreateInfo blitDepthStencilInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            nullptr,
            0,
            true, // depth test
            true, // depth write
            ShadowUseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS,
            false,
            false,
            VkStencilOpState{},
            VkStencilOpState{},
            0.f,
            0.f};

        VkPipelineColorBlendAttachmentState blitBlendAttachmentState = {
            VK_FALSE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

        VkPipelineColorBlendStateCreateInfo blitBlendStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            VK_FLAGS_NONE,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            1,
            &blitBlendAttachmentState,
            {0.0f, 0.0f, 0.0f, 0.0f}};

        std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout descriptorSetLayoutCB = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayoutCB)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayoutCB));

        VkPipelineLayoutCreateInfo blitPipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                             nullptr,
                                                             VK_FLAGS_NONE,
                                                             1,
                                                             &descriptorSetLayoutCB,
                                                             0,
                                                             nullptr};

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Assert(vkCreatePipelineLayout(backend.device, &blitPipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);
        log_debug(root, "vulkan: created blit pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

        VkPipelineCache cache = VK_NULL_HANDLE;

        const std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo    blitDynamicState = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,
            0,
            dynamicStates.size(),
            dynamicStates.data(),
        };

        const VkFormat shadowMapFormat = PixelFormatToVulkan(ShadowMapFormat);

        const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            nullptr,
            0, // viewMask
            0,
            nullptr,
            shadowMapFormat,
            VK_FORMAT_UNDEFINED,
        };

        VkGraphicsPipelineCreateInfo blitPipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                               &pipelineRenderingCreateInfo,
                                                               VK_FLAGS_NONE,
                                                               static_cast<u32>(blitShaderStages.size()),
                                                               blitShaderStages.data(),
                                                               &blitVertexInputStateInfo,
                                                               &blitInputAssemblyInfo,
                                                               nullptr,
                                                               &blitViewportStateInfo,
                                                               &blitRasterStateInfo,
                                                               &blitMSStateInfo,
                                                               &blitDepthStencilInfo,
                                                               &blitBlendStateInfo,
                                                               &blitDynamicState,
                                                               pipelineLayout,
                                                               VK_NULL_HANDLE,
                                                               0,
                                                               VK_NULL_HANDLE,
                                                               -1};

        VkPipeline pipeline = VK_NULL_HANDLE;
        Assert(vkCreateGraphicsPipelines(backend.device, cache, 1, &blitPipelineCreateInfo, nullptr, &pipeline)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created blit pipeline with handle: {}", static_cast<void*>(pipeline));

        vkDestroyShaderModule(backend.device, blitShaderVS, nullptr);
        vkDestroyShaderModule(backend.device, blitShaderFS, nullptr);

        return ShadowMapPipelineInfo{pipeline, pipelineLayout, descriptorSetLayoutCB};
    }

    VkDescriptorSet create_shadow_map_pass_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
                                                          VkDescriptorSetLayout layout)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              backend.global_descriptor_pool, 1, &layout};

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptor_set) == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptor_set));

        return descriptor_set;
    }

    void update_shadow_map_pass_descriptor_set(VulkanBackend& backend, const ShadowMapResources& resources,
                                               const ShadowPassData& shadow_pass)
    {
        Assert(shadow_pass.pass_index < resources.descriptor_sets.size());
        VkDescriptorSet descriptor_set = resources.descriptor_sets[shadow_pass.pass_index];

        const VkDescriptorBufferInfo shadowMapDescPassParams = get_vk_descriptor_buffer_info(
            resources.shadowMapPassConstantBuffer, GPUBufferView{shadow_pass.pass_index, 1});
        const VkDescriptorBufferInfo shadowMapDescInstanceParams =
            get_vk_descriptor_buffer_info(resources.shadowMapInstanceConstantBuffer,
                                          GPUBufferView{shadow_pass.instance_offset, shadow_pass.instance_count});

        std::array<VkWriteDescriptorSet, 2> shadowMapPassDescriptorSetWrites = {
            create_buffer_descriptor_write(descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           &shadowMapDescPassParams),
            create_buffer_descriptor_write(descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &shadowMapDescInstanceParams),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(shadowMapPassDescriptorSetWrites.size()),
                               shadowMapPassDescriptorSetWrites.data(), 0, nullptr);
    }

    GPUTextureProperties get_shadow_map_texture_properties(glm::uvec2 size)
    {
        GPUTextureProperties properties = DefaultGPUTextureProperties(size.x, size.y, ShadowMapFormat);

        properties.usageFlags =
            GPUTextureUsage::DepthStencilAttachment | GPUTextureUsage::InputAttachment | GPUTextureUsage::Sampled;

        return properties;
    }
} // namespace

ShadowMapResources create_shadow_map_resources(ReaperRoot& root, VulkanBackend& backend)
{
    ShadowMapResources resources = {};

    resources.pipe = create_shadow_map_pipeline(root, backend);

    resources.shadowMapPassConstantBuffer = create_buffer(
        root, backend.device, "Shadow Map Pass Constant buffer",
        DefaultGPUBufferProperties(MaxShadowPassCount, sizeof(ShadowMapPassParams), GPUBufferUsage::UniformBuffer),
        backend.vma_instance);

    resources.shadowMapInstanceConstantBuffer =
        create_buffer(root, backend.device, "Shadow Map Instance Constant buffer",
                      DefaultGPUBufferProperties(ShadowInstanceCountMax, sizeof(ShadowMapInstanceParams),
                                                 GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.descriptor_sets.push_back(
        create_shadow_map_pass_descriptor_set(root, backend, resources.pipe.descSetLayout));
    resources.descriptor_sets.push_back(
        create_shadow_map_pass_descriptor_set(root, backend, resources.pipe.descSetLayout));
    resources.descriptor_sets.push_back(
        create_shadow_map_pass_descriptor_set(root, backend, resources.pipe.descSetLayout));

    return resources;
}

void destroy_shadow_map_resources(VulkanBackend& backend, ShadowMapResources& resources)
{
    for (u32 i = 0; i < resources.shadowMap.size(); i++)
    {
        vmaDestroyImage(backend.vma_instance, resources.shadowMap[i].handle, resources.shadowMap[i].allocation);
        vkDestroyImageView(backend.device, resources.shadowMapView[i], nullptr);
    }

    vmaDestroyBuffer(backend.vma_instance, resources.shadowMapPassConstantBuffer.buffer,
                     resources.shadowMapPassConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.shadowMapInstanceConstantBuffer.buffer,
                     resources.shadowMapInstanceConstantBuffer.allocation);

    vkDestroyPipeline(backend.device, resources.pipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.descSetLayout, nullptr);
}

void prepare_shadow_map_objects(ReaperRoot& root, VulkanBackend& backend, const PreparedData& prepared,
                                ShadowMapResources& pass_resources)
{
    for (u32 i = 0; i < pass_resources.shadowMap.size(); i++)
    {
        vmaDestroyImage(backend.vma_instance, pass_resources.shadowMap[i].handle,
                        pass_resources.shadowMap[i].allocation);
        vkDestroyImageView(backend.device, pass_resources.shadowMapView[i], nullptr);
    }

    pass_resources.shadowMap.clear();
    pass_resources.shadowMapView.clear();
    for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
    {
        const GPUTextureProperties texture_properties = get_shadow_map_texture_properties(shadow_pass.shadow_map_size);

        ImageInfo& shadow_map = pass_resources.shadowMap.emplace_back();

        shadow_map = create_image(root, backend.device, "Shadow Map", texture_properties, backend.vma_instance);

        pass_resources.shadowMapView.push_back(create_depth_image_view(root, backend.device, shadow_map));
    }
}

void update_shadow_map_pass_descriptor_sets(VulkanBackend& backend, const PreparedData& prepared,
                                            ShadowMapResources& pass_resources)
{
    for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
    {
        update_shadow_map_pass_descriptor_set(backend, pass_resources, shadow_pass);
    }
}

void upload_shadow_map_resources(VulkanBackend& backend, const PreparedData& prepared, ShadowMapResources& resources)
{
    upload_buffer_data(backend.device, backend.vma_instance, resources.shadowMapPassConstantBuffer,
                       prepared.shadow_pass_params.data(),
                       prepared.shadow_pass_params.size() * sizeof(ShadowMapPassParams));

    upload_buffer_data(backend.device, backend.vma_instance, resources.shadowMapInstanceConstantBuffer,
                       prepared.shadow_instance_params.data(),
                       prepared.shadow_instance_params.size() * sizeof(ShadowMapInstanceParams));
}

void record_shadow_map_command_buffer(CommandBuffer& cmdBuffer, VulkanBackend& backend, const PreparedData& prepared,
                                      ShadowMapResources& resources, const CullResources& cull_resources,
                                      VkBuffer vertex_position_buffer)
{
    for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Shadow Pass", MP_DARKGOLDENROD);

        const VkImageView  shadowMapView = resources.shadowMapView[shadow_pass.pass_index];
        const VkExtent2D   output_extent = {shadow_pass.shadow_map_size.x, shadow_pass.shadow_map_size.y};
        const VkRect2D     pass_rect = default_vk_rect(output_extent);
        const VkClearValue shadowMapClearValue =
            VkClearDepthStencil(ShadowUseReverseZ ? 0.f : 1.f, 0); // NOTE: handle reverse Z more gracefully

        const VkRenderingAttachmentInfo depth_attachment = {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            nullptr,
            shadowMapView,
            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            VK_RESOLVE_MODE_NONE,
            VK_NULL_HANDLE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            shadowMapClearValue,
        };

        VkRenderingInfo renderingInfo = {
            VK_STRUCTURE_TYPE_RENDERING_INFO,
            nullptr,
            VK_FLAGS_NONE,
            pass_rect,
            1, // layerCount
            0, // viewMask
            0,
            nullptr,
            &depth_attachment,
            nullptr,
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderingInfo);

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipe.pipeline);

        const VkViewport viewport = default_vk_viewport(pass_rect);

        vkCmdSetViewport(cmdBuffer.handle, 0, 1, &viewport);
        vkCmdSetScissor(cmdBuffer.handle, 0, 1, &pass_rect);

        std::vector<VkBuffer> vertexBuffers = {
            vertex_position_buffer,
        };
        std::vector<VkDeviceSize> vertexBufferOffsets = {0};

        Assert(vertexBuffers.size() == vertexBufferOffsets.size());
        vkCmdBindIndexBuffer(cmdBuffer.handle, cull_resources.dynamicIndexBuffer.buffer, 0,
                             get_vk_culling_index_type());
        vkCmdBindVertexBuffers(cmdBuffer.handle, 0, static_cast<u32>(vertexBuffers.size()), vertexBuffers.data(),
                               vertexBufferOffsets.data());
        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipe.pipelineLayout, 0, 1,
                                &resources.descriptor_sets[shadow_pass.pass_index], 0, nullptr);

        const u32 draw_buffer_offset =
            shadow_pass.culling_pass_index * MaxIndirectDrawCount * sizeof(VkDrawIndexedIndirectCommand);
        const u32 draw_buffer_max_count = MaxIndirectDrawCount;

        if (backend.options.use_compacted_draw)
        {
            const u32 draw_buffer_count_offset = shadow_pass.culling_pass_index * 1 * sizeof(u32);
            vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, cull_resources.compactIndirectDrawBuffer.buffer,
                                          draw_buffer_offset, cull_resources.compactIndirectDrawCountBuffer.buffer,
                                          draw_buffer_count_offset, draw_buffer_max_count,
                                          cull_resources.compactIndirectDrawBuffer.descriptor.elementSize);
        }
        else
        {
            const u32 draw_buffer_count_offset = shadow_pass.culling_pass_index * IndirectDrawCountCount * sizeof(u32);
            vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, cull_resources.indirectDrawBuffer.buffer,
                                          draw_buffer_offset, cull_resources.indirectDrawCountBuffer.buffer,
                                          draw_buffer_count_offset, draw_buffer_max_count,
                                          cull_resources.indirectDrawBuffer.descriptor.elementSize);
        }

        vkCmdEndRendering(cmdBuffer.handle);
    }
}
} // namespace Reaper
