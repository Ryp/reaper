////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GuiPass.h"

#include "Frame.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/Shader.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/Profile.h"

namespace Reaper
{
GuiPipelineInfo create_gui_pipeline(ReaperRoot& root, VulkanBackend& backend, VkRenderPass renderPass)
{
    VkShaderModule shaderFS = VK_NULL_HANDLE;
    VkShaderModule shaderVS = VK_NULL_HANDLE;
    const char*    fileNameVS = "./build/shader/fullscreen_triangle.vert.spv"; // FIXME
    const char*    fileNameFS = "./build/shader/gui_write.frag.spv";           // FIXME
    const char*    entryPoint = "main";

    vulkan_create_shader_module(shaderFS, backend.device, fileNameFS);
    vulkan_create_shader_module(shaderVS, backend.device, fileNameVS);

    std::vector<VkPipelineShaderStageCreateInfo> blitShaderStages = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, shaderVS,
         entryPoint, nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, shaderFS,
         entryPoint, nullptr}};

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

    Assert(backend.physicalDeviceInfo.graphicsQueueFamilyIndex == backend.physicalDeviceInfo.presentQueueFamilyIndex);

    vkDestroyShaderModule(backend.device, shaderVS, nullptr);
    vkDestroyShaderModule(backend.device, shaderFS, nullptr);

    return GuiPipelineInfo{pipeline, pipelineLayout, descriptorSetLayoutCB};
}

namespace
{
    VkRenderPass create_gui_pass(ReaperRoot& /*root*/, VulkanBackend& backend, VkFormat gui_format)
    {
        const VkAttachmentDescription2 color_attachment = {VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                                                           nullptr,
                                                           VK_FLAGS_NONE,
                                                           gui_format,
                                                           VK_SAMPLE_COUNT_1_BIT,
                                                           VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                           VK_ATTACHMENT_STORE_OP_STORE,
                                                           VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                           VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                           VK_IMAGE_LAYOUT_UNDEFINED,
                                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

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

    VkFramebuffer create_gui_framebuffer(VulkanBackend& backend, GuiPassResources& resources)
    {
        std::array<VkFormat, 1>                         formats; // Kept alive until vkCreateFramebuffer() call
        std::array<VkFramebufferAttachmentImageInfo, 1> framebufferAttachmentInfo = {
            get_attachment_info_from_image_properties(resources.guiTexture.properties, &formats[0]),
        };

        VkFramebufferAttachmentsCreateInfo framebufferAttachmentsInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO, nullptr, framebufferAttachmentInfo.size(),
            framebufferAttachmentInfo.data()};

        VkFramebufferCreateInfo framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType                sType
            &framebufferAttachmentsInfo,               // const void                    *pNext
            VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,       // VkFramebufferCreateFlags       flags
            resources.guiRenderPass,                   // VkRenderPass                   renderPass
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

    void create_gui_pass_resizable_resources(ReaperRoot& root, VulkanBackend& backend, GuiPassResources& resources,
                                             glm::uvec2 /*extent*/)
    {
        resources.guiTexture =
            create_image(root, backend.device, "GUI", resources.guiTexture.properties, backend.vma_instance);
        resources.guiTextureView = create_default_image_view(root, backend.device, resources.guiTexture);

        resources.gui_framebuffer = create_gui_framebuffer(backend, resources);
    }

    void destroy_gui_pass_resizable_resources(VulkanBackend& backend, const GuiPassResources& resources)
    {
        vkDestroyFramebuffer(backend.device, resources.gui_framebuffer, nullptr);
    }

    void update_resource_properties(GuiPassResources& resources, glm::uvec2 extent)
    {
        GPUTextureProperties properties = DefaultGPUTextureProperties(extent.x, extent.y, PixelFormat::R8G8B8A8_SRGB);
        properties.usageFlags = GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled;

        resources.guiTexture.properties = properties;
    }

    VkDescriptorSet create_gui_pass_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
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

GuiPassResources create_gui_pass_resources(ReaperRoot& root, VulkanBackend& backend, glm::uvec2 extent)
{
    GuiPassResources resources = {};

    resources.guiRenderPass = create_gui_pass(root, backend, PixelFormatToVulkan(PixelFormat::R8G8B8A8_SRGB)); // FIXME

    update_resource_properties(resources, extent);

    create_gui_pass_resizable_resources(root, backend, resources, extent);

    resources.guiPipe = create_gui_pipeline(root, backend, resources.guiRenderPass);

    resources.descriptor_set = create_gui_pass_descriptor_set(root, backend, resources.guiPipe.descSetLayout);

    return resources;
}

void destroy_gui_pass_resources(VulkanBackend& backend, const GuiPassResources& resources)
{
    vkDestroyPipeline(backend.device, resources.guiPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.guiPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.guiPipe.descSetLayout, nullptr);

    vkDestroyRenderPass(backend.device, resources.guiRenderPass, nullptr);

    destroy_gui_pass_resizable_resources(backend, resources);
}

void resize_gui_pass_resources(ReaperRoot& root, VulkanBackend& backend, GuiPassResources& resources, glm::uvec2 extent)
{
    destroy_gui_pass_resizable_resources(backend, resources);

    update_resource_properties(resources, extent);

    create_gui_pass_resizable_resources(root, backend, resources, extent);
}

void record_gui_command_buffer(CommandBuffer& cmdBuffer, const GuiPassResources& pass_resources)
{
    REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Gui Pass", MP_DARKGOLDENROD);

    const GPUTextureProperties& p = pass_resources.guiTexture.properties;
    const VkRect2D              blitPassRect = default_vk_rect({p.width, p.height});

    std::array<VkImageView, 1> main_pass_framebuffer_views = {pass_resources.guiTextureView};

    VkRenderPassAttachmentBeginInfo main_pass_attachments = {VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,
                                                             nullptr, main_pass_framebuffer_views.size(),
                                                             main_pass_framebuffer_views.data()};

    VkRenderPassBeginInfo blitRenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                     &main_pass_attachments,
                                                     pass_resources.guiRenderPass,
                                                     pass_resources.gui_framebuffer,
                                                     blitPassRect,
                                                     0,
                                                     nullptr};

    vkCmdBeginRenderPass(cmdBuffer.handle, &blitRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.guiPipe.pipeline);

    const VkViewport blitViewport = default_vk_viewport(blitPassRect);

    vkCmdSetViewport(cmdBuffer.handle, 0, 1, &blitViewport);
    vkCmdSetScissor(cmdBuffer.handle, 0, 1, &blitPassRect);

    vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.guiPipe.pipelineLayout, 0,
                            1, &pass_resources.descriptor_set, 0, nullptr);

    vkCmdEndRenderPass(cmdBuffer.handle);
}
} // namespace Reaper
