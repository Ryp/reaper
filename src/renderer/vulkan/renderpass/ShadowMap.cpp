////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ShadowMap.h"

#include "Culling.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Shader.h"
#include "renderer/vulkan/SwapchainRendererBase.h"
#include "renderer/vulkan/api/Vulkan.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include <array>

#include "renderer/shader/share/shadow_map_pass.hlsl"

namespace Reaper
{
constexpr bool UseReverseZ = true;
constexpr u32  ShadowInstanceCountMax = 512;
constexpr u32  MaxShadowPassCount = 4;

namespace
{
    // FIXME dedup
    VkRect2D default_vk_rect(VkExtent2D image_extent) { return VkRect2D{{0, 0}, image_extent}; }

    // FIXME dedup
    VkViewport default_vk_viewport(VkRect2D output_rect)
    {
        return VkViewport{static_cast<float>(output_rect.offset.x),
                          static_cast<float>(output_rect.offset.y),
                          static_cast<float>(output_rect.extent.width),
                          static_cast<float>(output_rect.extent.height),
                          0.0f,
                          1.0f};
    }

    // FIXME de-duplicate
    VkClearValue VkClearDepthStencil(float depth, u32 stencil)
    {
        VkClearValue clearValue;

        clearValue.depthStencil.depth = depth;
        clearValue.depthStencil.stencil = stencil;

        return clearValue;
    }

    VkRenderPass create_shadow_raster_pass(ReaperRoot& /*root*/, VulkanBackend& backend)
    {
        constexpr u32 depth_index = 0;

        std::array<VkAttachmentDescription2, 1> attachmentDescriptions = {};

        // Depth attachment
        attachmentDescriptions[depth_index].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
        attachmentDescriptions[depth_index].pNext = nullptr;
        attachmentDescriptions[depth_index].format = PixelFormatToVulkan(ShadowMapFormat);
        attachmentDescriptions[depth_index].samples = VK_SAMPLE_COUNT_1_BIT; // NOTE: hardcoded, has no reason to change
        attachmentDescriptions[depth_index].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescriptions[depth_index].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDescriptions[depth_index].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescriptions[depth_index].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescriptions[depth_index].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescriptions[depth_index].finalLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

        VkAttachmentReference2 depthReference = {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, depth_index,
                                                 VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT};

        VkSubpassDescription2 subpassDescription = {};
        subpassDescription.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
        subpassDescription.pNext = nullptr;
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 0;
        subpassDescription.pColorAttachments = nullptr;
        subpassDescription.pDepthStencilAttachment = &depthReference;

        // Use subpass dependencies for layout transitions
        std::array<VkSubpassDependency2, 1> dependencies;

        dependencies[depth_index].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        dependencies[depth_index].pNext = nullptr;
        dependencies[depth_index].srcSubpass = 0;
        dependencies[depth_index].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[depth_index].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[depth_index].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[depth_index].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[depth_index].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[depth_index].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencies[depth_index].viewOffset = 0;

        // Create the actual renderpass
        VkRenderPassCreateInfo2 renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
        renderPassInfo.pNext = nullptr;
        renderPassInfo.flags = VK_FLAGS_NONE;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
        renderPassInfo.pAttachments = attachmentDescriptions.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassDescription;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();
        renderPassInfo.correlatedViewMaskCount = 0;
        renderPassInfo.pCorrelatedViewMasks = nullptr;

        VkRenderPass renderPass = VK_NULL_HANDLE;
        Assert(vkCreateRenderPass2(backend.device, &renderPassInfo, nullptr, &renderPass) == VK_SUCCESS);

        return renderPass;
    }

    ShadowMapPipelineInfo create_shadow_map_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass)
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
            UseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS,
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

        VkGraphicsPipelineCreateInfo blitPipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                               nullptr,
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
                                                               renderPass,
                                                               0,
                                                               VK_NULL_HANDLE,
                                                               -1};

        VkPipeline pipeline = VK_NULL_HANDLE;
        Assert(vkCreateGraphicsPipelines(backend.device, cache, 1, &blitPipelineCreateInfo, nullptr, &pipeline)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created blit pipeline with handle: {}", static_cast<void*>(pipeline));

        Assert(backend.physicalDeviceInfo.graphicsQueueIndex == backend.physicalDeviceInfo.presentQueueIndex);

        vkDestroyShaderModule(backend.device, blitShaderVS, nullptr);
        vkDestroyShaderModule(backend.device, blitShaderFS, nullptr);

        return ShadowMapPipelineInfo{pipeline, pipelineLayout, descriptorSetLayoutCB};
    }
} // namespace

GPUTextureProperties get_shadow_map_texture_properties(glm::uvec2 size)
{
    GPUTextureProperties properties = DefaultGPUTextureProperties(size.x, size.y, ShadowMapFormat);

    properties.usageFlags =
        GPUTextureUsage::DepthStencilAttachment | GPUTextureUsage::InputAttachment | GPUTextureUsage::Sampled;

    return properties;
}

VkFramebuffer create_shadow_map_framebuffer(VulkanBackend& backend, VkRenderPass renderPass,
                                            const GPUTextureProperties& properties)
{
    VkFormat format = PixelFormatToVulkan(properties.format);

    VkFramebufferAttachmentImageInfo shadowMapFramebufferAttachmentInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
        nullptr,
        GetVulkanCreateFlags(properties),           // VkImageCreateFlags    flags;
        GetVulkanUsageFlags(properties.usageFlags), // VkImageUsageFlags     usage;
        properties.width,                           // uint32_t              width;
        properties.height,                          // uint32_t              height;
        1,                                          // uint32_t              layerCount;
        1,                                          // uint32_t              viewFormatCount;
        &format                                     // const VkFormat*       pViewFormats;
    };

    VkFramebufferAttachmentsCreateInfo shadowMapFramebufferAttachmentsInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO, nullptr, 1, &shadowMapFramebufferAttachmentInfo};

    VkFramebufferCreateInfo shadowMapFramebufferInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType                sType
        &shadowMapFramebufferAttachmentsInfo,      // const void                    *pNext
        VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,       // VkFramebufferCreateFlags       flags
        renderPass,                                // VkRenderPass                   renderPass
        1,                                         // uint32_t                       attachmentCount
        nullptr,                                   // const VkImageView             *pAttachments
        properties.width,                          // uint32_t                       width
        properties.height,                         // uint32_t                       height
        1                                          // uint32_t                       layers
    };

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    Assert(vkCreateFramebuffer(backend.device, &shadowMapFramebufferInfo, nullptr, &framebuffer) == VK_SUCCESS);

    return framebuffer;
}

ShadowMapResources create_shadow_map_resources(ReaperRoot& root, VulkanBackend& backend)
{
    ShadowMapResources resources = {};

    resources.shadowMapPass = create_shadow_raster_pass(root, backend);
    resources.pipe = create_shadow_map_pipeline(root, backend, resources.shadowMapPass);

    resources.shadowMapPassConstantBuffer = create_buffer(
        root, backend.device, "Shadow Map Pass Constant buffer",
        DefaultGPUBufferProperties(MaxShadowPassCount, sizeof(ShadowMapPassParams), GPUBufferUsage::UniformBuffer),
        backend.vma_instance);

    resources.shadowMapInstanceConstantBuffer =
        create_buffer(root, backend.device, "Shadow Map Instance Constant buffer",
                      DefaultGPUBufferProperties(ShadowInstanceCountMax, sizeof(ShadowMapInstanceParams),
                                                 GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    return resources;
}

void destroy_shadow_map_resources(VulkanBackend& backend, ShadowMapResources& resources)
{
    for (u32 i = 0; i < resources.shadowMap.size(); i++)
    {
        vmaDestroyImage(backend.vma_instance, resources.shadowMap[i].handle, resources.shadowMap[i].allocation);
        vkDestroyImageView(backend.device, resources.shadowMapView[i], nullptr);
        vkDestroyFramebuffer(backend.device, resources.shadowMapFramebuffer[i], nullptr);
    }

    vmaDestroyBuffer(backend.vma_instance, resources.shadowMapPassConstantBuffer.buffer,
                     resources.shadowMapPassConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.shadowMapInstanceConstantBuffer.buffer,
                     resources.shadowMapInstanceConstantBuffer.allocation);

    vkDestroyRenderPass(backend.device, resources.shadowMapPass, nullptr);

    vkDestroyPipeline(backend.device, resources.pipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.pipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.pipe.descSetLayout, nullptr);
}

ShadowPassResources create_shadow_map_pass_descriptor_sets(ReaperRoot& root, VulkanBackend& backend,
                                                           const ShadowMapResources& resources,
                                                           const ShadowPassData&     shadow_pass)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                          backend.frame_descriptor_pool, 1,
                                                          &resources.pipe.descSetLayout};

    VkDescriptorSet shadowMapPassDescriptorSet = VK_NULL_HANDLE;

    Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &shadowMapPassDescriptorSet)
           == VK_SUCCESS);
    log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(shadowMapPassDescriptorSet));

    const VkDescriptorBufferInfo shadowMapDescPassParams =
        get_vk_descriptor_buffer_info(resources.shadowMapPassConstantBuffer, GPUBufferView{shadow_pass.pass_index, 1});
    const VkDescriptorBufferInfo shadowMapDescInstanceParams =
        get_vk_descriptor_buffer_info(resources.shadowMapInstanceConstantBuffer,
                                      GPUBufferView{shadow_pass.instance_offset, shadow_pass.instance_count});

    std::array<VkWriteDescriptorSet, 2> shadowMapPassDescriptorSetWrites = {
        create_buffer_descriptor_write(shadowMapPassDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                       &shadowMapDescPassParams),
        create_buffer_descriptor_write(shadowMapPassDescriptorSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &shadowMapDescInstanceParams),
    };

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(shadowMapPassDescriptorSetWrites.size()),
                           shadowMapPassDescriptorSetWrites.data(), 0, nullptr);

    return ShadowPassResources{shadowMapPassDescriptorSet};
}

void prepare_shadow_map_objects(ReaperRoot& root, VulkanBackend& backend, const PreparedData& prepared,
                                ShadowMapResources& pass_resources)
{
    for (u32 i = 0; i < pass_resources.shadowMap.size(); i++)
    {
        vmaDestroyImage(backend.vma_instance, pass_resources.shadowMap[i].handle,
                        pass_resources.shadowMap[i].allocation);
        vkDestroyImageView(backend.device, pass_resources.shadowMapView[i], nullptr);
        vkDestroyFramebuffer(backend.device, pass_resources.shadowMapFramebuffer[i], nullptr);
    }

    pass_resources.passes.clear();
    pass_resources.shadowMap.clear();
    pass_resources.shadowMapView.clear();
    pass_resources.shadowMapFramebuffer.clear();
    for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
    {
        ShadowPassResources& shadow_map_pass_resources = pass_resources.passes.emplace_back();
        shadow_map_pass_resources = create_shadow_map_pass_descriptor_sets(root, backend, pass_resources, shadow_pass);

        const GPUTextureProperties texture_properties = get_shadow_map_texture_properties(shadow_pass.shadow_map_size);

        ImageInfo& shadow_map = pass_resources.shadowMap.emplace_back();

        shadow_map = create_image(root, backend.device, "Shadow Map", texture_properties, backend.vma_instance);

        pass_resources.shadowMapView.push_back(create_depth_image_view(root, backend.device, shadow_map));

        pass_resources.shadowMapFramebuffer.push_back(
            create_shadow_map_framebuffer(backend, pass_resources.shadowMapPass, shadow_map.properties));
    }
}

void shadow_map_prepare_buffers(VulkanBackend& backend, const PreparedData& prepared, ShadowMapResources& resources)
{
    upload_buffer_data(backend.device, backend.vma_instance, resources.shadowMapPassConstantBuffer,
                       prepared.shadow_pass_params.data(),
                       prepared.shadow_pass_params.size() * sizeof(ShadowMapPassParams));

    upload_buffer_data(backend.device, backend.vma_instance, resources.shadowMapInstanceConstantBuffer,
                       prepared.shadow_instance_params.data(),
                       prepared.shadow_instance_params.size() * sizeof(ShadowMapInstanceParams));
}

void record_shadow_map_command_buffer(const CullOptions& cull_options, VkCommandBuffer cmdBuffer,
                                      VulkanBackend& backend, const PreparedData& prepared,
                                      ShadowMapResources& resources, const CullResources& cull_resources,
                                      VkBuffer vertex_buffer)
{
    for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
    {
        // REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Shadow Pass", MP_DARKGOLDENROD);

        const ShadowPassResources& shadow_map_pass_resources = resources.passes[shadow_pass.pass_index];

        const VkClearValue clearValue =
            VkClearDepthStencil(UseReverseZ ? 0.f : 1.f, 0); // NOTE: handle reverse Z more gracefully
        const VkExtent2D framebuffer_extent = {shadow_pass.shadow_map_size.x, shadow_pass.shadow_map_size.y};
        const VkRect2D   pass_rect = default_vk_rect(framebuffer_extent);

        const VkImageView   shadowMapView = resources.shadowMapView[shadow_pass.pass_index];
        const VkFramebuffer shadowMapFramebuffer = resources.shadowMapFramebuffer[shadow_pass.pass_index];

        VkRenderPassAttachmentBeginInfo shadowMapRenderPassAttachment = {
            VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO, nullptr, 1, &shadowMapView};

        VkRenderPassBeginInfo shadowMapRenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                              &shadowMapRenderPassAttachment,
                                                              resources.shadowMapPass,
                                                              shadowMapFramebuffer,
                                                              pass_rect,
                                                              1,
                                                              &clearValue};

        vkCmdBeginRenderPass(cmdBuffer, &shadowMapRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipe.pipeline);

        const VkViewport viewport = default_vk_viewport(pass_rect);

        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
        vkCmdSetScissor(cmdBuffer, 0, 1, &pass_rect);

        std::vector<VkBuffer> vertexBuffers = {
            vertex_buffer,
        };
        std::vector<VkDeviceSize> vertexBufferOffsets = {0};

        Assert(vertexBuffers.size() == vertexBufferOffsets.size());
        vkCmdBindIndexBuffer(cmdBuffer, cull_resources.dynamicIndexBuffer.buffer, 0, get_vk_culling_index_type());
        vkCmdBindVertexBuffers(cmdBuffer, 0, static_cast<u32>(vertexBuffers.size()), vertexBuffers.data(),
                               vertexBufferOffsets.data());
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipe.pipelineLayout, 0, 1,
                                &shadow_map_pass_resources.descriptor_set, 0, nullptr);

        const u32 draw_buffer_offset =
            shadow_pass.culling_pass_index * MaxIndirectDrawCount * sizeof(VkDrawIndexedIndirectCommand);
        const u32 draw_buffer_max_count = MaxIndirectDrawCount;

        if (cull_options.use_compacted_draw)
        {
            const u32 draw_buffer_count_offset = shadow_pass.culling_pass_index * 1 * sizeof(u32);
            vkCmdDrawIndexedIndirectCount(cmdBuffer, cull_resources.compactIndirectDrawBuffer.buffer,
                                          draw_buffer_offset, cull_resources.compactIndirectDrawCountBuffer.buffer,
                                          draw_buffer_count_offset, draw_buffer_max_count,
                                          cull_resources.compactIndirectDrawBuffer.descriptor.elementSize);
        }
        else
        {
            const u32 draw_buffer_count_offset = shadow_pass.culling_pass_index * IndirectDrawCountCount * sizeof(u32);
            vkCmdDrawIndexedIndirectCount(cmdBuffer, cull_resources.indirectDrawBuffer.buffer, draw_buffer_offset,
                                          cull_resources.indirectDrawCountBuffer.buffer, draw_buffer_count_offset,
                                          draw_buffer_max_count,
                                          cull_resources.indirectDrawBuffer.descriptor.elementSize);
        }

        vkCmdEndRenderPass(cmdBuffer);
    }

    {
        // REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Barrier", MP_RED);

        std::vector<VkImageMemoryBarrier> shadowMapImageBarrierInfo;

        for (const ShadowPassData& shadow_pass : prepared.shadow_passes)
        {
            shadowMapImageBarrierInfo.push_back(VkImageMemoryBarrier{
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                0,
                VK_ACCESS_MEMORY_READ_BIT,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                backend.physicalDeviceInfo.graphicsQueueIndex,
                backend.physicalDeviceInfo.graphicsQueueIndex,
                resources.shadowMap[shadow_pass.pass_index].handle,
                {VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}});
        }

        // FIXME is this a noop when there's no barriers?
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                             nullptr, 0, nullptr, static_cast<u32>(shadowMapImageBarrierInfo.size()),
                             shadowMapImageBarrierInfo.data());
    }
}
} // namespace Reaper
