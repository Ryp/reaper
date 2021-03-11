////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "SwapchainPass.h"

#include "ShadowConstants.h"

#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Shader.h"
#include "renderer/vulkan/SwapchainRendererBase.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "renderer/shader/share/swapchain.hlsl"

namespace Reaper
{
SwapchainPipelineInfo create_swapchain_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass)
{
    VkShaderModule        shaderFS = VK_NULL_HANDLE;
    VkShaderModule        shaderVS = VK_NULL_HANDLE;
    const char*           fileNameVS = "./build/shader/fullscreen_triangle.vert.spv";
    const char*           fileNameFS = "./build/shader/swapchain_write.frag.spv";
    const char*           entryPoint = "main";
    VkSpecializationInfo* specialization = nullptr;

    vulkan_create_shader_module(shaderFS, backend.device, fileNameFS);
    vulkan_create_shader_module(shaderVS, backend.device, fileNameVS);

    std::vector<VkPipelineShaderStageCreateInfo> blitShaderStages = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, shaderVS,
         entryPoint, specialization},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, shaderFS,
         entryPoint, specialization}};

    VkPipelineVertexInputStateCreateInfo blitVertexInputStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, VK_FLAGS_NONE, 0, nullptr, 0, nullptr};

    VkPipelineInputAssemblyStateCreateInfo blitInputAssemblyInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, VK_FLAGS_NONE,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};

    VkPipelineViewportStateCreateInfo blitViewportStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
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

    VkPipelineMultisampleStateCreateInfo blitMSStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
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
        false,
        false,
        VK_COMPARE_OP_GREATER,
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

    VkPipelineColorBlendStateCreateInfo blitBlendStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                              nullptr,
                                                              VK_FLAGS_NONE,
                                                              VK_FALSE,
                                                              VK_LOGIC_OP_COPY,
                                                              1,
                                                              &blitBlendAttachmentState,
                                                              {0.0f, 0.0f, 0.0f, 0.0f}};

    std::array<VkDescriptorSetLayoutBinding, 3> descriptorSetLayoutBinding = {
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };

    std::array<VkDescriptorBindingFlags, 3> bindingFlags = {VK_FLAGS_NONE, VK_FLAGS_NONE, VK_FLAGS_NONE};

    const VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr, bindingFlags.size(),
        bindingFlags.data()};

    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &descriptorSetLayoutBindingFlags, 0,
        static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

    VkDescriptorSetLayout descriptorSetLayoutCB = VK_NULL_HANDLE;
    Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayoutCB)
           == VK_SUCCESS);

    log_debug(root, "vulkan: created descriptor set layout with handle: {}", static_cast<void*>(descriptorSetLayoutCB));

    VkPipelineLayoutCreateInfo blitPipelineLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, VK_FLAGS_NONE, 1, &descriptorSetLayoutCB, 0, nullptr};

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    Assert(vkCreatePipelineLayout(backend.device, &blitPipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);
    log_debug(root, "vulkan: created blit pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

    VkPipelineCache cache = VK_NULL_HANDLE;

    const std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo    blitDynamicState = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, dynamicStates.size(), dynamicStates.data(),
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

    vkDestroyShaderModule(backend.device, shaderVS, nullptr);
    vkDestroyShaderModule(backend.device, shaderFS, nullptr);

    return SwapchainPipelineInfo{pipeline, pipelineLayout, descriptorSetLayoutCB};
}

namespace
{
    VkRenderPass create_swapchain_pass(ReaperRoot& /*root*/, VulkanBackend& backend, VkFormat swapchain_format)
    {
        const VkAttachmentDescription2 color_attachment = {VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                                                           nullptr,
                                                           VK_FLAGS_NONE,
                                                           swapchain_format,
                                                           VK_SAMPLE_COUNT_1_BIT,
                                                           VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                           VK_ATTACHMENT_STORE_OP_STORE,
                                                           VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                           VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                           VK_IMAGE_LAYOUT_UNDEFINED,
                                                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

        std::array<VkAttachmentDescription2, 1> attachmentDescriptions = {
            color_attachment,
        };

        VkAttachmentReference2 colorReference = {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, 0,
                                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT};

        VkSubpassDescription2 subpassDescription = {};
        subpassDescription.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
        subpassDescription.pNext = nullptr;
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &colorReference;
        subpassDescription.pDepthStencilAttachment = nullptr;

        const VkSubpassDependency2 color_dep = {
            VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
            nullptr,
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
        };

        // Use subpass dependencies for layout transitions
        std::array<VkSubpassDependency2, 1> dependencies = {color_dep};

        // Create the actual renderpass
        VkRenderPassCreateInfo2 renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
        renderPassInfo.pNext = nullptr;
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

    // FIXME de-duplicate
    VkFramebufferAttachmentImageInfo get_attachment_info_from_image_properties(const GPUTextureProperties& properties,
                                                                               VkFormat* output_format_ptr)
    {
        *output_format_ptr = PixelFormatToVulkan(properties.format);

        return VkFramebufferAttachmentImageInfo{
            VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
            nullptr,
            GetVulkanCreateFlags(properties),           // VkImageCreateFlags    flags;
            GetVulkanUsageFlags(properties.usageFlags), // VkImageUsageFlags     usage;
            properties.width,                           // uint32_t              width;
            properties.height,                          // uint32_t              height;
            1,                                          // uint32_t              layerCount;
            1,                                          // uint32_t              viewFormatCount;
            output_format_ptr                           // const VkFormat*       pViewFormats;
        };
    }

    VkFramebuffer create_swapchain_framebuffer(VulkanBackend& backend, SwapchainPassResources& resources)
    {
        std::array<VkFormat, 1>                         formats; // Kept alive until vkCreateFramebuffer() call
        std::array<VkFramebufferAttachmentImageInfo, 1> framebufferAttachmentInfo = {
            get_attachment_info_from_image_properties(resources.swapchain_properties, &formats[0]),
        };

        VkFramebufferAttachmentsCreateInfo framebufferAttachmentsInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO, nullptr, framebufferAttachmentInfo.size(),
            framebufferAttachmentInfo.data()};

        VkFramebufferCreateInfo framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType                sType
            &framebufferAttachmentsInfo,               // const void                    *pNext
            VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,       // VkFramebufferCreateFlags       flags
            resources.swapchainRenderPass,             // VkRenderPass                   renderPass
            framebufferAttachmentInfo.size(),          // uint32_t                       attachmentCount
            nullptr,                                   // const VkImageView             *pAttachments
            framebufferAttachmentInfo[0].width,        // uint32_t                       width
            framebufferAttachmentInfo[0].height,       // uint32_t                       height
            1                                          // uint32_t                       layers
        };

        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        Assert(vkCreateFramebuffer(backend.device, &framebuffer_create_info, nullptr, &framebuffer) == VK_SUCCESS);

        return framebuffer;
    }

    void create_swapchain_pass_resizable_resources(ReaperRoot& /*root*/, VulkanBackend& backend,
                                                   SwapchainPassResources& resources, glm::uvec2 /*extent*/)
    {
        // FIXME Get all infos from surface caps
        Assert(backend.presentInfo.usageFlags == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        const GPUTextureProperties swapchain_properties = {
            backend.presentInfo.surfaceExtent.width,
            backend.presentInfo.surfaceExtent.height,
            1,
            VulkanToPixelFormat(backend.presentInfo.surfaceFormat.format),
            1,
            1,
            1,
            GPUTextureUsage::ColorAttachment,
            GPUMiscFlags::None,
        };

        resources.swapchain_properties = swapchain_properties;

        resources.swapchain_framebuffer = create_swapchain_framebuffer(backend, resources);
    }

    void destroy_swapchain_pass_resizable_resources(VulkanBackend& backend, SwapchainPassResources& resources)
    {
        vkDestroyFramebuffer(backend.device, resources.swapchain_framebuffer, nullptr);
    }
} // namespace

SwapchainPassResources create_swapchain_pass_resources(ReaperRoot& root, VulkanBackend& backend, glm::uvec2 extent)
{
    SwapchainPassResources resources = {};

    resources.passConstantBuffer =
        create_buffer(root, backend.device, "Swapchain Pass Constant buffer",
                      DefaultGPUBufferProperties(1, sizeof(SwapchainPassParams), GPUBufferUsage::UniformBuffer),
                      backend.vma_instance);

    resources.swapchainRenderPass = create_swapchain_pass(root, backend, backend.presentInfo.surfaceFormat.format);

    create_swapchain_pass_resizable_resources(root, backend, resources, extent);

    resources.swapchainPipe = create_swapchain_pipeline(root, backend, resources.swapchainRenderPass);

    VkSamplerCreateInfo shadowMapSamplerCreateInfo = {};
    shadowMapSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    shadowMapSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    shadowMapSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    shadowMapSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerCreateInfo.anisotropyEnable = VK_FALSE;
    shadowMapSamplerCreateInfo.maxAnisotropy = 16;
    shadowMapSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    shadowMapSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    shadowMapSamplerCreateInfo.compareEnable = VK_FALSE;
    shadowMapSamplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    shadowMapSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadowMapSamplerCreateInfo.mipLodBias = 0.f;
    shadowMapSamplerCreateInfo.minLod = 0.f;
    shadowMapSamplerCreateInfo.maxLod = FLT_MAX;

    Assert(vkCreateSampler(backend.device, &shadowMapSamplerCreateInfo, nullptr, &resources.shadowMapSampler)
           == VK_SUCCESS);
    log_debug(root, "vulkan: created sampler with handle: {}", static_cast<void*>(resources.shadowMapSampler));

    return resources;
}

void destroy_swapchain_pass_resources(VulkanBackend& backend, SwapchainPassResources& resources)
{
    vkDestroySampler(backend.device, resources.shadowMapSampler, nullptr);

    vkDestroyPipeline(backend.device, resources.swapchainPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.swapchainPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.swapchainPipe.descSetLayout, nullptr);

    vkDestroyRenderPass(backend.device, resources.swapchainRenderPass, nullptr);

    destroy_swapchain_pass_resizable_resources(backend, resources);

    vmaDestroyBuffer(backend.vma_instance, resources.passConstantBuffer.buffer,
                     resources.passConstantBuffer.allocation);
}

void resize_swapchain_pass_resources(ReaperRoot& root, VulkanBackend& backend, SwapchainPassResources& resources,
                                     glm::uvec2 extent)
{
    destroy_swapchain_pass_resizable_resources(backend, resources);

    create_swapchain_pass_resizable_resources(root, backend, resources, extent);
}

VkDescriptorSet create_swapchain_pass_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
                                                     const SwapchainPassResources& resources, VkImageView texture_view)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                          backend.frame_descriptor_pool, 1,
                                                          &resources.swapchainPipe.descSetLayout};

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptor_set) == VK_SUCCESS);
    log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptor_set));

    const VkDescriptorBufferInfo drawDescPassParams = default_descriptor_buffer_info(resources.passConstantBuffer);
    const VkDescriptorImageInfo  drawDescShadowMapSampler = {resources.shadowMapSampler, VK_NULL_HANDLE,
                                                            VK_IMAGE_LAYOUT_UNDEFINED};
    const VkDescriptorImageInfo  descTexture = {VK_NULL_HANDLE, texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    std::array<VkWriteDescriptorSet, 3> drawPassDescriptorSetWrites = {
        create_buffer_descriptor_write(descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &drawDescPassParams),
        create_image_descriptor_write(descriptor_set, 1, VK_DESCRIPTOR_TYPE_SAMPLER, &drawDescShadowMapSampler),
        create_image_descriptor_write(descriptor_set, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &descTexture),
    };

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(drawPassDescriptorSetWrites.size()),
                           drawPassDescriptorSetWrites.data(), 0, nullptr);

    return descriptor_set;
}
} // namespace Reaper
