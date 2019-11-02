////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Test.h"

#include "Memory.h"
#include "Shader.h"
#include "Swapchain.h"
#include "SwapchainRendererBase.h"
#include "api/Vulkan.h"
#include "api/VulkanStringConversion.h"

#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/window/Event.h"
#include "renderer/window/Window.h"

#include "mesh/Mesh.h"
#include "mesh/ModelLoader.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/BitTricks.h"
#include "core/Profile.h"
#include "core/memory/Allocator.h"

namespace Reaper
{
namespace
{
    VkFormat PixelFormatToVulkan(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::R16G16B16A16_UNORM:
            return VK_FORMAT_R16G16B16A16_UNORM;
        case PixelFormat::R16G16B16A16_SFLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case PixelFormat::R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::R8G8B8A8_SRGB:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case PixelFormat::B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case PixelFormat::BC2_UNORM_BLOCK:
            return VK_FORMAT_BC2_UNORM_BLOCK;
        case PixelFormat::Unknown:
        default:
            AssertUnreachable();
        }
        return VK_FORMAT_UNDEFINED;
    }

    VkSampleCountFlagBits SampleCountToVulkan(u32 sampleCount)
    {
        Assert(sampleCount > 0);
        Assert(isPowerOfTwo(sampleCount));
        return static_cast<VkSampleCountFlagBits>(sampleCount);
    }

    void debug_memory_heap_properties(ReaperRoot& root, const VulkanBackend& backend, uint32_t memoryTypeIndex)
    {
        const VkMemoryType& memoryType = backend.physicalDeviceInfo.memory.memoryTypes[memoryTypeIndex];

        log_debug(root, "vulkan: selecting memory heap {} with these properties:", memoryType.heapIndex);
        for (u32 i = 0; i < sizeof(VkMemoryPropertyFlags) * 8; i++)
        {
            const VkMemoryPropertyFlags flag = 1 << i;

            if (memoryType.propertyFlags & flag)
                log_debug(root, "- {}", GetMemoryPropertyFlagBitToString(flag));
        }
    }
} // namespace

struct CmdBuffer
{
    VkCommandPool   cmdPool;
    VkCommandBuffer cmdBuffer;
};

struct PipelineInfo
{
    VkPipeline       pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet  descriptorSet;
    VkImage          image;
    VkImageView      imageView;
};

namespace
{
    void vulkan_cmd_transition_swapchain_layout(VulkanBackend& backend, VkCommandBuffer commandBuffer)
    {
        for (u32 swapchainImageIndex = 0; swapchainImageIndex < static_cast<u32>(backend.presentInfo.images.size());
             swapchainImageIndex++)
        {
            VkImageMemoryBarrier swapchainImageBarrierInfo = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                0, // VK_ACCESS_SHADER_WRITE_BIT ?
                VK_ACCESS_MEMORY_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                backend.physicalDeviceInfo.presentQueueIndex,
                backend.physicalDeviceInfo.presentQueueIndex,
                backend.presentInfo.images[swapchainImageIndex],
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &swapchainImageBarrierInfo);
        }
    }
} // namespace

static void vulkan_create_blit_pipeline(ReaperRoot& root, VulkanBackend& backend, VkPipeline& pipeline)
{
    VkShaderModule        blitShaderFS = VK_NULL_HANDLE;
    VkShaderModule        blitShaderVS = VK_NULL_HANDLE;
    const char*           fileNameVS = "./build/spv/mesh_simple.vert.spv";
    const char*           fileNameFS = "./build/spv/mesh_simple.frag.spv";
    const char*           entryPoint = "main";
    VkSpecializationInfo* specialization = nullptr;

    vulkan_create_shader_module(blitShaderFS, backend.device, fileNameFS);
    vulkan_create_shader_module(blitShaderVS, backend.device, fileNameVS);

    const u32                             meshVertexStride = sizeof(float) * 3; // Position only
    const VkVertexInputBindingDescription vertexInfoShaderBinding = {
        0,                          // binding
        meshVertexStride,           // stride
        VK_VERTEX_INPUT_RATE_VERTEX // input rate
    };

    constexpr u32                           vertexAttributesCount = 1;
    const VkVertexInputAttributeDescription vertexAttributes[vertexAttributesCount] = {
        0,                          // location
        0,                          // binding
        VK_FORMAT_R32G32B32_SFLOAT, // format
        0                           // offset
    };

    VkPipelineShaderStageCreateInfo blitShaderStages[2] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, blitShaderVS,
         entryPoint, specialization},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, blitShaderFS,
         entryPoint, specialization}};

    VkPipelineVertexInputStateCreateInfo blitVertexInputStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        VK_FLAGS_NONE,
        1,
        &vertexInfoShaderBinding,
        vertexAttributesCount,
        &vertexAttributes[0]};

    VkPipelineInputAssemblyStateCreateInfo blitInputAssemblyInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, VK_FLAGS_NONE,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};

    const VkExtent2D backbufferExtent = backend.presentInfo.surfaceExtent;

    VkViewport blitViewport = {
        0.0f, 0.0f, static_cast<float>(backbufferExtent.width), static_cast<float>(backbufferExtent.height),
        0.0f, 1.0f};

    VkRect2D blitScissors = {{0, 0}, backbufferExtent};

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
        VK_FRONT_FACE_CLOCKWISE, // FIXME
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

    // No descriptor set nor push constants yet
    VkPipelineLayoutCreateInfo blitPipelineLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, VK_FLAGS_NONE, 0, nullptr, 0, nullptr};

    VkPipelineLayout blitPipelineLayout = VK_NULL_HANDLE;

    Assert(vkCreatePipelineLayout(backend.device, &blitPipelineLayoutInfo, nullptr, &blitPipelineLayout) == VK_SUCCESS);
    log_debug(root, "vulkan: created blit pipeline layout with handle: {}", static_cast<void*>(blitPipelineLayout));

    VkPipelineCache cache = VK_NULL_HANDLE;

    VkGraphicsPipelineCreateInfo blitPipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                           nullptr,
                                                           VK_FLAGS_NONE,
                                                           static_cast<uint32_t>(2),
                                                           blitShaderStages,
                                                           &blitVertexInputStateInfo,
                                                           &blitInputAssemblyInfo,
                                                           nullptr,
                                                           &blitViewportStateInfo,
                                                           &blitRasterStateInfo,
                                                           &blitMSStateInfo,
                                                           nullptr,
                                                           &blitBlendStateInfo,
                                                           nullptr,
                                                           blitPipelineLayout,
                                                           backend.presentInfo.renderPass,
                                                           0,
                                                           VK_NULL_HANDLE,
                                                           -1};

    Assert(vkCreateGraphicsPipelines(backend.device, cache, 1, &blitPipelineCreateInfo, nullptr, &pipeline)
           == VK_SUCCESS);
    log_debug(root, "vulkan: created blit pipeline with handle: {}", static_cast<void*>(pipeline));

    Assert(backend.physicalDeviceInfo.graphicsQueueIndex == backend.physicalDeviceInfo.presentQueueIndex);

    vkDestroyShaderModule(backend.device, blitShaderVS, nullptr);
    vkDestroyShaderModule(backend.device, blitShaderFS, nullptr);
    vkDestroyPipelineLayout(backend.device, blitPipelineLayout, nullptr);
}

struct MeshBuffer
{
    VkBuffer       vertexBuffer;
    VkBuffer       indexBuffer;
    u32            indexBufferMemoryOffset;
    VkDeviceMemory deviceMemory;
};

static void create_mesh_gpu_buffers(ReaperRoot& root, const VulkanBackend& backend, const Mesh& mesh,
                                    MeshBuffer& meshBuffer)
{
    const VkBufferCreateInfo vertexBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                 nullptr,
                                                 VK_FLAGS_NONE,
                                                 mesh.vertices.size() * sizeof(mesh.vertices[0]),
                                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                 VK_SHARING_MODE_EXCLUSIVE,
                                                 0,
                                                 nullptr};

    const VkBufferCreateInfo indexBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                nullptr,
                                                VK_FLAGS_NONE,
                                                mesh.indexes.size() * sizeof(mesh.indexes[0]),
                                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                VK_SHARING_MODE_EXCLUSIVE,
                                                0,
                                                nullptr};

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    Assert(vkCreateBuffer(backend.device, &vertexBufferInfo, nullptr, &vertexBuffer) == VK_SUCCESS);

    log_debug(root, "vulkan: created vertex buffer with handle: {}", static_cast<void*>(vertexBuffer));

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    Assert(vkCreateBuffer(backend.device, &indexBufferInfo, nullptr, &indexBuffer) == VK_SUCCESS);

    log_debug(root, "vulkan: created index buffer with handle: {}", static_cast<void*>(indexBuffer));

    VkMemoryRequirements vertexBufferMemoryRequirements;
    VkMemoryRequirements indexBufferMemoryRequirements;
    vkGetBufferMemoryRequirements(backend.device, vertexBuffer, &vertexBufferMemoryRequirements);
    vkGetBufferMemoryRequirements(backend.device, indexBuffer, &indexBufferMemoryRequirements);

    log_debug(root, "- vertex buffer memory requirements: size = {}, alignment = {}, types = {:#b}",
              vertexBufferMemoryRequirements.size, vertexBufferMemoryRequirements.alignment,
              vertexBufferMemoryRequirements.memoryTypeBits);

    log_debug(root, "- index buffer memory requirements: size = {}, alignment = {}, types = {:#b}",
              indexBufferMemoryRequirements.size, indexBufferMemoryRequirements.alignment,
              indexBufferMemoryRequirements.memoryTypeBits);

    const VkMemoryPropertyFlags requiredMemoryType =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const uint32_t vertexBufferMemoryTypeIndex =
        getMemoryType(backend.physicalDevice, vertexBufferMemoryRequirements.memoryTypeBits, requiredMemoryType);
    const uint32_t indexBufferMemoryTypeIndex =
        getMemoryType(backend.physicalDevice, indexBufferMemoryRequirements.memoryTypeBits, requiredMemoryType);

    Assert(vertexBufferMemoryTypeIndex == indexBufferMemoryTypeIndex);
    Assert(vertexBufferMemoryRequirements.alignment == indexBufferMemoryRequirements.alignment);
    Assert(vertexBufferMemoryTypeIndex < backend.physicalDeviceInfo.memory.memoryTypeCount,
           "invalid memory type index");

    debug_memory_heap_properties(root, backend, vertexBufferMemoryTypeIndex);

    // ALIGN PROPERLY
    const VkDeviceSize vertexBufferSizeAligned =
        alignOffset(vertexBufferMemoryRequirements.size, vertexBufferMemoryRequirements.alignment);
    const VkDeviceSize indexBufferSizeAligned =
        alignOffset(indexBufferMemoryRequirements.size, indexBufferMemoryRequirements.alignment);
    const VkDeviceSize sharedBufferAllocSize = vertexBufferSizeAligned + indexBufferSizeAligned;
    const VkDeviceSize indexBufferMemoryOffset = vertexBufferSizeAligned;

    const VkMemoryAllocateInfo sharedBufferAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr,
                                                        sharedBufferAllocSize, vertexBufferMemoryTypeIndex};

    VkDeviceMemory sharedBufferMemory = VK_NULL_HANDLE;
    Assert(vkAllocateMemory(backend.device, &sharedBufferAllocInfo, nullptr, &sharedBufferMemory) == VK_SUCCESS);

    log_debug(root, "vulkan: allocated memory with handle: {}", static_cast<void*>(sharedBufferMemory));

    Assert(vkBindBufferMemory(backend.device, vertexBuffer, sharedBufferMemory, 0) == VK_SUCCESS);
    Assert(vkBindBufferMemory(backend.device, indexBuffer, sharedBufferMemory, indexBufferMemoryOffset) == VK_SUCCESS);

    meshBuffer.vertexBuffer = vertexBuffer;
    meshBuffer.indexBuffer = indexBuffer;
    meshBuffer.indexBufferMemoryOffset = indexBufferMemoryOffset;
    meshBuffer.deviceMemory = sharedBufferMemory;
}

static void upload_mesh_data_to_gpu(const VulkanBackend& backend, const Mesh& mesh, const MeshBuffer& meshBuffer)
{
    u8* vertexWritePtr = nullptr;
    u8* indexWritePtr = nullptr;

    Assert(vkMapMemory(backend.device, meshBuffer.deviceMemory, 0, VK_WHOLE_SIZE, 0,
                       reinterpret_cast<void**>(&vertexWritePtr))
           == VK_SUCCESS);
    indexWritePtr = vertexWritePtr + meshBuffer.indexBufferMemoryOffset;

    memcpy(vertexWritePtr, mesh.vertices.data(), mesh.vertices.size() * sizeof(mesh.vertices[0]));
    memcpy(indexWritePtr, mesh.indexes.data(), mesh.indexes.size() * sizeof(mesh.indexes[0]));

    vkUnmapMemory(backend.device, meshBuffer.deviceMemory);
}

static void vulkan_test_draw(ReaperRoot& root, VulkanBackend& backend, PipelineInfo& pipelineInfo)
{
    REAPER_PROFILE_SCOPE("Vulkan", MP_LIGHTYELLOW);
    // Read mesh file
    std::ifstream modelFile("res/model/quad.obj");
    const Mesh    mesh = ModelLoader::loadOBJ(modelFile);

    // Create vk buffers
    MeshBuffer meshBuffer = {};
    create_mesh_gpu_buffers(root, backend, mesh, meshBuffer);
    upload_mesh_data_to_gpu(backend, mesh, meshBuffer);

    // FIXME pushing compute shader on the graphics queue...
    CmdBuffer cmdInfo = {VK_NULL_HANDLE, VK_NULL_HANDLE};

    VkCommandPoolCreateInfo poolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                              backend.physicalDeviceInfo.graphicsQueueIndex};

    Assert(vkCreateCommandPool(backend.device, &poolCreateInfo, nullptr, &cmdInfo.cmdPool) == VK_SUCCESS);
    log_debug(root, "vulkan: created command pool with handle: {}", static_cast<void*>(cmdInfo.cmdPool));

    VkCommandBufferAllocateInfo cmdBufferAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
                                                      cmdInfo.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};

    Assert(vkAllocateCommandBuffers(backend.device, &cmdBufferAllocInfo, &cmdInfo.cmdBuffer) == VK_SUCCESS);
    log_debug(root, "vulkan: created command buffer with handle: {}", static_cast<void*>(cmdInfo.cmdBuffer));

    VkCommandBufferBeginInfo cmdBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
        0,      // Not caring yet
        nullptr // No inheritance yet
    };

    VkImageMemoryBarrier imageBarrierInfo = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        nullptr,
        0, // VK_ACCESS_SHADER_WRITE_BIT ?
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        backend.physicalDeviceInfo.graphicsQueueIndex,
        backend.physicalDeviceInfo.graphicsQueueIndex,
        pipelineInfo.image,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

    Assert(vkBeginCommandBuffer(cmdInfo.cmdBuffer, &cmdBufferBeginInfo) == VK_SUCCESS);

    vkCmdPipelineBarrier(cmdInfo.cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &imageBarrierInfo);

    vkCmdBindPipeline(cmdInfo.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineInfo.pipeline);
    vkCmdBindDescriptorSets(cmdInfo.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineInfo.pipelineLayout, 0, 1,
                            &pipelineInfo.descriptorSet, 0, nullptr);
    vkCmdDispatch(cmdInfo.cmdBuffer, 1, 1, 1);

    Assert(vkEndCommandBuffer(cmdInfo.cmdBuffer) == VK_SUCCESS);

    VkFenceCreateInfo fenceInfo = {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
        0 // Not signaled by default
    };

    VkFence drawFence = VK_NULL_HANDLE;

    vkCreateFence(backend.device, &fenceInfo, nullptr, &drawFence);
    log_debug(root, "vulkan: created fence with handle: {}", static_cast<void*>(drawFence));

    VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdInfo.cmdBuffer, 0, nullptr};

    log_debug(root, "vulkan: queue submit");
    Assert(vkQueueSubmit(backend.deviceInfo.graphicsQueue, 1, &submitInfo, drawFence) == VK_SUCCESS);

    log_debug(root, "vulkan: wait for fence");
    {
        const u64 waitTimeoutUs = 1000000;
        VkResult  waitResult = vkWaitForFences(backend.device, 1, &drawFence, VK_TRUE, waitTimeoutUs);
        Assert(waitResult == VK_SUCCESS || waitResult == VK_TIMEOUT);
        Assert(waitResult != VK_TIMEOUT); // TODO Handle timeout correctly

        Assert(vkGetFenceStatus(backend.device, drawFence) == VK_SUCCESS);

        Assert(vkResetFences(backend.device, 1, &drawFence) == VK_SUCCESS);
    }

    VkPipeline blitPipeline = VK_NULL_HANDLE;
    vulkan_create_blit_pipeline(root, backend, blitPipeline);

    IWindow*                   window = root.renderer->window;
    std::vector<Window::Event> events;

    log_info(root, "window: map window");
    window->map();

    const int MaxFrameCount = 0;

    bool mustTransitionSwapchain = true;
    bool shouldExit = false;
    int  frameIndex = 0;

    while (!shouldExit)
    {
        REAPER_PROFILE_SCOPE("Vulkan", MP_YELLOW);
        log_info(root, "window: pump events");
        window->pumpEvents(events);
        if (!events.empty())
        {
            // Should be front but whatever
            Window::Event event = events.back();

            if (event.type == Window::EventType::Resize)
            {
                const u32 width = event.message.resize.width;
                const u32 height = event.message.resize.height;
                log_debug(root, "window: resize event, width = {}, height = {}", width, height);

                resize_vulkan_wm_swapchain(root, backend, backend.presentInfo, {width, height});
                mustTransitionSwapchain = true;
            }
            else if (event.type == Window::EventType::KeyPress)
            {
                shouldExit = true;
                log_warning(root, "window: key press detected: now exiting...");
            }
            else
            {
                log_warning(root, "window: an unknown event has been caught and will not be handled");
            }

            events.pop_back();
        }

        log_debug(root, "vulkan: begin frame {}", frameIndex);

        VkResult acquireResult;
        u64      acquireTimeoutUs = 1000000000;
        uint32_t imageIndex = 0;

        constexpr u32 MaxAcquireTryCount = 10;

        for (u32 acquireTryCount = 0; acquireTryCount < MaxAcquireTryCount; acquireTryCount++)
        {
            log_debug(root, "vulkan: acquiring frame try #{}", acquireTryCount);
            acquireResult =
                vkAcquireNextImageKHR(backend.device, backend.presentInfo.swapchain, acquireTimeoutUs,
                                      backend.presentInfo.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

            if (acquireResult != VK_NOT_READY)
                break;
        }

        switch (acquireResult)
        {
        case VK_NOT_READY: // Should be good after X tries
            log_debug(root, "vulkan: vkAcquireNextImageKHR not ready");
            break;
        case VK_SUBOPTIMAL_KHR:
            log_debug(root, "vulkan: vkAcquireNextImageKHR suboptimal");
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
            log_warning(root, "vulkan: vkAcquireNextImageKHR out of date");
            continue;
            break;
        default:
            Assert(acquireResult == VK_SUCCESS);
            break;
        }

        log_debug(root, "vulkan: image index = {}", imageIndex);

        // VkImage currentBackBuffer = backend.presentInfo.images[imageIndex];

        VkClearValue     clearValue = {{1.0f, 0.8f, 0.4f, 0.0f}};
        const VkExtent2D backbufferExtent = backend.presentInfo.surfaceExtent;
        VkRect2D         blitPassRect = {{0, 0}, backbufferExtent};

        VkRenderPassBeginInfo blitRenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                         nullptr,
                                                         backend.presentInfo.renderPass,
                                                         backend.presentInfo.framebuffers[imageIndex],
                                                         blitPassRect,
                                                         1,
                                                         &clearValue};

        // Only wait for a previous frame fence
        if (frameIndex > 0)
        {
            log_debug(root, "vulkan: wait for fence");
            VkResult waitResult;
            {
                REAPER_PROFILE_SCOPE("Vulkan", MP_ORANGE);
                const u64 waitTimeoutUs = 10000000;
                waitResult = vkWaitForFences(backend.device, 1, &drawFence, VK_TRUE, waitTimeoutUs);
            }

            if (waitResult != VK_SUCCESS)
                log_debug(root, "- return result {}", GetResultToString(waitResult));

            // TODO Handle timeout correctly
            Assert(waitResult == VK_SUCCESS);

            Assert(vkGetFenceStatus(backend.device, drawFence) == VK_SUCCESS);

            log_debug(root, "vulkan: reset fence");
            Assert(vkResetFences(backend.device, 1, &drawFence) == VK_SUCCESS);
        }

        log_debug(root, "vulkan: record command buffer");
        Assert(vkResetCommandBuffer(cmdInfo.cmdBuffer, 0) == VK_SUCCESS);
        Assert(vkBeginCommandBuffer(cmdInfo.cmdBuffer, &cmdBufferBeginInfo) == VK_SUCCESS);

        if (mustTransitionSwapchain)
        {
            vulkan_cmd_transition_swapchain_layout(backend, cmdInfo.cmdBuffer);
            mustTransitionSwapchain = false;
        }

        vkCmdBeginRenderPass(cmdInfo.cmdBuffer, &blitRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmdInfo.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blitPipeline);

        constexpr u32      vertexBufferCount = 1;
        const VkDeviceSize offsets[vertexBufferCount] = {0};
        vkCmdBindIndexBuffer(cmdInfo.cmdBuffer, meshBuffer.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers(cmdInfo.cmdBuffer, 0, vertexBufferCount, &meshBuffer.vertexBuffer, offsets);

        vkCmdDrawIndexed(cmdInfo.cmdBuffer, mesh.indexes.size(), 1, 0, 0, 0);

        vkCmdEndRenderPass(cmdInfo.cmdBuffer);

        Assert(vkEndCommandBuffer(cmdInfo.cmdBuffer) == VK_SUCCESS);
        // Stop recording

        VkPipelineStageFlags blitWaitDstMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

        VkSubmitInfo blitSubmitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                       nullptr,
                                       1,
                                       &backend.presentInfo.imageAvailableSemaphore,
                                       &blitWaitDstMask,
                                       1,
                                       &cmdInfo.cmdBuffer,
                                       1,
                                       &backend.presentInfo.renderingFinishedSemaphore};

        log_debug(root, "vulkan: submit drawing commands");
        Assert(vkQueueSubmit(backend.deviceInfo.graphicsQueue, 1, &blitSubmitInfo, drawFence) == VK_SUCCESS);

        log_debug(root, "vulkan: present");

        VkResult         presentResult;
        VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                        nullptr,
                                        1,
                                        &backend.presentInfo.renderingFinishedSemaphore,
                                        1,
                                        &backend.presentInfo.swapchain,
                                        &imageIndex,
                                        &presentResult};
        Assert(vkQueuePresentKHR(backend.deviceInfo.presentQueue, &presentInfo) == VK_SUCCESS);
        Assert(presentResult == VK_SUCCESS);

        frameIndex++;
        if (frameIndex == MaxFrameCount)
            shouldExit = true;
    }

    vkQueueWaitIdle(backend.deviceInfo.presentQueue);

    log_info(root, "window: unmap window");
    window->unmap();

    vkDestroyBuffer(backend.device, meshBuffer.vertexBuffer, nullptr);
    vkDestroyBuffer(backend.device, meshBuffer.indexBuffer, nullptr);
    vkFreeMemory(backend.device, meshBuffer.deviceMemory, nullptr);

    vkDestroyPipeline(backend.device, blitPipeline, nullptr);

    // Destroy resources
    vkDestroyFence(backend.device, drawFence, nullptr);
    vkFreeCommandBuffers(backend.device, cmdInfo.cmdPool, 1, &cmdInfo.cmdBuffer);
    vkDestroyCommandPool(backend.device, cmdInfo.cmdPool, nullptr);
}

static void vulkan_test_pipeline(ReaperRoot& root, VulkanBackend& backend, VkImage image, VkImageView imageView)
{
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingTest = {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                                                   VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                               nullptr, 0, 1, &descriptorSetLayoutBindingTest};

    VkDescriptorSetLayout descriptorSetLayoutOne = VK_NULL_HANDLE;
    Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayoutOne)
           == VK_SUCCESS);

    log_debug(root, "vulkan: created descriptor set layout with handle: {}",
              static_cast<void*>(descriptorSetLayoutOne));

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &descriptorSetLayoutOne, 0, nullptr};

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    Assert(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

    log_debug(root, "vulkan: created pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

    // TODO make this a real shader
    VkShaderModule        computeShader = VK_NULL_HANDLE;
    const char*           fileName = "./build/spv/test.comp.spv";
    const char*           entryPoint = "main";
    VkSpecializationInfo* specialization = nullptr;

    vulkan_create_shader_module(computeShader, backend.device, fileName);

    VkPipelineShaderStageCreateInfo shaderStage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                   nullptr,
                                                   0,
                                                   VK_SHADER_STAGE_COMPUTE_BIT,
                                                   computeShader,
                                                   entryPoint,
                                                   specialization};

    VkComputePipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                      nullptr,
                                                      0,
                                                      shaderStage,
                                                      pipelineLayout,
                                                      VK_NULL_HANDLE, // do not care about pipeline derivatives
                                                      0};

    VkPipeline      pipeline = VK_NULL_HANDLE;
    VkPipelineCache cache = VK_NULL_HANDLE;

    Assert(vkCreateComputePipelines(backend.device, cache, 1, &pipelineCreateInfo, nullptr, &pipeline) == VK_SUCCESS);

    log_debug(root, "vulkan: created pipeline with handle: {}", static_cast<void*>(pipeline));

    std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {{VK_DESCRIPTOR_TYPE_SAMPLER, 10},
                                                             {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10},
                                                             {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10}};

    constexpr u32 MaxSets = 100;

    VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                           nullptr,
                                           0, // No special alloc flags
                                           MaxSets,
                                           static_cast<uint32_t>(descriptorPoolSizes.size()),
                                           descriptorPoolSizes.data()};

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    Assert(vkCreateDescriptorPool(backend.device, &poolInfo, nullptr, &descriptorPool) == VK_SUCCESS);
    log_debug(root, "vulkan: created descriptor pool with handle: {}", static_cast<void*>(descriptorPool));

    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                          descriptorPool, 1, &descriptorSetLayoutOne};

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &descriptorSet) == VK_SUCCESS);
    log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(descriptorSet));

    VkDescriptorImageInfo descriptorSetImageInfo = {VK_NULL_HANDLE, imageView, VK_IMAGE_LAYOUT_GENERAL};

    VkWriteDescriptorSet descriptorSetWrite = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        descriptorSet,
        0, // 0th binding
        0, // not an array
        1, // not an array
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        &descriptorSetImageInfo,
        nullptr,
        nullptr,
    };

    vkUpdateDescriptorSets(backend.device, 1, &descriptorSetWrite, 0, nullptr);

    PipelineInfo pipelineInfo = {pipeline, pipelineLayout, descriptorPool, descriptorSet, image, imageView};

    vulkan_test_draw(root, backend, pipelineInfo);

    // Pool not created with the 'freeable' flag
    // Assert(vkFreeDescriptorSets(backend.device, descriptorPool, 1, &descriptorSet) == VK_SUCCESS);
    vkDestroyDescriptorPool(backend.device, descriptorPool, nullptr);
    vkDestroyPipeline(backend.device, pipeline, nullptr);

    vkDestroyShaderModule(backend.device, computeShader, nullptr);
    vkDestroyPipelineLayout(backend.device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, descriptorSetLayoutOne, nullptr);
}

void vulkan_test(ReaperRoot& root, VulkanBackend& backend)
{
    log_info(root, "test ////////////////////////////////////////");

    GPUTextureProperties properties = DefaultGPUTextureProperties(800, 600, PixelFormat::R16G16B16A16_UNORM);

    VkExtent3D extent = {properties.width, properties.height, properties.depth};

    VkImageTiling     tilingMode = VK_IMAGE_TILING_OPTIMAL;
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                   nullptr,
                                   0,
                                   VK_IMAGE_TYPE_2D,
                                   PixelFormatToVulkan(properties.format),
                                   extent,
                                   properties.mipCount,
                                   properties.layerCount,
                                   SampleCountToVulkan(properties.sampleCount),
                                   tilingMode,
                                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                       | VK_IMAGE_USAGE_STORAGE_BIT,
                                   VK_SHARING_MODE_EXCLUSIVE,
                                   0,
                                   nullptr,
                                   VK_IMAGE_LAYOUT_UNDEFINED};

    log_debug(root, "vulkan: creating new image: extent = {}x{}x{}, format = {}", extent.width, extent.height,
              extent.depth, imageInfo.format);
    log_debug(root, "- mips = {}, layers = {}, samples = {}", imageInfo.mipLevels, imageInfo.arrayLayers,
              imageInfo.samples);

    VkImage image = VK_NULL_HANDLE;
    Assert(vkCreateImage(backend.device, &imageInfo, nullptr, &image) == VK_SUCCESS);

    log_debug(root, "vulkan: created image with handle: {}", static_cast<void*>(image));

    // check mem requirements
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(backend.device, image, &memoryRequirements);

    log_debug(root, "- image memory requirements: size = {}, alignment = {}, types = {:#b}", memoryRequirements.size,
              memoryRequirements.alignment, memoryRequirements.memoryTypeBits);

    VkMemoryPropertyFlags requiredMemoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    uint32_t memoryTypeIndex =
        getMemoryType(backend.physicalDevice, memoryRequirements.memoryTypeBits, requiredMemoryType);

    Assert(memoryTypeIndex < backend.physicalDeviceInfo.memory.memoryTypeCount, "invalid memory type index");

    debug_memory_heap_properties(root, backend, memoryTypeIndex);

    // It's fine here but when allocating from big buffer we need to align correctly the offset.
    VkDeviceSize allocSize = memoryRequirements.size + memoryRequirements.alignment - 1; // ALIGN PROPERLY

    // back with da memory
    VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, allocSize, memoryTypeIndex};

    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    Assert(vkAllocateMemory(backend.device, &memAllocInfo, nullptr, &imageMemory) == VK_SUCCESS);

    log_debug(root, "vulkan: allocated memory with handle: {}", static_cast<void*>(imageMemory));

    // bind resource with memory
    // VkDeviceSize memAlignedOffset = alignOffset(0, memoryRequirements.alignment); // take memreqs into account
    // vkAllocateMemory gives one aligned offset by default
    VkDeviceSize memAlignedOffset = 0;
    Assert(vkBindImageMemory(backend.device, image, imageMemory, memAlignedOffset) == VK_SUCCESS);

    // image view
    VkImageSubresourceRange viewRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0,
                                         VK_REMAINING_ARRAY_LAYERS};

    VkImageViewCreateInfo imageViewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0, // reserved
        image,
        VK_IMAGE_VIEW_TYPE_2D,
        imageInfo.format, // reuse same format
        VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY},
        viewRange};

    VkImageView imageView = VK_NULL_HANDLE;
    Assert(vkCreateImageView(backend.device, &imageViewInfo, nullptr, &imageView) == VK_SUCCESS);

    log_debug(root, "vulkan: created image view with handle: {}", static_cast<void*>(imageView));

    vulkan_test_pipeline(root, backend, image, imageView);

    // cleanup
    vkFreeMemory(backend.device, imageMemory, nullptr);
    vkDestroyImageView(backend.device, imageView, nullptr);
    vkDestroyImage(backend.device, image, nullptr);

    log_info(root, "test ////////////////////////////////////////");
}
} // namespace Reaper
