////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ShadowMap.h"

#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/Shader.h"
#include "renderer/vulkan/SwapchainRendererBase.h"

#include "renderer/vulkan/api/Vulkan.h"

#include "renderer/texture/GPUTextureProperties.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include <array>

#include "renderer/shader/share/types.hlsl"

namespace Reaper
{
constexpr bool UseReverseZ = true;

VkRenderPass create_shadow_raster_pass(ReaperRoot& /*root*/, VulkanBackend& backend,
                                       const GPUTextureProperties& shadowMapProperties)
{
    // Create a separate render pass for the offscreen rendering as it may differ from the one used for scene
    // rendering
    std::array<VkAttachmentDescription, 1> attachmentDescriptions = {};

    // Depth attachment
    attachmentDescriptions[0].format = PixelFormatToVulkan(shadowMapProperties.format);
    attachmentDescriptions[0].samples = SampleCountToVulkan(shadowMapProperties.sampleCount);
    attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 0;
    subpassDescription.pColorAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = &depthReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 1> dependencies;

    dependencies[0].srcSubpass = 0;
    dependencies[0].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
    renderPassInfo.pAttachments = attachmentDescriptions.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VkRenderPass renderPass = VK_NULL_HANDLE;
    Assert(vkCreateRenderPass(backend.device, &renderPassInfo, nullptr, &renderPass) == VK_SUCCESS);

    return renderPass;
}

ShadowMapPipelineInfo create_shadow_map_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass,
                                                 u32 shadowMapRes)
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

    const VkExtent2D shadowMapExtent = {shadowMapRes, shadowMapRes};

    VkViewport blitViewport = {
        0.0f, 0.0f, static_cast<float>(shadowMapExtent.width), static_cast<float>(shadowMapExtent.height), 0.0f, 1.0f};

    VkRect2D blitScissors = {{0, 0}, shadowMapExtent};

    VkPipelineViewportStateCreateInfo blitViewportStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                               nullptr,
                                                               VK_FLAGS_NONE,
                                                               1,
                                                               &blitViewport,
                                                               1,
                                                               &blitScissors};

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

    log_debug(root, "vulkan: created descriptor set layout with handle: {}", static_cast<void*>(descriptorSetLayoutCB));

    VkPipelineLayoutCreateInfo blitPipelineLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, VK_FLAGS_NONE, 1, &descriptorSetLayoutCB, 0, nullptr};

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    Assert(vkCreatePipelineLayout(backend.device, &blitPipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);
    log_debug(root, "vulkan: created blit pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

    VkPipelineCache cache = VK_NULL_HANDLE;

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
                                                           nullptr,
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

ShadowMapResources create_shadow_map_resources(ReaperRoot& root, VulkanBackend& backend)
{
    VkSampler           shadowMapSampler = VK_NULL_HANDLE;
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

    Assert(vkCreateSampler(backend.device, &shadowMapSamplerCreateInfo, nullptr, &shadowMapSampler) == VK_SUCCESS);
    log_debug(root, "vulkan: created sampler with handle: {}", static_cast<void*>(shadowMapSampler));

    return ShadowMapResources{shadowMapSampler};
}

void destroy_shadow_map_resources(VulkanBackend& backend, ShadowMapResources& resources)
{
    vkDestroySampler(backend.device, resources.shadowMapSampler, nullptr);
}
} // namespace Reaper
