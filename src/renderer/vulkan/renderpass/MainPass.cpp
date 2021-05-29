////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MainPass.h"

#include "Culling.h"
#include "ShadowConstants.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/Image.h"
#include "renderer/vulkan/MaterialResources.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/RenderPassHelpers.h"
#include "renderer/vulkan/Shader.h"
#include "renderer/vulkan/SwapchainRendererBase.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/Profile.h"

#include "renderer/shader/share/draw.hlsl"

namespace Reaper
{
constexpr u32  DiffuseMapMaxCount = 8;
constexpr u32  DrawInstanceCountMax = 512;
constexpr bool UseReverseZ = true;

namespace
{
    VkDescriptorSetLayout create_descriptor_set_layout_0(VulkanBackend& backend)
    {
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
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr,
            static_cast<u32>(bindingFlags.size()), bindingFlags.data()};

        const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &descriptorSetLayoutBindingFlags, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &layout) == VK_SUCCESS);

        return layout;
    }

    void update_descriptor_set_0(VulkanBackend& backend, const MainPassResources& resources,
                                 const nonstd::span<VkImageView> shadow_map_views)
    {
        const VkDescriptorBufferInfo passParams = default_descriptor_buffer_info(resources.drawPassConstantBuffer);
        const VkDescriptorBufferInfo instanceParams =
            default_descriptor_buffer_info(resources.drawInstanceConstantBuffer);
        const VkDescriptorImageInfo shadowMapSampler = {resources.shadowMapSampler, VK_NULL_HANDLE,
                                                        VK_IMAGE_LAYOUT_UNDEFINED};

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
            3,
            0,
            static_cast<u32>(drawDescShadowMaps.size()),
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            drawDescShadowMaps.data(),
            nullptr,
            nullptr,
        };

        std::vector<VkWriteDescriptorSet> writes = {
            create_buffer_descriptor_write(resources.descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &passParams),
            create_buffer_descriptor_write(resources.descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &instanceParams),
            create_image_descriptor_write(resources.descriptor_set, 2, VK_DESCRIPTOR_TYPE_SAMPLER, &shadowMapSampler),
            shadowMapBindlessImageWrite};

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    VkDescriptorSetLayout create_descriptor_set_layout_1(VulkanBackend& backend)
    {
        std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, DiffuseMapMaxCount,
                                         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        std::array<VkDescriptorBindingFlags, 2> bindingFlags = {VK_FLAGS_NONE,
                                                                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

        const VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr,
            static_cast<u32>(bindingFlags.size()), bindingFlags.data()};

        const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &descriptorSetLayoutBindingFlags, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &layout) == VK_SUCCESS);

        return layout;
    }

    void update_descriptor_set_1(VulkanBackend& backend, const MainPassResources& resources,
                                 const MaterialResources& material_resources, const nonstd::span<TextureHandle> handles)
    {
        std::vector<VkDescriptorImageInfo> descriptor_maps;

        for (auto handle : handles)
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

        const VkDescriptorImageInfo diffuseMapSampler = {resources.diffuseMapSampler, VK_NULL_HANDLE,
                                                         VK_IMAGE_LAYOUT_UNDEFINED};

        std::vector<VkWriteDescriptorSet> writes = {create_image_descriptor_write(resources.material_descriptor_set, 0,
                                                                                  VK_DESCRIPTOR_TYPE_SAMPLER,
                                                                                  &diffuseMapSampler),
                                                    diffuseMapBindlessImageWrite};

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }
} // namespace

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

    VkDescriptorSetLayout descriptorSetLayoutCB = create_descriptor_set_layout_0(backend);
    VkDescriptorSetLayout materialDescSetLayout = create_descriptor_set_layout_1(backend);

    std::array<VkDescriptorSetLayout, 2> mainPassDescriptorSetLayouts = {
        descriptorSetLayoutCB,
        materialDescSetLayout,
    };

    VkPipelineLayoutCreateInfo blitPipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                         nullptr,
                                                         VK_FLAGS_NONE,
                                                         static_cast<u32>(mainPassDescriptorSetLayouts.size()),
                                                         mainPassDescriptorSetLayouts.data(),
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
        static_cast<u32>(dynamicStates.size()),
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

    return MainPipelineInfo{pipeline, pipelineLayout, descriptorSetLayoutCB, materialDescSetLayout};
}

namespace
{
    VkRenderPass create_main_pass(ReaperRoot& /*root*/, VulkanBackend& backend, const MainPassResources& resources)
    {
        const GPUTextureProperties& depthProperties = resources.depthBuffer.properties;
        const GPUTextureProperties& hdrProperties = resources.hdrBuffer.properties;

        const VkAttachmentDescription2 color_attachment = {VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                                                           nullptr,
                                                           VK_FLAGS_NONE,
                                                           PixelFormatToVulkan(hdrProperties.format),
                                                           SampleCountToVulkan(hdrProperties.sampleCount),
                                                           VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                           VK_ATTACHMENT_STORE_OP_STORE,
                                                           VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                           VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                           VK_IMAGE_LAYOUT_UNDEFINED,
                                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        const VkAttachmentDescription2 depth_attachment = {VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                                                           nullptr,
                                                           VK_FLAGS_NONE,
                                                           PixelFormatToVulkan(depthProperties.format),
                                                           SampleCountToVulkan(depthProperties.sampleCount),
                                                           VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                           VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                           VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                           VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                           VK_IMAGE_LAYOUT_UNDEFINED,
                                                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL};

        std::array<VkAttachmentDescription2, 2> attachmentDescriptions = {
            color_attachment,
            depth_attachment,
        };

        VkAttachmentReference2 colorReference = {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, 0,
                                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT};
        VkAttachmentReference2 depthReference = {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, 1,
                                                 VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT};

        VkSubpassDescription2 subpassDescription = {};
        subpassDescription.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
        subpassDescription.pNext = nullptr;
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &colorReference;
        subpassDescription.pDepthStencilAttachment = &depthReference;

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

        const VkSubpassDependency2 depth_dep = {
            VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
            nullptr,
            0,
            VK_SUBPASS_EXTERNAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
        };

        // Use subpass dependencies for layout transitions
        std::array<VkSubpassDependency2, 2> dependencies = {color_dep, depth_dep};

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

    VkFramebuffer create_main_framebuffer(VulkanBackend& backend, MainPassResources& resources)
    {
        std::array<VkFormat, 2>                         formats; // Kept alive until vkCreateFramebuffer() call
        std::array<VkFramebufferAttachmentImageInfo, 2> framebufferAttachmentInfo = {
            get_attachment_info_from_image_properties(resources.hdrBuffer.properties, &formats[0]),
            get_attachment_info_from_image_properties(resources.depthBuffer.properties, &formats[1]),
        };

        VkFramebufferAttachmentsCreateInfo framebufferAttachmentsInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO, nullptr,
            static_cast<u32>(framebufferAttachmentInfo.size()), framebufferAttachmentInfo.data()};

        VkFramebufferCreateInfo framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,          // VkStructureType                sType
            &framebufferAttachmentsInfo,                        // const void                    *pNext
            VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,                // VkFramebufferCreateFlags       flags
            resources.mainRenderPass,                           // VkRenderPass                   renderPass
            static_cast<u32>(framebufferAttachmentInfo.size()), // uint32_t                       attachmentCount
            nullptr,                                            // const VkImageView             *pAttachments
            framebufferAttachmentInfo[0].width,                 // uint32_t                       width
            framebufferAttachmentInfo[0].height,                // uint32_t                       height
            1                                                   // uint32_t                       layers
        };

        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        Assert(vkCreateFramebuffer(backend.device, &framebuffer_create_info, nullptr, &framebuffer) == VK_SUCCESS);

        return framebuffer;
    }

    void create_main_pass_resizable_resources(ReaperRoot& root, VulkanBackend& backend, MainPassResources& resources)
    {
        resources.hdrBuffer =
            create_image(root, backend.device, "Main HDR Target", resources.hdrBuffer.properties, backend.vma_instance);
        resources.hdrBufferView = create_default_image_view(root, backend.device, resources.hdrBuffer);

        resources.depthBuffer = create_image(root, backend.device, "Main Depth Target",
                                             resources.depthBuffer.properties, backend.vma_instance);
        resources.depthBufferView = create_depth_image_view(root, backend.device, resources.depthBuffer);

        resources.main_framebuffer = create_main_framebuffer(backend, resources);
    }

    void destroy_main_pass_resizable_resources(VulkanBackend& backend, MainPassResources& resources)
    {
        vkDestroyFramebuffer(backend.device, resources.main_framebuffer, nullptr);

        vkDestroyImageView(backend.device, resources.hdrBufferView, nullptr);
        vmaDestroyImage(backend.vma_instance, resources.hdrBuffer.handle, resources.hdrBuffer.allocation);
        vkDestroyImageView(backend.device, resources.depthBufferView, nullptr);
        vmaDestroyImage(backend.vma_instance, resources.depthBuffer.handle, resources.depthBuffer.allocation);

        resources.hdrBuffer = {};
        resources.hdrBufferView = VK_NULL_HANDLE;
        resources.depthBuffer = {};
        resources.depthBufferView = VK_NULL_HANDLE;
    }

    void update_resource_properties(MainPassResources& resources, glm::uvec2 extent)
    {
        GPUTextureProperties hdrProperties =
            DefaultGPUTextureProperties(extent.x, extent.y, PixelFormat::B10G11R11_UFLOAT_PACK32);
        hdrProperties.usageFlags = GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled;

        GPUTextureProperties depthProperties = DefaultGPUTextureProperties(extent.x, extent.y, PixelFormat::D16_UNORM);
        depthProperties.usageFlags = GPUTextureUsage::DepthStencilAttachment;

        resources.hdrBuffer.properties = hdrProperties;
        resources.depthBuffer.properties = depthProperties;
    }

    VkDescriptorSet create_main_pass_descriptor_set(ReaperRoot& root, VulkanBackend& backend,
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

    VkSampler create_shadow_map_sampler(VulkanBackend& backend)
    {
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

        VkSampler sampler;
        Assert(vkCreateSampler(backend.device, &shadowMapSamplerCreateInfo, nullptr, &sampler) == VK_SUCCESS);

        return sampler;
    }

    VkSampler create_diffuse_map_sampler(VulkanBackend& backend)
    {
        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.anisotropyEnable = VK_TRUE;
        samplerCreateInfo.anisotropyEnable = VK_FALSE; // FIXME enable at higher level
        samplerCreateInfo.maxAnisotropy = 8;
        samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.mipLodBias = 0.f;
        samplerCreateInfo.minLod = 0.f;
        samplerCreateInfo.maxLod = FLT_MAX;

        VkSampler sampler = VK_NULL_HANDLE;
        Assert(vkCreateSampler(backend.device, &samplerCreateInfo, nullptr, &sampler) == VK_SUCCESS);

        return sampler;
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

    update_resource_properties(resources, extent);

    resources.mainRenderPass = create_main_pass(root, backend, resources);

    create_main_pass_resizable_resources(root, backend, resources);

    resources.mainPipe = create_main_pipeline(root, backend, resources.mainRenderPass);

    resources.descriptor_set = create_main_pass_descriptor_set(root, backend, resources.mainPipe.descSetLayout);
    resources.material_descriptor_set =
        create_material_descriptor_set(root, backend, resources.mainPipe.descSetLayout2);

    resources.shadowMapSampler = create_shadow_map_sampler(backend);
    resources.diffuseMapSampler = create_diffuse_map_sampler(backend);

    return resources;
}

void destroy_main_pass_resources(VulkanBackend& backend, MainPassResources& resources)
{
    vkDestroySampler(backend.device, resources.diffuseMapSampler, nullptr);
    vkDestroySampler(backend.device, resources.shadowMapSampler, nullptr);

    vkDestroyPipeline(backend.device, resources.mainPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.mainPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.mainPipe.descSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.mainPipe.descSetLayout2, nullptr);

    vkDestroyRenderPass(backend.device, resources.mainRenderPass, nullptr);

    destroy_main_pass_resizable_resources(backend, resources);

    vmaDestroyBuffer(backend.vma_instance, resources.drawPassConstantBuffer.buffer,
                     resources.drawPassConstantBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.drawInstanceConstantBuffer.buffer,
                     resources.drawInstanceConstantBuffer.allocation);
}

void resize_main_pass_resources(ReaperRoot& root, VulkanBackend& backend, MainPassResources& resources,
                                glm::uvec2 extent)
{
    destroy_main_pass_resizable_resources(backend, resources);

    update_resource_properties(resources, extent);

    create_main_pass_resizable_resources(root, backend, resources);
}

void update_main_pass_descriptor_sets(VulkanBackend& backend, const MainPassResources& resources,
                                      const MaterialResources&          material_resources,
                                      const nonstd::span<VkImageView>   shadow_map_views,
                                      const nonstd::span<TextureHandle> handles)
{
    update_descriptor_set_0(backend, resources, shadow_map_views);
    update_descriptor_set_1(backend, resources, material_resources, handles);
}

void upload_main_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                      MainPassResources& pass_resources)
{
    upload_buffer_data(backend.device, backend.vma_instance, pass_resources.drawPassConstantBuffer,
                       &prepared.draw_pass_params, sizeof(DrawPassParams));
    upload_buffer_data(backend.device, backend.vma_instance, pass_resources.drawInstanceConstantBuffer,
                       prepared.draw_instance_params.data(),
                       prepared.draw_instance_params.size() * sizeof(DrawInstanceParams));
}

void record_main_pass_command_buffer(const CullOptions& cull_options, VkCommandBuffer cmdBuffer,
                                     const PreparedData& prepared, const MainPassResources& pass_resources,
                                     const CullResources& cull_resources, const MeshCache& mesh_cache,
                                     VkExtent2D backbufferExtent)
{
    REAPER_PROFILE_SCOPE_GPU("Main Pass", MP_DARKGOLDENROD);

    const float                 depthClearValue = UseReverseZ ? 0.f : 1.f;
    const glm::fvec4            clearColor = {0.1f, 0.1f, 0.1f, 0.0f};
    std::array<VkClearValue, 2> clearValues = {VkClearColor(clearColor), VkClearDepthStencil(depthClearValue, 0)};
    const VkRect2D              blitPassRect = default_vk_rect(backbufferExtent);

    std::array<VkImageView, 2> main_pass_framebuffer_views = {pass_resources.hdrBufferView,
                                                              pass_resources.depthBufferView};

    VkRenderPassAttachmentBeginInfo main_pass_attachments = {
        VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO, nullptr,
        static_cast<u32>(main_pass_framebuffer_views.size()), main_pass_framebuffer_views.data()};

    VkRenderPassBeginInfo blitRenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                     &main_pass_attachments,
                                                     pass_resources.mainRenderPass,
                                                     pass_resources.main_framebuffer,
                                                     blitPassRect,
                                                     static_cast<u32>(clearValues.size()),
                                                     clearValues.data()};

    vkCmdBeginRenderPass(cmdBuffer, &blitRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.mainPipe.pipeline);

    const VkViewport blitViewport = default_vk_viewport(blitPassRect);

    vkCmdSetViewport(cmdBuffer, 0, 1, &blitViewport);
    vkCmdSetScissor(cmdBuffer, 0, 1, &blitPassRect);

    std::vector<VkBuffer> vertexBuffers = {
        mesh_cache.vertexBufferPosition.buffer,
        mesh_cache.vertexBufferNormal.buffer,
        mesh_cache.vertexBufferUV.buffer,
    };
    std::vector<VkDeviceSize> vertexBufferOffsets = {
        0,
        0,
        0,
    };
    Assert(vertexBuffers.size() == vertexBufferOffsets.size());
    vkCmdBindIndexBuffer(cmdBuffer, cull_resources.dynamicIndexBuffer.buffer, 0, get_vk_culling_index_type());
    vkCmdBindVertexBuffers(cmdBuffer, 0, static_cast<u32>(vertexBuffers.size()), vertexBuffers.data(),
                           vertexBufferOffsets.data());

    std::array<VkDescriptorSet, 2> main_pass_descriptors = {
        pass_resources.descriptor_set,
        pass_resources.material_descriptor_set,
    };

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_resources.mainPipe.pipelineLayout, 0,
                            static_cast<u32>(main_pass_descriptors.size()), main_pass_descriptors.data(), 0, nullptr);

    const u32 pass_index = prepared.draw_culling_pass_index;

    const u32 draw_buffer_offset = pass_index * MaxIndirectDrawCount * sizeof(VkDrawIndexedIndirectCommand);
    const u32 draw_buffer_max_count = MaxIndirectDrawCount;

    if (cull_options.use_compacted_draw)
    {
        const u32 draw_buffer_count_offset = pass_index * 1 * sizeof(u32);
        vkCmdDrawIndexedIndirectCount(cmdBuffer, cull_resources.compactIndirectDrawBuffer.buffer, draw_buffer_offset,
                                      cull_resources.compactIndirectDrawCountBuffer.buffer, draw_buffer_count_offset,
                                      draw_buffer_max_count,
                                      cull_resources.compactIndirectDrawBuffer.descriptor.elementSize);
    }
    else
    {
        const u32 draw_buffer_count_offset = pass_index * IndirectDrawCountCount * sizeof(u32);
        vkCmdDrawIndexedIndirectCount(cmdBuffer, cull_resources.indirectDrawBuffer.buffer, draw_buffer_offset,
                                      cull_resources.indirectDrawCountBuffer.buffer, draw_buffer_count_offset,
                                      draw_buffer_max_count, cull_resources.indirectDrawBuffer.descriptor.elementSize);
    }

    vkCmdEndRenderPass(cmdBuffer);
}
} // namespace Reaper
