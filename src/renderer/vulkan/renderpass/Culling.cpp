////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Culling.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/ComputeHelper.h"
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
    void cmd_insert_compute_to_compute_barrier(VkCommandBuffer cmdBuffer)
    {
        std::array<VkMemoryBarrier, 1> memoryBarriers = {VkMemoryBarrier{
            VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
        }};

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             static_cast<u32>(memoryBarriers.size()), memoryBarriers.data(), 0, nullptr, 0, nullptr);
    }

    VkDescriptorSet create_culling_descriptor_sets(ReaperRoot& root, VulkanBackend& backend,
                                                   CullResources& cull_resources, VkDescriptorSetLayout layout,
                                                   BufferInfo& staticIndexBuffer, BufferInfo& vertexBufferPosition,
                                                   u32 pass_index)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              backend.frame_descriptor_pool, 1, &layout};

        VkDescriptorSet cullPassDescriptorSet = VK_NULL_HANDLE;

        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &cullPassDescriptorSet) == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}", static_cast<void*>(cullPassDescriptorSet));

        const VkDescriptorBufferInfo cullDescPassParams =
            get_vk_descriptor_buffer_info(cull_resources.cullPassConstantBuffer, GPUBufferView{pass_index, 1});
        const VkDescriptorBufferInfo cullDescIndices = default_descriptor_buffer_info(staticIndexBuffer);
        const VkDescriptorBufferInfo cullDescVertexPositions = default_descriptor_buffer_info(vertexBufferPosition);
        const VkDescriptorBufferInfo cullDescInstanceParams =
            default_descriptor_buffer_info(cull_resources.cullInstanceParamsBuffer);
        const VkDescriptorBufferInfo cullDescIndicesOut =
            get_vk_descriptor_buffer_info(cull_resources.dynamicIndexBuffer,
                                          GPUBufferView{pass_index * DynamicIndexBufferSize, DynamicIndexBufferSize});
        const VkDescriptorBufferInfo cullDescDrawCommandOut = get_vk_descriptor_buffer_info(
            cull_resources.indirectDrawBuffer, GPUBufferView{pass_index * MaxIndirectDrawCount, MaxIndirectDrawCount});
        const VkDescriptorBufferInfo cullDescDrawCountOut =
            get_vk_descriptor_buffer_info(cull_resources.indirectDrawCountBuffer,
                                          GPUBufferView{pass_index * IndirectDrawCountCount, IndirectDrawCountCount});

        std::array<VkWriteDescriptorSet, 7> cullPassDescriptorSetWrites = {
            create_buffer_descriptor_write(cullPassDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           &cullDescPassParams),
            create_buffer_descriptor_write(cullPassDescriptorSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &cullDescIndices),
            create_buffer_descriptor_write(cullPassDescriptorSet, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &cullDescVertexPositions),
            create_buffer_descriptor_write(cullPassDescriptorSet, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &cullDescInstanceParams),
            create_buffer_descriptor_write(cullPassDescriptorSet, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &cullDescIndicesOut),
            create_buffer_descriptor_write(cullPassDescriptorSet, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &cullDescDrawCommandOut),
            create_buffer_descriptor_write(cullPassDescriptorSet, 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           &cullDescDrawCountOut),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(cullPassDescriptorSetWrites.size()),
                               cullPassDescriptorSetWrites.data(), 0, nullptr);

        return cullPassDescriptorSet;
    }

    VkDescriptorSet create_culling_compact_prep_descriptor_sets(ReaperRoot& root, VulkanBackend& backend,
                                                                CullResources&        cull_resources,
                                                                VkDescriptorSetLayout layout, u32 pass_index)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              backend.frame_descriptor_pool, 1, &layout};

        VkDescriptorSet compactPrepPassDescriptorSet = VK_NULL_HANDLE;

        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &compactPrepPassDescriptorSet)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}",
                  static_cast<void*>(compactPrepPassDescriptorSet));

        const VkDescriptorBufferInfo compactionDescDrawCommandCount =
            get_vk_descriptor_buffer_info(cull_resources.indirectDrawCountBuffer,
                                          GPUBufferView{pass_index * IndirectDrawCountCount, IndirectDrawCountCount});
        const VkDescriptorBufferInfo compactionDescDispatchCommandOut = get_vk_descriptor_buffer_info(
            cull_resources.compactionIndirectDispatchBuffer, GPUBufferView{pass_index, 1});
        const VkDescriptorBufferInfo compactionDescDrawCommandCountOut =
            get_vk_descriptor_buffer_info(cull_resources.compactIndirectDrawCountBuffer, GPUBufferView{pass_index, 1});

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
                                                           u32 pass_index)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                              backend.frame_descriptor_pool, 1, &layout};

        VkDescriptorSet compactionPassDescriptorSet = VK_NULL_HANDLE;

        Assert(vkAllocateDescriptorSets(backend.device, &descriptorSetAllocInfo, &compactionPassDescriptorSet)
               == VK_SUCCESS);
        log_debug(root, "vulkan: created descriptor set with handle: {}",
                  static_cast<void*>(compactionPassDescriptorSet));

        const VkDescriptorBufferInfo compactionDescCommand = get_vk_descriptor_buffer_info(
            cull_resources.indirectDrawBuffer, GPUBufferView{pass_index * MaxIndirectDrawCount, MaxIndirectDrawCount});
        const VkDescriptorBufferInfo compactionDescCommandCount =
            get_vk_descriptor_buffer_info(cull_resources.indirectDrawCountBuffer,
                                          GPUBufferView{pass_index * IndirectDrawCountCount, IndirectDrawCountCount});
        const VkDescriptorBufferInfo compactionDescCommandOut =
            get_vk_descriptor_buffer_info(cull_resources.compactIndirectDrawBuffer,
                                          GPUBufferView{pass_index * MaxIndirectDrawCount, MaxIndirectDrawCount});
        const VkDescriptorBufferInfo compactionDescCommandCountOut =
            get_vk_descriptor_buffer_info(cull_resources.compactIndirectDrawCountBuffer, GPUBufferView{pass_index, 1});

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

    CullPipelineInfo create_cull_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        std::array<VkDescriptorSetLayoutBinding, 7> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayout));

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

        Assert(vkCreateComputePipelines(backend.device, cache, 1, &pipelineCreateInfo, nullptr, &pipeline)
               == VK_SUCCESS);

        vkDestroyShaderModule(backend.device, computeShader, nullptr);

        log_debug(root, "vulkan: created compute pipeline with handle: {}", static_cast<void*>(pipeline));

        return CullPipelineInfo{pipeline, pipelineLayout, descriptorSetLayout};
    }

    CompactionPrepPipelineInfo create_compaction_prep_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        std::array<VkDescriptorSetLayoutBinding, 3> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, VK_FLAGS_NONE, 1, &descriptorSetLayout, 0, nullptr};

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Assert(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

        log_debug(root, "vulkan: created pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

        VkShaderModule        computeShader = VK_NULL_HANDLE;
        const char*           fileName = "./build/shader/compaction_prepare_indirect_dispatch.comp.spv";
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

        Assert(vkCreateComputePipelines(backend.device, cache, 1, &pipelineCreateInfo, nullptr, &pipeline)
               == VK_SUCCESS);

        vkDestroyShaderModule(backend.device, computeShader, nullptr);

        log_debug(root, "vulkan: created compute pipeline with handle: {}", static_cast<void*>(pipeline));

        return CompactionPrepPipelineInfo{pipeline, pipelineLayout, descriptorSetLayout};
    }

    CompactionPipelineInfo create_compaction_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        std::array<VkDescriptorSetLayoutBinding, 4> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, VK_FLAGS_NONE, 1, &descriptorSetLayout, 0, nullptr};

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Assert(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

        log_debug(root, "vulkan: created pipeline layout with handle: {}", static_cast<void*>(pipelineLayout));

        VkShaderModule        computeShader = VK_NULL_HANDLE;
        const char*           fileName = "./build/shader/draw_command_compaction.comp.spv";
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

        Assert(vkCreateComputePipelines(backend.device, cache, 1, &pipelineCreateInfo, nullptr, &pipeline)
               == VK_SUCCESS);

        vkDestroyShaderModule(backend.device, computeShader, nullptr);

        log_debug(root, "vulkan: created compute pipeline with handle: {}", static_cast<void*>(pipeline));

        return CompactionPipelineInfo{pipeline, pipelineLayout, descriptorSetLayout};
    }
} // namespace

CullResources create_culling_resources(ReaperRoot& root, VulkanBackend& backend)
{
    CullResources resources;

    resources.cullPipe = create_cull_pipeline(root, backend);
    resources.compactPrepPipe = create_compaction_prep_pipeline(root, backend);
    resources.compactionPipe = create_compaction_pipeline(root, backend);

    resources.cullPassConstantBuffer = create_buffer(
        root, backend.device, "Cull Pass Constant buffer",
        DefaultGPUBufferProperties(MaxCullPassCount, sizeof(CullPassParams), GPUBufferUsage::UniformBuffer),
        backend.vma_instance);

    resources.cullInstanceParamsBuffer = create_buffer(
        root, backend.device, "Culling instance constant buffer",
        DefaultGPUBufferProperties(CullInstanceCountMax, sizeof(CullMeshInstanceParams), GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    resources.indirectDrawCountBuffer =
        create_buffer(root, backend.device, "Indirect draw count buffer",
                      DefaultGPUBufferProperties(IndirectDrawCountCount * MaxCullPassCount, sizeof(u32),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.dynamicIndexBuffer =
        create_buffer(root, backend.device, "Culling dynamic index buffer",
                      DefaultGPUBufferProperties(DynamicIndexBufferSize * MaxCullPassCount, 1,
                                                 GPUBufferUsage::IndexBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.indirectDrawBuffer = create_buffer(
        root, backend.device, "Indirect draw buffer",
        DefaultGPUBufferProperties(MaxIndirectDrawCount * MaxCullPassCount, sizeof(VkDrawIndexedIndirectCommand),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    resources.compactIndirectDrawBuffer = create_buffer(
        root, backend.device, "Compact indirect draw buffer",
        DefaultGPUBufferProperties(MaxIndirectDrawCount * MaxCullPassCount, sizeof(VkDrawIndexedIndirectCommand),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    resources.compactIndirectDrawCountBuffer =
        create_buffer(root, backend.device, "Compact indirect draw count buffer",
                      DefaultGPUBufferProperties(1 * MaxCullPassCount, sizeof(u32),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    Assert(MaxIndirectDrawCount < backend.physicalDeviceProperties.limits.maxDrawIndirectCount);

    resources.compactionIndirectDispatchBuffer =
        create_buffer(root, backend.device, "Compact indirect dispatch buffer",
                      DefaultGPUBufferProperties(1 * MaxCullPassCount, sizeof(VkDispatchIndirectCommand),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    return resources;
}

CullPassResources create_culling_pass_descriptor_sets(ReaperRoot& root, VulkanBackend& backend,
                                                      CullResources& resources, u32 pass_index, BufferInfo& indexBuffer,
                                                      BufferInfo& vertexBufferPosition)
{
    Assert(pass_index < MaxCullPassCount);

    return {create_culling_descriptor_sets(root, backend, resources, resources.cullPipe.descSetLayout, indexBuffer,
                                           vertexBufferPosition, pass_index),
            create_culling_compact_prep_descriptor_sets(root, backend, resources,
                                                        resources.compactPrepPipe.descSetLayout, pass_index),
            create_culling_compact_descriptor_sets(root, backend, resources, resources.compactionPipe.descSetLayout,
                                                   pass_index)};
}

void destroy_culling_resources(VulkanBackend& backend, CullResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.cullPassConstantBuffer.buffer,
                     resources.cullPassConstantBuffer.allocation);
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

    vkDestroyPipeline(backend.device, resources.cullPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.cullPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.cullPipe.descSetLayout, nullptr);

    vkDestroyPipeline(backend.device, resources.compactPrepPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.compactPrepPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.compactPrepPipe.descSetLayout, nullptr);

    vkDestroyPipeline(backend.device, resources.compactionPipe.pipeline, nullptr);
    vkDestroyPipelineLayout(backend.device, resources.compactionPipe.pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(backend.device, resources.compactionPipe.descSetLayout, nullptr);
}

void culling_prepare_buffers(const CullOptions& options, VulkanBackend& backend, const PreparedData& prepared,
                             CullResources& resources)
{
    upload_buffer_data(backend.device, backend.vma_instance, resources.cullPassConstantBuffer,
                       prepared.cull_pass_params.data(), prepared.cull_pass_params.size() * sizeof(CullPassParams));

    upload_buffer_data(backend.device, backend.vma_instance, resources.cullInstanceParamsBuffer,
                       prepared.cull_mesh_instance_params.data(),
                       prepared.cull_mesh_instance_params.size() * sizeof(CullMeshInstanceParams));

    if (!options.freeze_culling)
    {
        std::array<u32, IndirectDrawCountCount* MaxCullPassCount> zero = {};
        upload_buffer_data(backend.device, backend.vma_instance, resources.indirectDrawCountBuffer, zero.data(),
                           zero.size() * sizeof(u32));
    }
}

void record_culling_command_buffer(const CullOptions& options, VkCommandBuffer cmdBuffer, const PreparedData& prepared,
                                   CullResources& resources)
{
    // REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Culling Pass", MP_DARKGOLDENROD);

    if (!options.freeze_culling)
    {
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, resources.cullPipe.pipeline);

        for (const CullPassData& cull_pass : prepared.cull_passes)
        {
            const CullPassResources& pass_resources = resources.passes[cull_pass.pass_index];

            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, resources.cullPipe.pipelineLayout, 0, 1,
                                    &pass_resources.cull_descriptor_set, 0, nullptr);

            for (const CullCmd& command : cull_pass.cull_cmds)
            {
                vkCmdPushConstants(cmdBuffer, resources.cullPipe.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   sizeof(command.push_constants), &command.push_constants);

                vkCmdDispatch(cmdBuffer, div_round_up(command.push_constants.triangleCount, ComputeCullingGroupSize),
                              command.instanceCount, 1);
            }
        }
    }

    {
        // REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Barrier", MP_RED);
        cmd_insert_compute_to_compute_barrier(cmdBuffer);
    }

    // Compaction prepare pass
    {
        // REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Cull Compaction Prepare", MP_DARKGOLDENROD);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, resources.compactPrepPipe.pipeline);

        for (const CullPassData& cull_pass : prepared.cull_passes)
        {
            const CullPassResources& pass_resources = resources.passes[cull_pass.pass_index];

            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, resources.compactPrepPipe.pipelineLayout,
                                    0, 1, &pass_resources.compact_prep_descriptor_set, 0, nullptr);

            vkCmdDispatch(cmdBuffer, 1, 1, 1);
        }
    }

    {
        // REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Barrier", MP_RED);
        cmd_insert_compute_to_compute_barrier(cmdBuffer);
    }

    // Compaction pass
    {
        // REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Cull Compaction", MP_DARKGOLDENROD);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, resources.compactionPipe.pipeline);

        for (const CullPassData& cull_pass : prepared.cull_passes)
        {
            const CullPassResources& pass_resources = resources.passes[cull_pass.pass_index];

            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, resources.compactionPipe.pipelineLayout,
                                    0, 1, &pass_resources.compact_descriptor_set, 0, nullptr);

            vkCmdDispatchIndirect(cmdBuffer, resources.compactionIndirectDispatchBuffer.buffer, 0);
        }
    }

    {
        // REAPER_PROFILE_SCOPE_GPU(pGpuLog, "Barrier", MP_RED);

        std::array<VkMemoryBarrier, 1> memoryBarriers = {VkMemoryBarrier{
            VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        }};

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0,
                             static_cast<u32>(memoryBarriers.size()), memoryBarriers.data(), 0, nullptr, 0, nullptr);
    }
}
} // namespace Reaper
