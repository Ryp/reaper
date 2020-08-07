////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Culling.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/Shader.h"
#include "renderer/vulkan/SwapchainRendererBase.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include <array>

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/shader/share/culling.hlsl"

namespace Reaper
{
namespace
{
    // FIXME
    VkDescriptorBufferInfo default_descriptor_buffer_info(const BufferInfo& bufferInfo)
    {
        return {bufferInfo.buffer, 0, VK_WHOLE_SIZE};
    }

    VkWriteDescriptorSet create_buffer_descriptor_write(VkDescriptorSet descriptorSet, u32 binding,
                                                        VkDescriptorType              descriptorType,
                                                        const VkDescriptorBufferInfo* bufferInfo)
    {
        return {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            descriptorSet,
            binding,
            0,
            1,
            descriptorType,
            nullptr,
            bufferInfo,
            nullptr,
        };
    }
} // namespace

CullPipelineInfo create_cull_pipeline(ReaperRoot& root, VulkanBackend& backend)
{
    std::array<VkDescriptorSetLayoutBinding, 6> descriptorSetLayoutBinding = {
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
        static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout)
           == VK_SUCCESS);

    log_debug(root, "vulkan: created descriptor set layout with handle: {}", static_cast<void*>(descriptorSetLayout));

    const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullPushConstants)};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                     nullptr,
                                                     VK_FLAGS_NONE,
                                                     1,
                                                     &descriptorSetLayout,
                                                     1,
                                                     &cullPushConstantRange};

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    Assert(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

    log_debug(root, "vulkan: created pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

    VkShaderModule        computeShader = VK_NULL_HANDLE;
    const char*           fileName = "./build/shader/cull_triangle_batch.comp.spv";
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

    vkDestroyShaderModule(backend.device, computeShader, nullptr);

    log_debug(root, "vulkan: created compute pipeline with handle: {}", static_cast<void*>(pipeline));

    return CullPipelineInfo{pipeline, pipelineLayout, descriptorSetLayout};
}

// Second uint is for keeping track of total triangles
constexpr u32 IndirectDrawCountSize = 2;
constexpr u32 CullInstanceCountMax = 512;
constexpr u32 MaxIndirectDrawCount = 2000;
constexpr u32 DynamicIndexBufferSize = static_cast<u32>(2000_kiB);

CullResources create_culling_resources(ReaperRoot& root, VulkanBackend& backend)
{
    CullResources resources;

    resources.cullInstanceParamsBuffer = create_buffer(
        root, backend.device, "Culling instance constant buffer",
        DefaultGPUBufferProperties(CullInstanceCountMax, sizeof(CullInstanceParams), GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    resources.indirectDrawCountBuffer =
        create_buffer(root, backend.device, "Indirect draw count buffer",
                      DefaultGPUBufferProperties(IndirectDrawCountSize, sizeof(u32),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.dynamicIndexBuffer =
        create_buffer(root, backend.device, "Culling dynamic index buffer",
                      DefaultGPUBufferProperties(DynamicIndexBufferSize, 1,
                                                 GPUBufferUsage::IndexBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.indirectDrawBuffer =
        create_buffer(root, backend.device, "Indirect draw buffer",
                      DefaultGPUBufferProperties(MaxIndirectDrawCount, sizeof(VkDrawIndexedIndirectCommand),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.compactIndirectDrawBuffer =
        create_buffer(root, backend.device, "Compact indirect draw buffer",
                      DefaultGPUBufferProperties(MaxIndirectDrawCount, sizeof(VkDrawIndexedIndirectCommand),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.compactIndirectDrawCountBuffer = create_buffer(
        root, backend.device, "Compact indirect draw count buffer",
        DefaultGPUBufferProperties(1, sizeof(u32), GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    Assert(MaxIndirectDrawCount < backend.physicalDeviceProperties.limits.maxDrawIndirectCount);

    resources.compactionIndirectDispatchBuffer =
        create_buffer(root, backend.device, "Compact indirect dispatch buffer",
                      DefaultGPUBufferProperties(1, sizeof(VkDispatchIndirectCommand),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    return resources;
}

VkDescriptorSet create_culling_descriptor_sets(ReaperRoot& root, VulkanBackend& backend, CullResources& cull_resources,
                                               VkDescriptorSetLayout layout, VkDescriptorPool descriptor_pool,
                                               BufferInfo& staticIndexBuffer, BufferInfo& vertexBufferPosition,
                                               u32 pass_index)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                          descriptor_pool, 1, &layout};

    VkDescriptorSet cullPassDescriptorSet = VK_NULL_HANDLE;

    Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &cullPassDescriptorSet) == VK_SUCCESS);
    log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(cullPassDescriptorSet));

    const VkDescriptorBufferInfo cullDescIndices = default_descriptor_buffer_info(staticIndexBuffer);
    const VkDescriptorBufferInfo cullDescVertexPositions = default_descriptor_buffer_info(vertexBufferPosition);
    const VkDescriptorBufferInfo cullDescInstanceParams =
        default_descriptor_buffer_info(cull_resources.cullInstanceParamsBuffer);
    const VkDescriptorBufferInfo cullDescIndicesOut = default_descriptor_buffer_info(cull_resources.dynamicIndexBuffer);
    const VkDescriptorBufferInfo cullDescDrawCommandOut =
        default_descriptor_buffer_info(cull_resources.indirectDrawBuffer);
    const VkDescriptorBufferInfo cullDescDrawCountOut =
        default_descriptor_buffer_info(cull_resources.indirectDrawCountBuffer);

    std::array<VkWriteDescriptorSet, 6> cullPassDescriptorSetWrites = {
        create_buffer_descriptor_write(cullPassDescriptorSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &cullDescIndices),
        create_buffer_descriptor_write(cullPassDescriptorSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &cullDescVertexPositions),
        create_buffer_descriptor_write(cullPassDescriptorSet, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &cullDescInstanceParams),
        create_buffer_descriptor_write(cullPassDescriptorSet, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &cullDescIndicesOut),
        create_buffer_descriptor_write(cullPassDescriptorSet, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &cullDescDrawCommandOut),
        create_buffer_descriptor_write(cullPassDescriptorSet, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &cullDescDrawCountOut),
    };

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(cullPassDescriptorSetWrites.size()),
                           cullPassDescriptorSetWrites.data(), 0, nullptr);

    return cullPassDescriptorSet;
}

VkDescriptorSet create_culling_compact_prep_descriptor_sets(ReaperRoot& root, VulkanBackend& backend,
                                                            CullResources& cull_resources, VkDescriptorSetLayout layout,
                                                            VkDescriptorPool descriptor_pool)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                          descriptor_pool, 1, &layout};

    VkDescriptorSet compactPrepPassDescriptorSet = VK_NULL_HANDLE;

    Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &compactPrepPassDescriptorSet)
           == VK_SUCCESS);
    log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(compactPrepPassDescriptorSet));

    const VkDescriptorBufferInfo compactionDescDrawCommandCount =
        default_descriptor_buffer_info(cull_resources.indirectDrawCountBuffer);
    const VkDescriptorBufferInfo compactionDescDispatchCommandOut =
        default_descriptor_buffer_info(cull_resources.compactionIndirectDispatchBuffer);
    const VkDescriptorBufferInfo compactionDescDrawCommandCountOut =
        default_descriptor_buffer_info(cull_resources.compactIndirectDrawCountBuffer);

    std::array<VkWriteDescriptorSet, 3> compactionPassDescriptorSetWrites = {
        create_buffer_descriptor_write(compactPrepPassDescriptorSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &compactionDescDrawCommandCount),
        create_buffer_descriptor_write(compactPrepPassDescriptorSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &compactionDescDispatchCommandOut),
        create_buffer_descriptor_write(compactPrepPassDescriptorSet, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &compactionDescDrawCommandCountOut),
    };

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(compactionPassDescriptorSetWrites.size()),
                           compactionPassDescriptorSetWrites.data(), 0, nullptr);

    return compactPrepPassDescriptorSet;
}

VkDescriptorSet create_culling_compact_descriptor_sets(ReaperRoot& root, VulkanBackend& backend,
                                                       CullResources& cull_resources, VkDescriptorSetLayout layout,
                                                       VkDescriptorPool descriptor_pool)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                          descriptor_pool, 1, &layout};

    VkDescriptorSet compactionPassDescriptorSet = VK_NULL_HANDLE;

    Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &compactionPassDescriptorSet)
           == VK_SUCCESS);
    log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(compactionPassDescriptorSet));

    const VkDescriptorBufferInfo compactionDescCommand =
        default_descriptor_buffer_info(cull_resources.indirectDrawBuffer);
    const VkDescriptorBufferInfo compactionDescCommandCount =
        default_descriptor_buffer_info(cull_resources.indirectDrawCountBuffer);
    const VkDescriptorBufferInfo compactionDescCommandOut =
        default_descriptor_buffer_info(cull_resources.compactIndirectDrawBuffer);
    const VkDescriptorBufferInfo compactionDescCommandCountOut =
        default_descriptor_buffer_info(cull_resources.compactIndirectDrawCountBuffer);

    std::array<VkWriteDescriptorSet, 4> compactionPassDescriptorSetWrites = {
        create_buffer_descriptor_write(compactionPassDescriptorSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &compactionDescCommand),
        create_buffer_descriptor_write(compactionPassDescriptorSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &compactionDescCommandCount),
        create_buffer_descriptor_write(compactionPassDescriptorSet, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &compactionDescCommandOut),
        create_buffer_descriptor_write(compactionPassDescriptorSet, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       &compactionDescCommandCountOut),
    };

    vkUpdateDescriptorSets(backend.device, static_cast<u32>(compactionPassDescriptorSetWrites.size()),
                           compactionPassDescriptorSetWrites.data(), 0, nullptr);

    return compactionPassDescriptorSet;
}

void destroy_culling_resources(VulkanBackend& backend, CullResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.indirectDrawCountBuffer.buffer,
                     resources.indirectDrawCountBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.cullInstanceParamsBuffer.buffer,
                     resources.cullInstanceParamsBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.dynamicIndexBuffer.buffer,
                     resources.dynamicIndexBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.indirectDrawBuffer.buffer,
                     resources.indirectDrawBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.compactIndirectDrawBuffer.buffer,
                     resources.compactIndirectDrawBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.compactionIndirectDispatchBuffer.buffer,
                     resources.compactionIndirectDispatchBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.compactIndirectDrawCountBuffer.buffer,
                     resources.compactIndirectDrawCountBuffer.allocation);
}

void culling_prepare_buffers(const CullOptions& options, VulkanBackend& backend, const PreparedData& prepared,
                             CullResources& resources)
{
    // FIXME
    upload_buffer_data(backend.device, backend.vma_instance, resources.cullInstanceParamsBuffer,
                       prepared.cull_passes.front().cull_instance_params.data(),
                       prepared.cull_passes.front().cull_instance_params.size() * sizeof(CullInstanceParams));

    if (!options.freeze_culling)
    {
        std::array<u32, IndirectDrawCountSize> zero = {};
        upload_buffer_data(backend.device, backend.vma_instance, resources.indirectDrawCountBuffer, zero.data(),
                           zero.size() * sizeof(u32));
    }
}
} // namespace Reaper
