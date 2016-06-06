////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "VulkanRenderer.h"

#include <cmath>
#include <cstring>

#include "Shader.h"
#include "Memory.h"

#include "renderer/mesh/ModelLoader.h"

using namespace vk;

namespace
{
    struct VSPushConstants {
        float scale;
    };
}

struct VSUBO {
    float time;
    float scaleFactor;
};

void VulkanRenderer::startup(Window* window)
{
    parent_type::startup(window);

    _pipeline = VK_NULL_HANDLE;
    _pipelineLayout = VK_NULL_HANDLE;

    _deviceMemory = VK_NULL_HANDLE;
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
    createDescriptorSet();
    createCommandBuffers();
}

void VulkanRenderer::shutdown()
{
    Assert(vkDeviceWaitIdle(_device) == VK_SUCCESS);

    vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr);
    vkDestroyBuffer(_device, _uniformData.buffer, nullptr);
    vkFreeMemory(_device, _uniformData.memory, nullptr);

    vkDestroyBuffer(_device, _vertexBuffer, nullptr);
    vkDestroyBuffer(_device, _indexBuffer, nullptr);
    vkFreeMemory(_device, _deviceMemory, nullptr);

    vkDestroyPipeline(_device, _pipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);

    parent_type::shutdown();
}

void VulkanRenderer::render()
{
    uint32_t image_index;

    VkResult result = vkAcquireNextImageKHR(_device, _swapChain, UINT64_MAX, _imageAvailableSemaphore, VK_NULL_HANDLE, &image_index);
    switch(result)
    {
        case VK_SUCCESS:
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
        &_gfxCmdBuffers[image_index],   // const VkCommandBuffer       *pCommandBuffers
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

void VulkanRenderer::createCommandBuffers()
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
                    300,                                        // int32_t                        width
                    300,                                        // int32_t                        height
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
        vkCmdDrawIndexed(_gfxCmdBuffers[i], 6, 1, 0, 0, 0);

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
                _gfxQueueIndex,               // uint32_t                     srcQueueFamilyIndex
                _prsQueueIndex,                // uint32_t                     dstQueueFamilyIndex
                _swapChainImages[i],                  // VkImage                      image
                image_subresource_range                       // VkImageSubresourceRange      subresourceRange
            };
            vkCmdPipelineBarrier(_gfxCmdBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_draw_to_present );
        }
        Assert(vkEndCommandBuffer(_gfxCmdBuffers[i]) == VK_SUCCESS);
    }
}

void VulkanRenderer::createDescriptorPool()
{
    // We need to tell the API the number of max. requested descriptors per type
    VkDescriptorPoolSize typeCounts[1];
    // This example only uses one descriptor type (uniform buffer) and only
    // requests one descriptor of this type
    typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    typeCounts[0].descriptorCount = 1;
    // For additional types you need to add new entries in the type count list
    // E.g. for two combined image samplers :
    // typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // typeCounts[1].descriptorCount = 2;

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
    descriptorPoolInfo.poolSizeCount = 1;
    descriptorPoolInfo.pPoolSizes = typeCounts;

    Assert(vkCreateDescriptorPool(_device, &descriptorPoolInfo, nullptr, &_descriptorPool) == VK_SUCCESS);
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.pNext = nullptr;
    descriptorLayout.bindingCount = 1;
    descriptorLayout.pBindings = &layoutBinding;

    Assert(vkCreateDescriptorSetLayout(_device, &descriptorLayout, nullptr, &_descriptorSetLayout) == VK_SUCCESS);

    {
        /*{
            VkBufferCreateInfo bufferInfo = {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(*_uniforms);
            bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

            // Create a new buffer
            Assert(vkCreateBuffer(_device, &bufferInfo, nullptr, &_uniformData.buffer) == VK_SUCCESS);

            auto memoryHeaps = enumerateHeaps(_physicalDevice);
            _uniformData.memory = AllocateMemory(memoryHeaps, _device, 10 << 10);
        }*/
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
            // Get the memory type index that supports host visibile memory access
            // Most implementations offer multiple memory tpyes and selecting the
            // correct one to allocate memory from is important
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
        _uniformData.descriptor.range = sizeof(_uniformData);

        updateUniforms();
    }
}

void VulkanRenderer::createPipeline()
{
    VkShaderModule vertModule;
    VkShaderModule fragModule;

    createShaderModule(vertModule, _device, "./build/spv/uniform.vert.spv");
    createShaderModule(fragModule, _device, "./build/spv/uniform.frag.spv");

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
    vertexInputBindingDescription.stride = sizeof (float) * 5;

    VkVertexInputAttributeDescription vertexInputAttributeDescription [2] = {};
    vertexInputAttributeDescription[0].binding = vertexInputBindingDescription.binding;
    vertexInputAttributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescription[0].location = 0;
    vertexInputAttributeDescription[0].offset = 0;

    vertexInputAttributeDescription[1].binding = vertexInputBindingDescription.binding;
    vertexInputAttributeDescription[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescription[1].location = 1;
    vertexInputAttributeDescription[1].offset = sizeof (float) * 3;

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
        300.0f,                                                       // float                                          width
        300.0f,                                                       // float                                          height
        0.0f,                                                         // float                                          minDepth
        1.0f                                                          // float                                          maxDepth
    };

    VkRect2D scissor = {
        {                                                             // VkOffset2D                                     offset
            0,                                                            // int32_t                                        x
            0                                                             // int32_t                                        y
        },
        {                                                             // VkExtent2D                                     extent
            300,                                                          // int32_t                                        width
            300                                                           // int32_t                                        height
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
        VK_CULL_MODE_BACK_BIT,                                        // VkCullModeFlags                                cullMode
        VK_FRONT_FACE_CLOCKWISE,                                      // VkFrontFace                                    frontFace
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
        _mainRenderPass,                                                  // VkRenderPass                                   renderPass
        0,                                                            // uint32_t                                       subpass
        VK_NULL_HANDLE,                                               // VkPipeline                                     basePipelineHandle
        -1                                                            // int32_t                                        basePipelineIndex
    };

    Assert(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &_pipeline) == VK_SUCCESS);

    vkDestroyShaderModule(_device, fragModule, nullptr);
    vkDestroyShaderModule(_device, vertModule, nullptr);
}

void VulkanRenderer::createMeshBuffers()
{
    ModelLoader loader;
    MeshCache cache;

    loader.load("res/model/quad.obj", cache);

    const Mesh& mesh = cache["res/model/quad.obj"];

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
    const std::size_t indexBufferOffset = ((vertexDataBytes / 16) + 1) * 16;

    auto memoryHeaps = enumerateHeaps(_physicalDevice);
    _deviceMemory = AllocateMemory(memoryHeaps, _device,
                                    640 << 10 /* 640 KiB ought to be enough for anybody */);
    _vertexBuffer = AllocateBuffer(_device, vertexDataBytes,
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    vkBindBufferMemory(_device, _vertexBuffer, _deviceMemory, 0);

    _indexBuffer = AllocateBuffer (_device, indexDataBytes,
                                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    vkBindBufferMemory(_device, _indexBuffer, _deviceMemory, indexBufferOffset);

    void* mapping = nullptr;
    vkMapMemory (_device, _deviceMemory, 0, VK_WHOLE_SIZE, 0, &mapping);
    ::memcpy(mapping, vertexData.data(), vertexDataBytes);
    ::memcpy(static_cast<uint8_t*> (mapping) + indexBufferOffset, mesh.indexes.data(), indexDataBytes);
    vkUnmapMemory (_device, _deviceMemory);
}

void VulkanRenderer::createDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,  // VkStructureType                 sType;
        nullptr,                                        // const void*                     pNext;
        _descriptorPool,                                // VkDescriptorPool                descriptorPool;
        1,                                              // uint32_t                        descriptorSetCount;
        &_descriptorSetLayout                            // const VkDescriptorSetLayout*    pSetLayouts;
    };

    Assert(vkAllocateDescriptorSets(_device, &allocInfo, &_descriptorSet) == VK_SUCCESS);

    VkWriteDescriptorSet writeDescriptorSet = {
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
    };

    vkUpdateDescriptorSets(_device, 1, &writeDescriptorSet, 0, nullptr);
}

void VulkanRenderer::updateUniforms()
{
//     _uniforms->transform = glm::scale(glm::mat4(), glm::vec3(0.5f, 0.5f, 0.5f));
    _uniforms->time += 0.001f;
    _uniforms->scaleFactor = sinf(_uniforms->time);

    // Map uniform buffer and update it
    // If you want to keep a handle to the memory and not unmap it afer updating,
    // create the memory with the VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    uint8_t *pData;
    Assert(vkMapMemory(_device, _uniformData.memory, 0, sizeof(*_uniforms), 0, (void **)&pData) == VK_SUCCESS);

    ::memcpy(pData, _uniforms, sizeof(*_uniforms));

    vkUnmapMemory(_device, _uniformData.memory);
}
