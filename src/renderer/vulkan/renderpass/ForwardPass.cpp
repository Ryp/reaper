////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ForwardPass.h"

#include "Culling.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/MaterialResources.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/SamplerResources.h"
#include "renderer/vulkan/Shader.h"
#include "renderer/vulkan/renderpass/ForwardPassConstants.h"
#include "renderer/vulkan/renderpass/LightingPass.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "renderer/shader/share/forward.hlsl"

namespace Reaper
{
constexpr u32 DiffuseMapMaxCount = 8;
constexpr u32 DrawInstanceCountMax = 512;

namespace
{
    VkDescriptorSetLayout create_descriptor_set_layout_0(VulkanBackend& backend)
    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
             nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {7, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, ShadowMapMaxCount, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        std::vector<VkDescriptorBindingFlags> bindingFlags = {
            VK_FLAGS_NONE, VK_FLAGS_NONE, VK_FLAGS_NONE, VK_FLAGS_NONE,
            VK_FLAGS_NONE, VK_FLAGS_NONE, VK_FLAGS_NONE, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

        Assert(bindingFlags.size() == descriptorSetLayoutBinding.size());

        const VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags =
            descriptor_set_layout_binding_flags_create_info(bindingFlags);

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo =
            descriptor_set_layout_create_info(descriptorSetLayoutBinding);
        descriptorSetLayoutInfo.pNext = &descriptorSetLayoutBindingFlags;

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &layout) == VK_SUCCESS);

        return layout;
    }

    void update_descriptor_set_0(VulkanBackend& backend, const ForwardPassResources& resources,
                                 const SamplerResources& sampler_resources, const MeshCache& mesh_cache,
                                 const LightingPassResources&    lighting_resources,
                                 const nonstd::span<VkImageView> shadow_map_views)
    {
        const VkDescriptorBufferInfo passParams = default_descriptor_buffer_info(resources.passConstantBuffer);
        const VkDescriptorBufferInfo instanceParams = default_descriptor_buffer_info(resources.instancesConstantBuffer);

        const VkDescriptorBufferInfo vertexPositionParams =
            default_descriptor_buffer_info(mesh_cache.vertexBufferPosition);
        const VkDescriptorBufferInfo vertexNormalParams = default_descriptor_buffer_info(mesh_cache.vertexBufferNormal);
        const VkDescriptorBufferInfo vertexUVParams = default_descriptor_buffer_info(mesh_cache.vertexBufferUV);
        const VkDescriptorBufferInfo pointLightParams =
            default_descriptor_buffer_info(lighting_resources.pointLightBuffer);

        const VkDescriptorImageInfo shadowMapSampler = {sampler_resources.shadowMapSampler, VK_NULL_HANDLE,
                                                        VK_IMAGE_LAYOUT_UNDEFINED};

        std::vector<VkWriteDescriptorSet> writes = {
            create_buffer_descriptor_write(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &passParams),
            create_buffer_descriptor_write(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &instanceParams),
            create_buffer_descriptor_write(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &vertexPositionParams),
            create_buffer_descriptor_write(resources.descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &vertexNormalParams),
            create_buffer_descriptor_write(resources.descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &vertexUVParams),
            create_buffer_descriptor_write(resources.descriptor_set, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &pointLightParams),
            create_image_descriptor_write(resources.descriptor_set, 6, VK_DESCRIPTOR_TYPE_SAMPLER, &shadowMapSampler)};

        std::vector<VkDescriptorImageInfo> drawDescShadowMaps;

        for (auto shadow_map_texture_view : shadow_map_views)
        {
            drawDescShadowMaps.push_back(
                {VK_NULL_HANDLE, shadow_map_texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        }

        const VkWriteDescriptorSet shadowMapBindlessImageWrite = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            resources.descriptor_set,
            7,
            0,
            static_cast<u32>(drawDescShadowMaps.size()),
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            drawDescShadowMaps.data(),
            nullptr,
            nullptr,
        };

        // Only stage descriptor write if we actually have shadow maps to bind
        if (!drawDescShadowMaps.empty())
        {
            writes.push_back(shadowMapBindlessImageWrite);
        }

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    VkDescriptorSetLayout create_descriptor_set_layout_1(VulkanBackend& backend)
    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, DiffuseMapMaxCount, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        std::vector<VkDescriptorBindingFlags> bindingFlags = {VK_FLAGS_NONE, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

        Assert(bindingFlags.size() == descriptorSetLayoutBinding.size());

        const VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags =
            descriptor_set_layout_binding_flags_create_info(bindingFlags);

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo =
            descriptor_set_layout_create_info(descriptorSetLayoutBinding);
        descriptorSetLayoutInfo.pNext = &descriptorSetLayoutBindingFlags;

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &layout) == VK_SUCCESS);

        return layout;
    }

    void update_descriptor_set_1(VulkanBackend& backend, const ForwardPassResources& resources,
                                 const SamplerResources& sampler_resources, const MaterialResources& material_resources)
    {
        if (material_resources.texture_handles.empty())
            return;

        std::vector<VkDescriptorImageInfo> descriptor_maps;

        for (auto handle : material_resources.texture_handles)
        {
            VkImageView image_view = material_resources.textures[handle].default_view;
            descriptor_maps.push_back({VK_NULL_HANDLE, image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        }

        Assert(descriptor_maps.size() <= DiffuseMapMaxCount);

        const VkWriteDescriptorSet diffuseMapBindlessImageWrite = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            resources.material_descriptor_set,
            1,
            0,
            static_cast<u32>(descriptor_maps.size()),
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            descriptor_maps.data(),
            nullptr,
            nullptr,
        };

        const VkDescriptorImageInfo diffuseMapSampler = {sampler_resources.diffuseMapSampler, VK_NULL_HANDLE,
                                                         VK_IMAGE_LAYOUT_UNDEFINED};

        std::vector<VkWriteDescriptorSet> writes = {create_image_descriptor_write(resources.material_descriptor_set, 0,
                                                                                  VK_DESCRIPTOR_TYPE_SAMPLER,
                                                                                  &diffuseMapSampler),
                                                    diffuseMapBindlessImageWrite};

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    VkPipeline create_forward_pipeline(ReaperRoot& root, VulkanBackend& backend, VkPipelineLayout pipeline_layout)
    {
        VkShaderModule blitShaderVS = vulkan_create_shader_module(backend.device, "build/shader/forward.vert.spv");
        VkShaderModule blitShaderFS = vulkan_create_shader_module(backend.device, "build/shader/forward.frag.spv");

        const char* entryPoint = "main";

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, blitShaderVS,
             entryPoint, nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
             blitShaderFS, entryPoint, nullptr}};

        VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, VK_FLAGS_NONE, 0, nullptr, 0, nullptr};

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {
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
            ForwardUseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS,
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

        VkPipelineCache cache = VK_NULL_HANDLE;

        const std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo    blitDynamicState = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,
            0,
            static_cast<u32>(dynamicStates.size()),
            dynamicStates.data(),
        };

        const VkFormat color_format = PixelFormatToVulkan(ForwardHDRColorFormat);
        const VkFormat depth_format = PixelFormatToVulkan(ForwardDepthFormat);

        VkPipelineCreationFeedback              feedback = {};
        std::vector<VkPipelineCreationFeedback> feedback_stages(shaderStages.size());
        VkPipelineCreationFeedbackCreateInfo    feedback_info = {
            VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO,
            nullptr,
            &feedback,
            static_cast<u32>(shaderStages.size()),
            feedback_stages.data(),
        };

        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            &feedback_info,
            0, // viewMask;
            1,
            &color_format,
            depth_format,
            VK_FORMAT_UNDEFINED,
        };

        VkGraphicsPipelineCreateInfo blitPipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                               &pipelineRenderingCreateInfo,
                                                               VK_FLAGS_NONE,
                                                               static_cast<u32>(shaderStages.size()),
                                                               shaderStages.data(),
                                                               &vertexInputStateInfo,
                                                               &inputAssemblyInfo,
                                                               nullptr,
                                                               &blitViewportStateInfo,
                                                               &blitRasterStateInfo,
                                                               &blitMSStateInfo,
                                                               &blitDepthStencilInfo,
                                                               &blitBlendStateInfo,
                                                               &blitDynamicState,
                                                               pipeline_layout,
                                                               VK_NULL_HANDLE,
                                                               0,
                                                               VK_NULL_HANDLE,
                                                               -1};

        VkPipeline pipeline = VK_NULL_HANDLE;
        Assert(vkCreateGraphicsPipelines(backend.device, cache, 1, &blitPipelineCreateInfo, nullptr, &pipeline)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created blit pipeline with handle: {}", static_cast<void*>(pipeline));

        log_debug(root, "- total time = {}ms, vs = {}ms, fs = {}ms", feedback.duration / 1000,
                  feedback_stages[0].duration / 1000, feedback_stages[1].duration / 1000);

        vkDestroyShaderModule(backend.device, blitShaderVS, nullptr);
        vkDestroyShaderModule(backend.device, blitShaderFS, nullptr);

        return pipeline;
    }

    VkDescriptorSet create_forward_pass_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
                                                       VkDescriptorSetLayout layout)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              backend.global_descriptor_pool, 1, &layout};

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptor_set) == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptor_set));

        return descriptor_set;
    }

    VkDescriptorSet create_material_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
                                                   VkDescriptorSetLayout layout)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              backend.global_descriptor_pool, 1, &layout};

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptor_set) == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptor_set));

        return descriptor_set;
    }
} // namespace

ForwardPassResources create_forward_pass_resources(ReaperRoot& root, VulkanBackend& backend)
{
    ForwardPassResources resources = {};

    resources.pipe.descSetLayout = create_descriptor_set_layout_0(backend);
    resources.pipe.descSetLayout2 = create_descriptor_set_layout_1(backend);

    std::vector<VkDescriptorSetLayout> passDescriptorSetLayouts = {
        resources.pipe.descSetLayout,
        resources.pipe.descSetLayout2,
    };

    resources.pipe.pipelineLayout = create_pipeline_layout(backend.device, passDescriptorSetLayouts);

    resources.pipe.pipeline = create_forward_pipeline(root, backend, resources.pipe.pipelineLayout);

    resources.passConstantBuffer =
        create_buffer(root, backend.device, "Forward Pass Constant buffer",
                      DefaultGPUBufferProperties(1, sizeof(ForwardPassParams), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.instancesConstantBuffer = create_buffer(
        root, backend.device, "Forward Instance buffer",
        DefaultGPUBufferProperties(DrawInstanceCountMax, sizeof(ForwardInstanceParams), GPUBufferUsage::StorageBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.descriptor_set = create_forward_pass_descriptor_set(root, backend, resources.pipe.descSetLayout);
    resources.material_descriptor_set = create_material_descriptor_set(root, backend, resources.pipe.descSetLayout2);

    return resources;
}

void destroy_forward_pass_resources(VulkanBackend& backend, ForwardPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.pipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.descSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.descSetLayout2, nullptr);

    vmaDestroyBuffer(backend.vma_instance, resources.passConstantBuffer.handle,
                     resources.passConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.instancesConstantBuffer.handle,
                     resources.instancesConstantBuffer.allocation);
}

void update_forward_pass_descriptor_sets(VulkanBackend& backend, const ForwardPassResources& resources,
                                         const SamplerResources&  sampler_resources,
                                         const MaterialResources& material_resources, const MeshCache& mesh_cache,
                                         const LightingPassResources&    lighting_resources,
                                         const nonstd::span<VkImageView> shadow_map_views)
{
    update_descriptor_set_0(backend, resources, sampler_resources, mesh_cache, lighting_resources, shadow_map_views);
    update_descriptor_set_1(backend, resources, sampler_resources, material_resources);
}

void upload_forward_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                         ForwardPassResources& pass_resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.forward_instances.empty())
        return;

    upload_buffer_data(backend.device, backend.vma_instance, pass_resources.passConstantBuffer,
                       &prepared.forward_pass_constants, sizeof(ForwardPassParams));

    upload_buffer_data(backend.device, backend.vma_instance, pass_resources.instancesConstantBuffer,
                       prepared.forward_instances.data(),
                       prepared.forward_instances.size() * sizeof(ForwardInstanceParams));
}

void record_forward_pass_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                        const ForwardPassResources& pass_resources, const CullResources& cull_resources,
                                        VkExtent2D backbufferExtent, VkImageView hdrBufferView,
                                        VkImageView depthBufferView)
{
    if (prepared.forward_instances.empty())
        return;

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipe.pipeline);

    const glm::fvec4 clearColor = {0.1f, 0.1f, 0.1f, 0.0f};
    const float      depthClearValue = ForwardUseReverseZ ? 0.f : 1.f;
    const VkRect2D   blitPassRect = default_vk_rect(backbufferExtent);

    const VkRenderingAttachmentInfo color_attachment = {
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        nullptr,
        hdrBufferView,
        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        VK_RESOLVE_MODE_NONE,
        VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VkClearColor(clearColor),
    };

    const VkRenderingAttachmentInfo depth_attachment = {
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        nullptr,
        depthBufferView,
        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        VK_RESOLVE_MODE_NONE,
        VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VkClearDepthStencil(depthClearValue, 0),
    };

    VkRenderingInfo renderingInfo = {
        VK_STRUCTURE_TYPE_RENDERING_INFO,
        nullptr,
        VK_FLAGS_NONE,
        blitPassRect,
        1, // layerCount
        0, // viewMask
        1,
        &color_attachment,
        &depth_attachment,
        nullptr,
    };

    vkCmdBeginRendering(cmdBuffer.handle, &renderingInfo);

    const VkViewport blitViewport = default_vk_viewport(blitPassRect);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &blitViewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &blitPassRect);

    const CullingDrawParams draw_params = get_culling_draw_params(prepared.forward_culling_pass_index);

    vkCmdBindIndexBuffer(cmdBuffer.handle, cull_resources.dynamicIndexBuffer.handle, draw_params.index_buffer_offset,
                         draw_params.index_type);

    std::array<VkDescriptorSet, 2> pass_descriptors = {
        pass_resources.descriptor_set,
        pass_resources.material_descriptor_set,
    };

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.pipe.pipelineLayout, 0,
                            static_cast<u32>(pass_descriptors.size()), pass_descriptors.data(), 0, nullptr);

    vkCmdDrawIndexedIndirectCount(cmdBuffer.handle, cull_resources.indirectDrawBuffer.handle,
                                  draw_params.command_buffer_offset, cull_resources.countersBuffer.handle,
                                  draw_params.counter_buffer_offset, draw_params.command_buffer_max_count,
                                  cull_resources.indirectDrawBuffer.properties.element_size_bytes);
    vkCmdEndRendering(cmdBuffer.handle);
}
} // namespace Reaper
