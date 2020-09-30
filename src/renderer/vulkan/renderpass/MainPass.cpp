////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MainPass.h"

#include "ShadowConstants.h"

#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Shader.h"
#include "renderer/vulkan/SwapchainRendererBase.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "renderer/shader/share/draw.hlsl"

namespace Reaper
{
constexpr u32  DrawInstanceCountMax = 512;
constexpr bool UseReverseZ = true;

VkRenderPass create_main_pass(ReaperRoot& /*root*/, VulkanBackend& backend, const GPUTextureProperties& depthProperties)
{
    // FIXME build this at swapchain creation
    GPUTextureProperties swapchainProperties = depthProperties;
    swapchainProperties.usageFlags = GPUTextureUsage::Swapchain;
    swapchainProperties.miscFlags = 0;

    constexpr u32 color_index = 0;
    constexpr u32 depth_index = 1;

    std::array<VkAttachmentDescription2, 2> attachmentDescriptions = {};

    // Color attachment
    attachmentDescriptions[color_index].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    attachmentDescriptions[color_index].pNext = nullptr;
    attachmentDescriptions[color_index].format = backend.presentInfo.surfaceFormat.format; // FIXME
    attachmentDescriptions[color_index].samples = SampleCountToVulkan(swapchainProperties.sampleCount);
    attachmentDescriptions[color_index].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[color_index].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[color_index].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[color_index].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[color_index].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachmentDescriptions[color_index].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    attachmentDescriptions[depth_index].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    attachmentDescriptions[depth_index].pNext = nullptr;
    attachmentDescriptions[depth_index].format = PixelFormatToVulkan(depthProperties.format);
    attachmentDescriptions[depth_index].samples = SampleCountToVulkan(depthProperties.sampleCount);
    attachmentDescriptions[depth_index].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[depth_index].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[depth_index].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[depth_index].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[depth_index].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[depth_index].finalLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

    VkAttachmentReference2 colorReference = {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, color_index,
                                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT};
    VkAttachmentReference2 depthReference = {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, depth_index,
                                             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT};

    VkSubpassDescription2 subpassDescription = {};
    subpassDescription.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
    subpassDescription.pNext = nullptr;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;
    subpassDescription.pDepthStencilAttachment = &depthReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency2, 2> dependencies;

    dependencies[color_index].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    dependencies[color_index].pNext = nullptr;
    dependencies[color_index].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[color_index].dstSubpass = 0;
    dependencies[color_index].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[color_index].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[color_index].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[color_index].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[color_index].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[color_index].viewOffset = 0;

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

MainPipelineInfo create_main_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass)
{
    VkShaderModule        blitShaderFS = VK_NULL_HANDLE;
    VkShaderModule        blitShaderVS = VK_NULL_HANDLE;
    const char*           fileNameVS = "./build/shader/mesh_transformed_shaded.vert.spv";
    const char*           fileNameFS = "./build/shader/mesh_transformed_shaded.frag.spv";
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
        {
            1,                          // binding
            sizeof(hlsl_float3),        // stride
            VK_VERTEX_INPUT_RATE_VERTEX // input rate
        },
        {
            2,                          // binding
            sizeof(hlsl_float2),        // stride
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
        {
            1,                          // location
            1,                          // binding
            VK_FORMAT_R32G32B32_SFLOAT, // format
            0                           // offset
        },
        {
            2,                       // location
            2,                       // binding
            VK_FORMAT_R32G32_SFLOAT, // format
            0                        // offset
        },
    };

    std::vector<VkPipelineShaderStageCreateInfo> blitShaderStages = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, blitShaderVS,
         entryPoint, specialization},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, blitShaderFS,
         entryPoint, specialization}};

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

    VkPipelineColorBlendStateCreateInfo blitBlendStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                              nullptr,
                                                              VK_FLAGS_NONE,
                                                              VK_FALSE,
                                                              VK_LOGIC_OP_COPY,
                                                              1,
                                                              &blitBlendAttachmentState,
                                                              {0.0f, 0.0f, 0.0f, 0.0f}};

    std::array<VkDescriptorSetLayoutBinding, 4> descriptorSetLayoutBinding = {
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, ShadowMapMaxCount,
                                     VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };

    std::array<VkDescriptorBindingFlags, 4> bindingFlags = {VK_FLAGS_NONE, VK_FLAGS_NONE, VK_FLAGS_NONE,
                                                            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

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

    vkDestroyShaderModule(backend.device, blitShaderVS, nullptr);
    vkDestroyShaderModule(backend.device, blitShaderFS, nullptr);

    return MainPipelineInfo{pipeline, pipelineLayout, descriptorSetLayoutCB};
}

namespace
{
    void create_depth_buffer(ReaperRoot& root, VulkanBackend& backend, MainPassResources& resources, glm::uvec2 extent)
    {
        GPUTextureProperties depthProperties = DefaultGPUTextureProperties(extent.x, extent.y, PixelFormat::D16_UNORM);
        depthProperties.usageFlags = GPUTextureUsage::DepthStencilAttachment;

        resources.depthBuffer =
            create_image(root, backend.device, "Main Depth Buffer", depthProperties, backend.vma_instance);
        resources.depthBufferView = create_depth_image_view(root, backend.device, resources.depthBuffer);
    }

    void destroy_depth_buffer(VulkanBackend& backend, MainPassResources& resources)
    {
        vkDestroyImageView(backend.device, resources.depthBufferView, nullptr);
        vmaDestroyImage(backend.vma_instance, resources.depthBuffer.handle, resources.depthBuffer.allocation);

        resources.depthBuffer = {};
        resources.depthBufferView = VK_NULL_HANDLE;
    }

    void create_framebuffers(ReaperRoot& /*root*/, VulkanBackend& backend, VkRenderPass renderPass,
                             VkImageView depthBufferView, std::vector<VkFramebuffer>& framebuffers)
    {
        const size_t imgCount = backend.presentInfo.imageCount;

        framebuffers.resize(imgCount);

        for (size_t i = 0; i < imgCount; ++i)
        {
            std::array<const VkImageView, 2> imageViews = {backend.presentInfo.imageViews[i], depthBufferView};

            VkFramebufferCreateInfo framebuffer_create_info = {
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType                sType
                nullptr,                                   // const void                    *pNext
                0,                                         // VkFramebufferCreateFlags       flags
                renderPass,                                // VkRenderPass                   renderPass
                static_cast<u32>(imageViews.size()),       // uint32_t                       attachmentCount
                imageViews.data(),                         // const VkImageView             *pAttachments
                backend.presentInfo.surfaceExtent.width,   // uint32_t                       width
                backend.presentInfo.surfaceExtent.height,  // uint32_t                       height
                1                                          // uint32_t                       layers
            };

            Assert(vkCreateFramebuffer(backend.device, &framebuffer_create_info, nullptr, &framebuffers[i])
                   == VK_SUCCESS);
        }
    }

    void destroy_framebuffers(VkDevice device, std::vector<VkFramebuffer>& framebuffers)
    {
        for (auto& framebuffer : framebuffers)
            vkDestroyFramebuffer(device, framebuffer, nullptr);

        framebuffers.clear();
    }
} // namespace

MainPassResources create_main_pass_resources(ReaperRoot& root, VulkanBackend& backend, glm::uvec2 extent)
{
    MainPassResources resources = {};

    resources.drawPassConstantBuffer = create_buffer(
        root, backend.device, "Draw Pass Constant buffer",
        DefaultGPUBufferProperties(1, sizeof(DrawPassParams), GPUBufferUsage::UniformBuffer), backend.vma_instance);

    resources.drawInstanceConstantBuffer = create_buffer(
        root, backend.device, "Draw Instance Constant buffer",
        DefaultGPUBufferProperties(DrawInstanceCountMax, sizeof(DrawInstanceParams), GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    create_depth_buffer(root, backend, resources, extent);

    resources.mainRenderPass = create_main_pass(root, backend, resources.depthBuffer.properties);
    resources.mainPipe = create_main_pipeline(root, backend, resources.mainRenderPass);

    create_framebuffers(root, backend, resources.mainRenderPass, resources.depthBufferView, resources.framebuffers);

    VkSamplerCreateInfo shadowMapSamplerCreateInfo = {};
    shadowMapSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    shadowMapSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    shadowMapSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    shadowMapSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowMapSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowMapSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowMapSamplerCreateInfo.anisotropyEnable = VK_FALSE;
    shadowMapSamplerCreateInfo.maxAnisotropy = 16;
    shadowMapSamplerCreateInfo.borderColor =
        UseReverseZ ? VK_BORDER_COLOR_INT_OPAQUE_BLACK : VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    shadowMapSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    shadowMapSamplerCreateInfo.compareEnable = VK_TRUE;
    shadowMapSamplerCreateInfo.compareOp = UseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
    shadowMapSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadowMapSamplerCreateInfo.mipLodBias = 0.f;
    shadowMapSamplerCreateInfo.minLod = 0.f;
    shadowMapSamplerCreateInfo.maxLod = FLT_MAX;

    Assert(vkCreateSampler(backend.device, &shadowMapSamplerCreateInfo, nullptr, &resources.shadowMapSampler)
           == VK_SUCCESS);
    log_debug(root, "vulkan: created sampler with handle: {}", static_cast<void*>(resources.shadowMapSampler));

    return resources;
}

void destroy_main_pass_resources(VulkanBackend& backend, MainPassResources& resources)
{
    vkDestroySampler(backend.device, resources.shadowMapSampler, nullptr);

    destroy_framebuffers(backend.device, resources.framebuffers);

    vkDestroyPipeline(backend.device, resources.mainPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.mainPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.mainPipe.descSetLayout, nullptr);

    vkDestroyRenderPass(backend.device, resources.mainRenderPass, nullptr);

    destroy_depth_buffer(backend, resources);

    vmaDestroyBuffer(backend.vma_instance, resources.drawPassConstantBuffer.buffer,
                     resources.drawPassConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.drawInstanceConstantBuffer.buffer,
                     resources.drawInstanceConstantBuffer.allocation);
}

void resize_main_pass_depth_buffer(ReaperRoot& root, VulkanBackend& backend, MainPassResources& resources,
                                   glm::uvec2 extent)
{
    destroy_framebuffers(backend.device, resources.framebuffers);
    destroy_depth_buffer(backend, resources);

    create_depth_buffer(root, backend, resources, extent);
    create_framebuffers(root, backend, resources.mainRenderPass, resources.depthBufferView, resources.framebuffers);
}

VkDescriptorSet create_main_pass_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
                                                const MainPassResources&        resources,
                                                const nonstd::span<VkImageView> shadow_map_views)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                          backend.frame_descriptor_pool, 1,
                                                          &resources.mainPipe.descSetLayout};

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptor_set) == VK_SUCCESS);
    log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptor_set));

    const VkDescriptorBufferInfo drawDescPassParams = default_descriptor_buffer_info(resources.drawPassConstantBuffer);
    const VkDescriptorBufferInfo drawDescInstanceParams =
        default_descriptor_buffer_info(resources.drawInstanceConstantBuffer);
    const VkDescriptorImageInfo drawDescShadowMapSampler = {resources.shadowMapSampler, VK_NULL_HANDLE,
                                                            VK_IMAGE_LAYOUT_UNDEFINED};

    std::vector<VkDescriptorImageInfo> drawDescShadowMapTexture;

    for (auto shadow_map_texture_view : shadow_map_views)
    {
        drawDescShadowMapTexture.push_back(
            {VK_NULL_HANDLE, shadow_map_texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    }

    const VkWriteDescriptorSet shadowMapBindlessImageWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        descriptor_set,
        3,
        0,
        static_cast<u32>(drawDescShadowMapTexture.size()),
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        drawDescShadowMapTexture.data(),
        nullptr,
        nullptr,
    };

    std::array<VkWriteDescriptorSet, 4> drawPassDescriptorSetWrites = {
        create_buffer_descriptor_write(descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &drawDescPassParams),
        create_buffer_descriptor_write(descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &drawDescInstanceParams),
        create_image_descriptor_write(descriptor_set, 2, VK_DESCRIPTOR_TYPE_SAMPLER, &drawDescShadowMapSampler),
        shadowMapBindlessImageWrite};

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(drawPassDescriptorSetWrites.size()),
                           drawPassDescriptorSetWrites.data(), 0, nullptr);

    return descriptor_set;
}
} // namespace Reaper
