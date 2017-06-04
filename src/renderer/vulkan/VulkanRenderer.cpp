////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "VulkanRenderer.h"

#include "common/Log.h"

#include <cmath>
#include <cstring>

#include "Shader.h"
#include "Memory.h"

#include "renderer/mesh/ModelLoader.h"
#include "renderer/texture/TextureLoader.h"

#include "allocator/GPUStackAllocator.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace vk;

namespace
{
    struct VSPushConstants {
        float scale;
    };

    constexpr std::size_t TextureCacheSize = 10_MB;
    static const std::string TextureFile("res/texture/uvmap.dds");
    static const std::string MeshFile("res/model/sphere.obj");
    static const std::string ShaderFragFile("./build/spv/mesh_shade.frag.spv");
    static const std::string ShaderVertFile("./build/spv/mesh_shade.vert.spv");
}

struct VSUBO {
    glm::mat4 transform;
    float time;
    float scaleFactor;
};

OldRenderer::OldRenderer()
:   _texCache(TextureCacheSize)
{}

void OldRenderer::startup(IWindow* window)
{
    parent_type::startup(window);

    ModelLoader meshLoader;
    meshLoader.load(MeshFile, _meshCache);

    TextureLoader texLoader;
    texLoader.load(TextureFile, _texCache);

    _pipeline = VK_NULL_HANDLE;
    _pipelineLayout = VK_NULL_HANDLE;

    _deviceMemory = VK_NULL_HANDLE;

    _texMemory = VK_NULL_HANDLE;
    _texImage = VK_NULL_HANDLE;
    _texImageView = VK_NULL_HANDLE;
    _texSampler = VK_NULL_HANDLE;

    _vertexBuffer = VK_NULL_HANDLE;
    _indexBuffer = VK_NULL_HANDLE;

    _descriptorPool = VK_NULL_HANDLE;
    _descriptorSet = VK_NULL_HANDLE;
    _descriptorSetLayout = VK_NULL_HANDLE;

    _uniforms = new VSUBO;
    _uniforms->time = 0.f;
    _uniforms->scaleFactor = 0.f;

    createDescriptorPool();
    createPipeline();
    createMeshBuffers();
    createTextures();
    createDescriptorSet();
    createCommandBuffers();
}

void OldRenderer::shutdown()
{
    Assert(vkDeviceWaitIdle(_device) == VK_SUCCESS);

    vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr);
    vkDestroyBuffer(_device, _uniformData.buffer, nullptr);
    vkFreeMemory(_device, _uniformData.memory, nullptr);

    vkDestroyImageView(_device, _texImageView, nullptr);
    vkDestroyImage(_device, _texImage, nullptr);
    vkDestroySampler(_device, _texSampler, nullptr);
    vkFreeMemory(_device, _texMemory, nullptr);

    vkDestroyBuffer(_device, _vertexBuffer, nullptr);
    vkDestroyBuffer(_device, _indexBuffer, nullptr);
    vkFreeMemory(_device, _deviceMemory, nullptr);

    vkDestroyPipeline(_device, _pipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);

    delete _uniforms;

    parent_type::shutdown();
}

void OldRenderer::render()
{
    uint32_t image_index;

    VkResult result = vkAcquireNextImageKHR(_device, _swapChain, UINT64_MAX, _imageAvailableSemaphore, VK_NULL_HANDLE, &image_index);
    switch(result)
    {
        case VK_SUCCESS:
        case VK_NOT_READY:
        case VK_TIMEOUT:
        case VK_SUBOPTIMAL_KHR:
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
            Assert(false, "Resize not implemented");
            break;
        default:
            Assert(false, "Problem occurred during swap chain image acquisition");
            break;
    }

    updateUniforms();

    VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit_info = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,      // VkStructureType              sType
        nullptr,                            // const void                  *pNext
        1,                                  // uint32_t                     waitSemaphoreCount
        &_imageAvailableSemaphore,          // const VkSemaphore           *pWaitSemaphores
        &wait_dst_stage_mask,               // const VkPipelineStageFlags  *pWaitDstStageMask;
        1,                                  // uint32_t                     commandBufferCount
        &_gfxCmdBuffers[image_index],       // const VkCommandBuffer       *pCommandBuffers
        1,                                  // uint32_t                     signalSemaphoreCount
        &_renderingFinishedSemaphore        // const VkSemaphore           *pSignalSemaphores
    };

    Assert(vkQueueSubmit(_graphicsQueue, 1, &submit_info, VK_NULL_HANDLE) == VK_SUCCESS);

    VkPresentInfoKHR present_info = {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, // VkStructureType              sType
        nullptr,                            // const void                  *pNext
        1,                                  // uint32_t                     waitSemaphoreCount
        &_renderingFinishedSemaphore,       // const VkSemaphore           *pWaitSemaphores
        1,                                  // uint32_t                     swapchainCount
        &_swapChain,                        // const VkSwapchainKHR        *pSwapchains
        &image_index,                       // const uint32_t              *pImageIndices
        nullptr                             // VkResult                    *pResults
    };
    result = vkQueuePresentKHR(_presentationQueue, &present_info);

    switch( result ) {
        case VK_SUCCESS:
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
            Assert(false, "not supported");
            break;
        default:
            Assert(false, "problem occurred during image presentation");
            break;
    }
}

void OldRenderer::createCommandBuffers()
{
    VkCommandBufferBeginInfo graphics_commandd_buffer_begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,    // VkStructureType                        sType
        nullptr,                                        // const void                            *pNext
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,   // VkCommandBufferUsageFlags              flags
        nullptr                                         // const VkCommandBufferInheritanceInfo  *pInheritanceInfo
    };

    VkImageSubresourceRange image_subresource_range = {
        VK_IMAGE_ASPECT_COLOR_BIT,                      // VkImageAspectFlags             aspectMask
        0,                                              // uint32_t                       baseMipLevel
        1,                                              // uint32_t                       levelCount
        0,                                              // uint32_t                       baseArrayLayer
        1                                               // uint32_t                       layerCount
    };

    VkClearValue clear_value = {
        { 1.0f, 0.8f, 0.4f, 0.0f },                     // VkClearColorValue              color
    };

    for (size_t i = 0; i < _swapChainImageCount; ++i)
    {
        Assert(vkBeginCommandBuffer(_gfxCmdBuffers[i], &graphics_commandd_buffer_begin_info) == VK_SUCCESS);

        if (_presentationQueue != _graphicsQueue)
        {
            VkImageMemoryBarrier barrier_from_present_to_draw = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType                sType
                nullptr,                                    // const void                    *pNext
                VK_ACCESS_MEMORY_READ_BIT,                  // VkAccessFlags                  srcAccessMask
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,       // VkAccessFlags                  dstAccessMask
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,            // VkImageLayout                  oldLayout
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,            // VkImageLayout                  newLayout
                _prsQueueIndex,              // uint32_t                       srcQueueFamilyIndex
                _gfxQueueIndex,             // uint32_t                       dstQueueFamilyIndex
                _swapChainImages[i],                // VkImage                        image
                image_subresource_range                     // VkImageSubresourceRange        subresourceRange
            };
            vkCmdPipelineBarrier(_gfxCmdBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_present_to_draw );
        }

        VkRenderPassBeginInfo render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,     // VkStructureType                sType
            nullptr,                                      // const void                    *pNext
            _mainRenderPass,                            // VkRenderPass                   renderPass
            _framebuffers[i],                       // VkFramebuffer                  framebuffer
            {                                             // VkRect2D                       renderArea
                {                                           // VkOffset2D                     offset
                    0,                                          // int32_t                        x
                    0                                           // int32_t                        y
                },
                {                                           // VkExtent2D                     extent
                    800,                                        // int32_t                        width
                    800,                                        // int32_t                        height
                }
            },
            1,                                            // uint32_t                       clearValueCount
            &clear_value                                  // const VkClearValue            *pClearValues
        };

        vkCmdBeginRenderPass(_gfxCmdBuffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );

        VSPushConstants constants;
        constants.scale = 0.5f;

        vkCmdPushConstants(_gfxCmdBuffers[i], _pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);

        vkCmdBindPipeline(_gfxCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

        vkCmdBindDescriptorSets(_gfxCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &_descriptorSet, 0, nullptr);

        VkDeviceSize offsets [] = { 0 };
        vkCmdBindIndexBuffer(_gfxCmdBuffers[i], _indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers(_gfxCmdBuffers[i], 0, 1, &_vertexBuffer, offsets);

        vkCmdDrawIndexed(_gfxCmdBuffers[i], static_cast<u32>(_meshCache[MeshFile].indexes.size()), 1, 0, 0, 0);

        //vkCmdDraw(_gfxCmdBuffers[i], 3, 1, 0, 0 );

        vkCmdEndRenderPass(_gfxCmdBuffers[i] );

        if (_presentationQueue != _graphicsQueue) {
            VkImageMemoryBarrier barrier_from_draw_to_present = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,       // VkStructureType              sType
                nullptr,                                      // const void                  *pNext
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,         // VkAccessFlags                srcAccessMask
                VK_ACCESS_MEMORY_READ_BIT,                    // VkAccessFlags                dstAccessMask
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,              // VkImageLayout                oldLayout
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,              // VkImageLayout                newLayout
                _gfxQueueIndex,                               // uint32_t                     srcQueueFamilyIndex
                _prsQueueIndex,                               // uint32_t                     dstQueueFamilyIndex
                _swapChainImages[i],                          // VkImage                      image
                image_subresource_range                       // VkImageSubresourceRange      subresourceRange
            };
            vkCmdPipelineBarrier(_gfxCmdBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_draw_to_present );
        }
        Assert(vkEndCommandBuffer(_gfxCmdBuffers[i]) == VK_SUCCESS);
    }
}

void OldRenderer::createDescriptorPool()
{
    // We need to tell the API the number of max. requested descriptors per type
    VkDescriptorPoolSize typeCounts[2];
    // This example only uses one descriptor type (uniform buffer) and only
    // requests one descriptor of this type
    typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    typeCounts[0].descriptorCount = 1;
    // For additional types you need to add new entries in the type count list
    // E.g. for two combined image samplers :
    typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    typeCounts[1].descriptorCount = 1;

    // Create the global descriptor pool
    // All descriptors used in this example are allocated from this pool
    VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, // VkStructureType                sType;
        nullptr, // const void*                    pNext;
        0, // VkDescriptorPoolCreateFlags    flags;
        1, // uint32_t                       maxSets;
        1, // uint32_t                       poolSizeCount;
        typeCounts // const VkDescriptorPoolSize*    pPoolSizes;
    };

    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.pNext = NULL;
    descriptorPoolInfo.poolSizeCount = 2;
    descriptorPoolInfo.pPoolSizes = typeCounts;

    Assert(vkCreateDescriptorPool(_device, &descriptorPoolInfo, nullptr, &_descriptorPool) == VK_SUCCESS);

    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    //layoutBinding.binding = 0;
    layoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding layoutSampler = {};
    layoutSampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutSampler.descriptorCount = 1;
    layoutSampler.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutSampler.binding = 1;
    layoutSampler.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
    setLayoutBindings.push_back(layoutBinding);
    setLayoutBindings.push_back(layoutSampler);

    VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.pNext = nullptr;
    descriptorLayout.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
    descriptorLayout.pBindings = setLayoutBindings.data();

    Assert(vkCreateDescriptorSetLayout(_device, &descriptorLayout, nullptr, &_descriptorSetLayout) == VK_SUCCESS);

    {
        {
            // Prepare and initialize a uniform buffer block containing shader uniforms
            // In Vulkan there are no more single uniforms like in GL
            // All shader uniforms are passed as uniform buffer blocks
            VkMemoryRequirements memReqs;

            // Vertex shader uniform buffer block
            VkBufferCreateInfo bufferInfo = {};
            VkMemoryAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.pNext = nullptr;
            allocInfo.allocationSize = 0;
            allocInfo.memoryTypeIndex = 0;

            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(*_uniforms);
            bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

            // Create a new buffer
            Assert(vkCreateBuffer(_device, &bufferInfo, nullptr, &_uniformData.buffer) == VK_SUCCESS);
            // Get memory requirements including size, alignment and memory type
            vkGetBufferMemoryRequirements(_device, _uniformData.buffer, &memReqs);
            allocInfo.allocationSize = memReqs.size;
            allocInfo.memoryTypeIndex = getMemoryType(_physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

            // Allocate memory for the uniform buffer
            Assert(vkAllocateMemory(_device, &allocInfo, nullptr, &(_uniformData.memory)) == VK_SUCCESS);
            // Bind memory to buffer
        }
        // Bind memory to buffer
        Assert(vkBindBufferMemory(_device, _uniformData.buffer, _uniformData.memory, 0) == VK_SUCCESS);

        // Store information in the uniform's descriptor
        _uniformData.descriptor.buffer = _uniformData.buffer;
        _uniformData.descriptor.offset = 0;
        _uniformData.descriptor.range = sizeof(VSUBO);

        updateUniforms();
    }
}

void OldRenderer::createPipeline()
{
    VkShaderModule vertModule;
    VkShaderModule fragModule;

    vulkan_create_shader_module(vertModule, _device, ShaderVertFile);
    vulkan_create_shader_module(fragModule, _device, ShaderFragFile);

    std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos = {
        // Vertex shader
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,        // VkStructureType                                sType
            nullptr,                                                    // const void                                    *pNext
            0,                                                          // VkPipelineShaderStageCreateFlags               flags
            VK_SHADER_STAGE_VERTEX_BIT,                                 // VkShaderStageFlagBits                          stage
            vertModule,                                                 // VkShaderModule                                 module
            "main",                                                     // const char                                    *pName
            nullptr                                                     // const VkSpecializationInfo                    *pSpecializationInfo
        },
        // Fragment shader
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,        // VkStructureType                                sType
            nullptr,                                                    // const void                                    *pNext
            0,                                                          // VkPipelineShaderStageCreateFlags               flags
            VK_SHADER_STAGE_FRAGMENT_BIT,                               // VkShaderStageFlagBits                          stage
            fragModule,                                                 // VkShaderModule                                 module
            "main",                                                     // const char                                    *pName
            nullptr                                                     // const VkSpecializationInfo                    *pSpecializationInfo
        }
    };

    VkVertexInputBindingDescription vertexInputBindingDescription;
    vertexInputBindingDescription.binding = 0;
    vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexInputBindingDescription.stride = sizeof(float) * 5;

    VkVertexInputAttributeDescription vertexInputAttributeDescription [2] = {};
    vertexInputAttributeDescription[0].binding = vertexInputBindingDescription.binding;
    vertexInputAttributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescription[0].location = 0;
    vertexInputAttributeDescription[0].offset = 0;

    vertexInputAttributeDescription[1].binding = vertexInputBindingDescription.binding;
    vertexInputAttributeDescription[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescription[1].location = 1;
    vertexInputAttributeDescription[1].offset = sizeof(float) * 3;

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,    // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineVertexInputStateCreateFlags          flags;
        1,                                                            // uint32_t                                       vertexBindingDescriptionCount
        &vertexInputBindingDescription,                               // const VkVertexInputBindingDescription         *pVertexBindingDescriptions
        std::extent<decltype(vertexInputAttributeDescription)>::value,// uint32_t                                       vertexAttributeDescriptionCount
        vertexInputAttributeDescription                               // const VkVertexInputAttributeDescription       *pVertexAttributeDescriptions
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,  // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineInputAssemblyStateCreateFlags        flags
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                          // VkPrimitiveTopology                            topology
        VK_FALSE                                                      // VkBool32                                       primitiveRestartEnable
    };

    VkViewport viewport = {
        0.0f,                                                         // float                                          x
        0.0f,                                                         // float                                          y
        800.0f,                                                       // float                                          width
        800.0f,                                                       // float                                          height
        0.0f,                                                         // float                                          minDepth
        1.0f                                                          // float                                          maxDepth
    };

    VkRect2D scissor = {
        {                                                             // VkOffset2D                                     offset
            0,                                                            // int32_t                                        x
            0                                                             // int32_t                                        y
        },
        {                                                             // VkExtent2D                                     extent
            800,                                                          // int32_t                                        width
            800                                                           // int32_t                                        height
        }
    };

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,        // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineViewportStateCreateFlags             flags
        1,                                                            // uint32_t                                       viewportCount
        &viewport,                                                    // const VkViewport                              *pViewports
        1,                                                            // uint32_t                                       scissorCount
        &scissor                                                      // const VkRect2D                                *pScissors
    };

    // FIXME change to VK_FRONT_FACE_COUNTER_CLOCKWISE when we have proper matrices
    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,   // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineRasterizationStateCreateFlags        flags
        VK_FALSE,                                                     // VkBool32                                       depthClampEnable
        VK_FALSE,                                                     // VkBool32                                       rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,                                         // VkPolygonMode                                  polygonMode
        VK_CULL_MODE_NONE,                                            // VkCullModeFlags                                cullMode
        VK_FRONT_FACE_COUNTER_CLOCKWISE,                              // VkFrontFace                                    frontFace
        VK_FALSE,                                                     // VkBool32                                       depthBiasEnable
        0.0f,                                                         // float                                          depthBiasConstantFactor
        0.0f,                                                         // float                                          depthBiasClamp
        0.0f,                                                         // float                                          depthBiasSlopeFactor
        1.0f                                                          // float                                          lineWidth
    };

    VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,     // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineMultisampleStateCreateFlags          flags
        VK_SAMPLE_COUNT_1_BIT,                                        // VkSampleCountFlagBits                          rasterizationSamples
        VK_FALSE,                                                     // VkBool32                                       sampleShadingEnable
        1.0f,                                                         // float                                          minSampleShading
        nullptr,                                                      // const VkSampleMask                            *pSampleMask
        VK_FALSE,                                                     // VkBool32                                       alphaToCoverageEnable
        VK_FALSE                                                      // VkBool32                                       alphaToOneEnable
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
        VK_FALSE,                                                     // VkBool32                                       blendEnable
        VK_BLEND_FACTOR_ONE,                                          // VkBlendFactor                                  srcColorBlendFactor
        VK_BLEND_FACTOR_ZERO,                                         // VkBlendFactor                                  dstColorBlendFactor
        VK_BLEND_OP_ADD,                                              // VkBlendOp                                      colorBlendOp
        VK_BLEND_FACTOR_ONE,                                          // VkBlendFactor                                  srcAlphaBlendFactor
        VK_BLEND_FACTOR_ZERO,                                         // VkBlendFactor                                  dstAlphaBlendFactor
        VK_BLEND_OP_ADD,                                              // VkBlendOp                                      alphaBlendOp
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |         // VkColorComponentFlags                          colorWriteMask
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,     // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineColorBlendStateCreateFlags           flags
        VK_FALSE,                                                     // VkBool32                                       logicOpEnable
        VK_LOGIC_OP_COPY,                                             // VkLogicOp                                      logicOp
        1,                                                            // uint32_t                                       attachmentCount
        &color_blend_attachment_state,                                // const VkPipelineColorBlendAttachmentState     *pAttachments
        { 0.0f, 0.0f, 0.0f, 0.0f }                                    // float                                          blendConstants[4]
    };

    VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_VERTEX_BIT, // VkShaderStageFlags    stageFlags;
        0,                          // uint32_t              offset;
        sizeof(VSPushConstants)     // uint32_t              size;
    };

    VkPipelineLayoutCreateInfo layout_create_info = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,  // VkStructureType                sType
        nullptr,                                        // const void                    *pNext
        0,                                              // VkPipelineLayoutCreateFlags    flags
        1,                                              // uint32_t                       setLayoutCount
        &_descriptorSetLayout,                          // const VkDescriptorSetLayout   *pSetLayouts
        1,                                              // uint32_t                       pushConstantRangeCount
        &pushConstantRange                              // const VkPushConstantRange     *pPushConstantRanges
    };

    Assert(vkCreatePipelineLayout(_device, &layout_create_info, nullptr, &_pipelineLayout) == VK_SUCCESS);

    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,              // VkStructureType                                sType
        nullptr,                                                      // const void                                    *pNext
        0,                                                            // VkPipelineCreateFlags                          flags
        static_cast<uint32_t>(shader_stage_create_infos.size()),      // uint32_t                                       stageCount
        &shader_stage_create_infos[0],                                // const VkPipelineShaderStageCreateInfo         *pStages
        &vertex_input_state_create_info,                              // const VkPipelineVertexInputStateCreateInfo    *pVertexInputState;
        &input_assembly_state_create_info,                            // const VkPipelineInputAssemblyStateCreateInfo  *pInputAssemblyState
        nullptr,                                                      // const VkPipelineTessellationStateCreateInfo   *pTessellationState
        &viewport_state_create_info,                                  // const VkPipelineViewportStateCreateInfo       *pViewportState
        &rasterization_state_create_info,                             // const VkPipelineRasterizationStateCreateInfo  *pRasterizationState
        &multisample_state_create_info,                               // const VkPipelineMultisampleStateCreateInfo    *pMultisampleState
        nullptr,                                                      // const VkPipelineDepthStencilStateCreateInfo   *pDepthStencilState
        &color_blend_state_create_info,                               // const VkPipelineColorBlendStateCreateInfo     *pColorBlendState
        nullptr,                                                      // const VkPipelineDynamicStateCreateInfo        *pDynamicState
        _pipelineLayout,                                              // VkPipelineLayout                               layout
        _mainRenderPass,                                              // VkRenderPass                                   renderPass
        0,                                                            // uint32_t                                       subpass
        VK_NULL_HANDLE,                                               // VkPipeline                                     basePipelineHandle
        0                                                             // int32_t                                        basePipelineIndex
    };

    Assert(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &_pipeline) == VK_SUCCESS);

    vkDestroyShaderModule(_device, fragModule, nullptr);
    vkDestroyShaderModule(_device, vertModule, nullptr);
}

void OldRenderer::createMeshBuffers()
{
    const Mesh& mesh = _meshCache[MeshFile];

    const std::size_t gpuBufferSize = 1_MB;
    GPUStackAllocator gpuStack(gpuBufferSize);

    struct Vertex
    {
        glm::vec3 position;
        glm::vec2 uv;
    };

    std::vector<Vertex> vertexData(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
    {
        vertexData[i].position = mesh.vertices[i];
        vertexData[i].uv = mesh.uvs[i];
    }

    const std::size_t vertexDataBytes = vertexData.size() * sizeof(vertexData[0]);
    const std::size_t indexDataBytes = mesh.indexes.size() * sizeof(mesh.indexes[0]);

    VkMemoryRequirements memReqsVertex;
    VkMemoryRequirements memReqsIndex;

    auto memoryHeaps = enumerateHeaps(_physicalDevice);
    _deviceMemory = AllocateMemory(memoryHeaps, _device, gpuBufferSize);
    _vertexBuffer = AllocateBuffer(_device, vertexDataBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    vkGetBufferMemoryRequirements(_device, _vertexBuffer, &memReqsVertex);

    const std::size_t vertexBufferOffset = gpuStack.alloc(vertexDataBytes, memReqsVertex.alignment);

    vkBindBufferMemory(_device, _vertexBuffer, _deviceMemory, vertexBufferOffset);

    _indexBuffer = AllocateBuffer (_device, indexDataBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    vkGetBufferMemoryRequirements(_device, _indexBuffer, &memReqsIndex);

    const std::size_t indexBufferOffset = gpuStack.alloc(indexDataBytes, memReqsIndex.alignment);

    vkBindBufferMemory(_device, _indexBuffer, _deviceMemory, indexBufferOffset);

    void* mapping = nullptr;
    Assert(vkMapMemory(_device, _deviceMemory, 0, VK_WHOLE_SIZE, 0, &mapping) == VK_SUCCESS);
    std::memcpy(static_cast<uint8_t*>(mapping) + vertexBufferOffset, vertexData.data(), vertexDataBytes);
    std::memcpy(static_cast<uint8_t*>(mapping) + indexBufferOffset, mesh.indexes.data(), indexDataBytes);
    vkUnmapMemory (_device, _deviceMemory);
}

namespace
{
    VkFormat pixelFormatToVkFormat(u32 format)
    {
        return static_cast<VkFormat>(format);
    }

    void setImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange)
    {
        // Create an image barrier object
        VkImageMemoryBarrier imageMemoryBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType                sType
            nullptr,                                    // const void                    *pNext
            0,                                          // VkAccessFlags                  srcAccessMask
            0,                                          // VkAccessFlags                  dstAccessMask
            oldImageLayout,                             // VkImageLayout                  oldLayout
            newImageLayout,                             // VkImageLayout                  newLayout
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                       srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                       dstQueueFamilyIndex
            image,                                      // VkImage                        image
            subresourceRange                            // VkImageSubresourceRange        subresourceRange
        };

        // Only sets masks for layouts used in this example
        // For a more complete version that can be used with other layouts see vkTools::setImageLayout

        // Source layouts (old)
        switch (oldImageLayout)
        {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                // Only valid as initial layout, memory contents are not preserved
                // Can be accessed directly, no source dependency required
                imageMemoryBarrier.srcAccessMask = 0;
                break;
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                // Only valid as initial layout for linear images, preserves memory contents
                // Make sure host writes to the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Old layout is transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;
            default:
                Assert(false, "Unsupported old image layout");
                break;
        }

        // Target layouts (new)
        switch (newImageLayout)
        {
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Transfer source (copy, blit)
                // Make sure any reads from the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Transfer destination (copy, blit)
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Shader read (sampler, input attachment)
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                Assert(false, "Unsupported new image layout");
                break;
        }

        // Put barrier on top of pipeline
        VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        // Put barrier inside setup command buffer
        vkCmdPipelineBarrier(
                cmdBuffer,
                srcStageFlags,
                destStageFlags,
                VK_FLAGS_NONE,
                0, nullptr,
                0, nullptr,
                1, &imageMemoryBarrier);
    }
}

void OldRenderer::createTextures()
{
    const Texture& texture = _texCache[TextureFile.c_str()];

    VkFormat format = pixelFormatToVkFormat(texture.format);
    Assert(format != VK_FORMAT_UNDEFINED);

    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(_physicalDevice, format, &formatProperties);

    bool useStaging = true;
    {
        // Create a host-visible staging buffer that contains the raw image data
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = texture.size;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; // This buffer is used as a transfer source for the buffer copy
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        Assert(vkCreateBuffer(_device, &bufferCreateInfo, nullptr, &stagingBuffer) == VK_SUCCESS);

        VkMemoryRequirements memReqs;
        // Get memory requirements for the staging buffer (alignment, memory type bits)
        vkGetBufferMemoryRequirements(_device, stagingBuffer, &memReqs);

        VkMemoryAllocateInfo memAllocInfo = {};
        memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAllocInfo.pNext = nullptr;
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = getMemoryType(_physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        Assert(vkAllocateMemory(_device, &memAllocInfo, nullptr, &stagingMemory) == VK_SUCCESS);
        Assert(vkBindBufferMemory(_device, stagingBuffer, stagingMemory, 0) == VK_SUCCESS);

        // Copy texture data into staging buffer
        uint8_t *data = nullptr;
        Assert(vkMapMemory(_device, stagingMemory, 0, memReqs.size, 0, (void**)&data) == VK_SUCCESS);
        std::memcpy(data, texture.rawData, texture.size);
        vkUnmapMemory(_device, stagingMemory);

        // Setup buffer copy regions for each mip level
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        uint32_t offset = 0;

        for (uint32_t currentMip = 0; currentMip < texture.mipLevels; currentMip++)
        {
            VkBufferImageCopy bufferCopyRegion = {};
            u32 mipWidth = texture.width >> currentMip;
            u32 mipHeight = texture.height >> currentMip;
            u32 mipSize = texture.bytesPerPixel * mipHeight * mipHeight;

            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = currentMip;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = mipWidth;
            bufferCopyRegion.imageExtent.height = mipHeight;
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);

            offset += mipSize;
        }

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.pNext = nullptr;
        imageCreateInfo.flags = 0;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = texture.mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Set initial layout of the image to undefined
        imageCreateInfo.extent = { texture.width, texture.height, 1 };
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        Assert(vkCreateImage(_device, &imageCreateInfo, nullptr, &_texImage) == VK_SUCCESS);

        vkGetImageMemoryRequirements(_device, _texImage, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = getMemoryType(_physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        Assert(vkAllocateMemory(_device, &memAllocInfo, nullptr, &_texMemory) == VK_SUCCESS);
        Assert(vkBindImageMemory(_device, _texImage, _texMemory, 0) == VK_SUCCESS);

        VkCommandBufferAllocateInfo cmdBufAllocateInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // VkStructureType              sType
            nullptr,                                        // const void*                  pNext
            _gfxCmdPool,                                    // VkCommandPool                commandPool
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,                // VkCommandBufferLevel         level
            1                                               // uint32_t                     bufferCount
        };

        VkCommandBuffer copyCmdBuffer = VK_NULL_HANDLE;
        Assert(vkAllocateCommandBuffers(_device, &cmdBufAllocateInfo, &copyCmdBuffer) == VK_SUCCESS);

        VkCommandBufferBeginInfo graphics_command_buffer_begin_info = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,    // VkStructureType                        sType
            nullptr,                                        // const void                            *pNext
            VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,   // VkCommandBufferUsageFlags              flags
            nullptr                                         // const VkCommandBufferInheritanceInfo  *pInheritanceInfo
        };

        Assert(vkBeginCommandBuffer(copyCmdBuffer, &graphics_command_buffer_begin_info) == VK_SUCCESS);

        // Image barrier for optimal image

        // The sub resource range describes the regions of the image we will be transition
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Image only contains color data
        subresourceRange.baseMipLevel = 0; // Start at first mip level
        subresourceRange.levelCount = texture.mipLevels; // We will transition on all mip levels
        subresourceRange.layerCount = 1; // The 2D texture only has one layer

        // Optimal image will be used as destination for the copy, so we must transfer from our
        // initial undefined image layout to the transfer destination layout
        setImageLayout(
                copyCmdBuffer,
                _texImage,
                //             VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                subresourceRange);

        // Copy mip levels from staging buffer
        vkCmdCopyBufferToImage(
                copyCmdBuffer,
                stagingBuffer,
                _texImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                static_cast<uint32_t>(bufferCopyRegions.size()),
                bufferCopyRegions.data());

        // Change texture image layout to shader read after all mip levels have been copied
        VkImageLayout texImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        setImageLayout(
                copyCmdBuffer,
                _texImage,
                //             VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                texImageLayout,
                subresourceRange);

        Assert(vkEndCommandBuffer(copyCmdBuffer) == VK_SUCCESS);

        VkSubmitInfo submit_info = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,      // VkStructureType              sType
            nullptr,                            // const void                  *pNext
            0,                                  // uint32_t                     waitSemaphoreCount
            nullptr,                            // const VkSemaphore           *pWaitSemaphores
            nullptr,                            // const VkPipelineStageFlags  *pWaitDstStageMask;
            1,                                  // uint32_t                     commandBufferCount
            &copyCmdBuffer,                 // const VkCommandBuffer       *pCommandBuffers
            0,                                  // uint32_t                     signalSemaphoreCount
            nullptr                             // const VkSemaphore           *pSignalSemaphores
        };

        Assert(vkQueueSubmit(_graphicsQueue, 1, &submit_info, VK_NULL_HANDLE) == VK_SUCCESS);
        Assert(vkQueueWaitIdle(_graphicsQueue) == VK_SUCCESS);
        vkFreeCommandBuffers(_device, _gfxCmdPool, 1, &copyCmdBuffer);

        // Clean up staging resources
        vkFreeMemory(_device, stagingMemory, nullptr);
        vkDestroyBuffer(_device, stagingBuffer, nullptr);

        // Create sampler
        // In Vulkan textures are accessed by samplers
        // This separates all the sampling information from the
        // texture data
        // This means you could have multiple sampler objects
        // for the same texture with different settings
        // Similar to the samplers available with OpenGL 3.3
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.pNext = nullptr;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerInfo.minLod = 0.0f;
        // Max level-of-detail should match mip level count
        samplerInfo.maxLod = (useStaging) ? (float)texture.mipLevels : 0.0f;
        // Enable anisotropic filtering
        samplerInfo.maxAnisotropy = 8;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        Assert(vkCreateSampler(_device, &samplerInfo, nullptr, &_texSampler) == VK_SUCCESS);

        // Create image view
        // Textures are not directly accessed by the shaders and
        // are abstracted by image views containing additional
        // information and sub resource ranges
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.pNext = nullptr;
        viewInfo.image = VK_NULL_HANDLE;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        // Linear tiling usually won't support mip maps
        // Only set mip map count if optimal tiling is used
        viewInfo.subresourceRange.levelCount = (useStaging) ? texture.mipLevels : 1;
        viewInfo.image = _texImage;

        Assert(vkCreateImageView(_device, &viewInfo, nullptr, &_texImageView) == VK_SUCCESS);
    }
}

void OldRenderer::createDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                 sType;
        nullptr,                                        // const void*                     pNext;
        _descriptorPool,                                // VkDescriptorPool                descriptorPool;
        1,                                              // uint32_t                        descriptorSetCount;
        &_descriptorSetLayout                           // const VkDescriptorSetLayout*    pSetLayouts;
    };

    Assert(vkAllocateDescriptorSets(_device, &allocInfo, &_descriptorSet) == VK_SUCCESS);

    // Image descriptor for the color map texture
    VkDescriptorImageInfo descriptorImageInfo = {};
    descriptorImageInfo.sampler = _texSampler;
    descriptorImageInfo.imageView = _texImageView;
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets =
    {
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType                  sType;
            nullptr,                                // const void*                      pNext;
            _descriptorSet,                         // VkDescriptorSet                  dstSet;
            0,                                      // uint32_t                         dstBinding;
            0,                                      // uint32_t                         dstArrayElement;
            1,                                      // uint32_t                         descriptorCount;
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,      // VkDescriptorType                 descriptorType;
            nullptr,                                // const VkDescriptorImageInfo*     pImageInfo;
            &_uniformData.descriptor,               // const VkDescriptorBufferInfo*    pBufferInfo;
            nullptr                                 // const VkBufferView*              pTexelBufferView;
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType                  sType;
            nullptr,                                // const void*                      pNext;
            _descriptorSet,                         // VkDescriptorSet                  dstSet;
            1,                                      // uint32_t                         dstBinding;
            0,                                      // uint32_t                         dstArrayElement;
            1,                                      // uint32_t                         descriptorCount;
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  // VkDescriptorType             descriptorType;
            &descriptorImageInfo,                   // const VkDescriptorImageInfo*     pImageInfo;
            nullptr,                                // const VkDescriptorBufferInfo*    pBufferInfo;
            nullptr                                 // const VkBufferView*              pTexelBufferView;
        }
    };

    vkUpdateDescriptorSets(_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}

void OldRenderer::updateUniforms()
{
    _uniforms->transform = glm::scale(glm::mat4(), glm::vec3(0.5f, 0.5f, 0.5f));
    _uniforms->time += 0.01f;
    _uniforms->scaleFactor = sinf(_uniforms->time);

    // Map uniform buffer and update it
    // If you want to keep a handle to the memory and not unmap it afer updating,
    // create the memory with the VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    uint8_t *pData = nullptr;
    Assert(vkMapMemory(_device, _uniformData.memory, 0, sizeof(*_uniforms), 0, (void **)&pData) == VK_SUCCESS);

    std::memcpy(pData, _uniforms, sizeof(*_uniforms));

    vkUnmapMemory(_device, _uniformData.memory);
}

void test_vulkan_renderer(ReaperRoot& root, VulkanBackend& backend)
{
    log_info(root, "vulkan: running test function");

    VkCommandPool gfxCmdPool = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo cmd_pool_create_info = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,     // VkStructureType              sType
        nullptr,                                        // const void*                  pNext
        0,                                              // VkCommandPoolCreateFlags     flags
        backend.physicalDeviceInfo.graphicsQueueIndex   // uint32_t                     queueFamilyIndex
    };
    Assert(vkCreateCommandPool(backend.device, &cmd_pool_create_info, nullptr, &gfxCmdPool) == VK_SUCCESS);



    vkDestroyCommandPool(backend.device, gfxCmdPool, nullptr);
}

