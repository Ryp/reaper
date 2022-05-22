////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Culling.h"

#include "CullingConstants.h"
#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Barrier.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/Debug.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/Shader.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/Profile.h"

#include <array>

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/shader/share/meshlet.hlsl"
#include "renderer/shader/share/meshlet_culling.hlsl"

namespace Reaper
{
namespace
{
    std::vector<VkDescriptorSet> create_descriptor_sets(VulkanBackend& backend, VkDescriptorSetLayout set_layout,
                                                        u32 count)
    {
        std::vector<VkDescriptorSetLayout> layouts(count, set_layout);
        std::vector<VkDescriptorSet>       sets(layouts.size());

        const VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                       backend.global_descriptor_pool, static_cast<u32>(layouts.size()),
                                                       layouts.data()};

        Assert(vkAllocateDescriptorSets(backend.device, &allocInfo, sets.data()) == VK_SUCCESS);

        return sets;
    }

    void update_cull_meshlet_descriptor_sets(VulkanBackend& backend, CullResources& resources,
                                             const BufferInfo& meshletBuffer, u32 pass_index)
    {
        VkDescriptorSet descriptor_set = resources.cull_meshlet_descriptor_sets[pass_index];

        const VkDescriptorBufferInfo meshlets = default_descriptor_buffer_info(meshletBuffer);
        const VkDescriptorBufferInfo instanceParams =
            default_descriptor_buffer_info(resources.cullInstanceParamsBuffer);
        const VkDescriptorBufferInfo counters = get_vk_descriptor_buffer_info(
            resources.countersBuffer, BufferSubresource{pass_index * CountersCount, CountersCount});
        const VkDescriptorBufferInfo meshlet_offsets = get_vk_descriptor_buffer_info(
            resources.dynamicMeshletBuffer,
            BufferSubresource{pass_index * DynamicMeshletBufferElements, DynamicMeshletBufferElements});

        std::vector<VkWriteDescriptorSet> writes = {
            create_buffer_descriptor_write(descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &meshlets),
            create_buffer_descriptor_write(descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &instanceParams),
            create_buffer_descriptor_write(descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &counters),
            create_buffer_descriptor_write(descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &meshlet_offsets),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    void update_culling_descriptor_sets(VulkanBackend& backend, CullResources& resources,
                                        const BufferInfo& staticIndexBuffer, const BufferInfo& vertexBufferPosition,
                                        u32 pass_index)
    {
        VkDescriptorSet descriptor_set = resources.cull_triangles_descriptor_sets[pass_index];

        const VkDescriptorBufferInfo meshlets = get_vk_descriptor_buffer_info(
            resources.dynamicMeshletBuffer,
            BufferSubresource{pass_index * DynamicMeshletBufferElements, DynamicMeshletBufferElements});
        const VkDescriptorBufferInfo indices = default_descriptor_buffer_info(staticIndexBuffer);
        const VkDescriptorBufferInfo vertexPositions = default_descriptor_buffer_info(vertexBufferPosition);
        const VkDescriptorBufferInfo instanceParams =
            default_descriptor_buffer_info(resources.cullInstanceParamsBuffer);
        const VkDescriptorBufferInfo indicesOut = get_vk_descriptor_buffer_info(
            resources.dynamicIndexBuffer,
            BufferSubresource{pass_index * DynamicIndexBufferSizeBytes, DynamicIndexBufferSizeBytes});
        const VkDescriptorBufferInfo drawCommandOut = get_vk_descriptor_buffer_info(
            resources.indirectDrawBuffer, BufferSubresource{pass_index * MaxIndirectDrawCount, MaxIndirectDrawCount});
        const VkDescriptorBufferInfo countersOut = get_vk_descriptor_buffer_info(
            resources.countersBuffer, BufferSubresource{pass_index * CountersCount, CountersCount});

        std::vector<VkWriteDescriptorSet> writes = {
            create_buffer_descriptor_write(descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &meshlets),
            create_buffer_descriptor_write(descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indices),
            create_buffer_descriptor_write(descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &vertexPositions),
            create_buffer_descriptor_write(descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &instanceParams),
            create_buffer_descriptor_write(descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indicesOut),
            create_buffer_descriptor_write(descriptor_set, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &drawCommandOut),
            create_buffer_descriptor_write(descriptor_set, 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &countersOut),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    void update_cull_prepare_indirect_descriptor_set(VulkanBackend& backend, CullResources& resources,
                                                     VkDescriptorSet descriptor_set)
    {
        const VkDescriptorBufferInfo counters = default_descriptor_buffer_info(resources.countersBuffer);
        const VkDescriptorBufferInfo indirectCommands =
            default_descriptor_buffer_info(resources.indirectDispatchBuffer);

        std::vector<VkWriteDescriptorSet> writes = {
            create_buffer_descriptor_write(descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &counters),
            create_buffer_descriptor_write(descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indirectCommands),
        };

        vkUpdateDescriptorSets(backend.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    SimplePipeline create_meshlet_prepare_indirect_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        const char*           fileName = "./build/shader/prepare_fine_culling_indirect.comp.spv";
        const char*           entryPoint = "main";
        VkSpecializationInfo* specialization = nullptr;
        VkShaderModule        computeShader = vulkan_create_shader_module(backend.device, fileName);

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<u32>(descriptorSetLayoutBinding.size()), descriptorSetLayoutBinding.data()};

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        Assert(vkCreateDescriptorSetLayout(backend.device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout)
               == VK_SUCCESS);

        log_debug(root, "vulkan: created descriptor set layout with handle: {}",
                  static_cast<void*>(descriptorSetLayout));

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                           sizeof(CullMeshletPushConstants)};

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

        return SimplePipeline{pipeline, pipelineLayout, descriptorSetLayout};
    }

    SimplePipeline create_cull_meshlet_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        const char*           fileName = "./build/shader/cull_meshlet.comp.spv";
        const char*           entryPoint = "main";
        VkSpecializationInfo* specialization = nullptr;
        VkShaderModule        computeShader = vulkan_create_shader_module(backend.device, fileName);

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
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

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                           sizeof(CullMeshletPushConstants)};

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

        return SimplePipeline{pipeline, pipelineLayout, descriptorSetLayout};
    }

    SimplePipeline create_cull_triangles_pipeline(ReaperRoot& root, VulkanBackend& backend)
    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
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

        const char*           fileName = "./build/shader/cull_triangle_batch.comp.spv";
        const char*           entryPoint = "main";
        VkSpecializationInfo* specialization = nullptr;
        VkShaderModule        computeShader = vulkan_create_shader_module(backend.device, fileName);

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

        return SimplePipeline{pipeline, pipelineLayout, descriptorSetLayout};
    }
} // namespace

CullResources create_culling_resources(ReaperRoot& root, VulkanBackend& backend)
{
    CullResources resources;

    resources.cullMeshletPipe = create_cull_meshlet_pipeline(root, backend);
    resources.cullMeshletPrepIndirect = create_meshlet_prepare_indirect_pipeline(root, backend);
    resources.cullTrianglesPipe = create_cull_triangles_pipeline(root, backend);

    resources.cullInstanceParamsBuffer = create_buffer(
        root, backend.device, "Culling instance constants",
        DefaultGPUBufferProperties(CullInstanceCountMax, sizeof(CullMeshInstanceParams), GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    resources.countersBuffer =
        create_buffer(root, backend.device, "Meshlet counters",
                      DefaultGPUBufferProperties(CountersCount * MaxCullPassCount, sizeof(u32),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::TransferDst
                                                     | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.dynamicMeshletBuffer =
        create_buffer(root, backend.device, "Dynamic meshlet offsets",
                      DefaultGPUBufferProperties(DynamicMeshletBufferElements * MaxCullPassCount,
                                                 sizeof(MeshletOffsets), GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.indirectDispatchBuffer =
        create_buffer(root, backend.device, "Indirect dispatch buffer",
                      DefaultGPUBufferProperties(MaxCullPassCount, sizeof(VkDispatchIndirectCommand),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.dynamicIndexBuffer =
        create_buffer(root, backend.device, "Dynamic indices",
                      DefaultGPUBufferProperties(DynamicIndexBufferSizeBytes * MaxCullPassCount, 1,
                                                 GPUBufferUsage::IndexBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.indirectDrawBuffer = create_buffer(
        root, backend.device, "Indirect draw buffer",
        DefaultGPUBufferProperties(MaxIndirectDrawCount * MaxCullPassCount, sizeof(VkDrawIndexedIndirectCommand),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    Assert(MaxIndirectDrawCount < backend.physicalDeviceProperties.limits.maxDrawIndirectCount);

    // FIXME
    resources.cull_meshlet_descriptor_sets =
        create_descriptor_sets(backend, resources.cullMeshletPipe.descSetLayout, 4);
    resources.cull_prepare_descriptor_set =
        create_descriptor_sets(backend, resources.cullMeshletPrepIndirect.descSetLayout, 1).front();
    resources.cull_triangles_descriptor_sets =
        create_descriptor_sets(backend, resources.cullTrianglesPipe.descSetLayout, 4);

    return resources;
}

namespace
{
    void destroy_simple_pipeline(VkDevice device, SimplePipeline simple_pipeline)
    {
        vkDestroyPipeline(device, simple_pipeline.pipeline, nullptr);
        vkDestroyPipelineLayout(device, simple_pipeline.pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, simple_pipeline.descSetLayout, nullptr);
    }
} // namespace

void destroy_culling_resources(VulkanBackend& backend, CullResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.countersBuffer.handle, resources.countersBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.cullInstanceParamsBuffer.handle,
                     resources.cullInstanceParamsBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.dynamicIndexBuffer.handle,
                     resources.dynamicIndexBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.indirectDispatchBuffer.handle,
                     resources.indirectDispatchBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.dynamicMeshletBuffer.handle,
                     resources.dynamicMeshletBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, resources.indirectDrawBuffer.handle,
                     resources.indirectDrawBuffer.allocation);

    destroy_simple_pipeline(backend.device, resources.cullMeshletPipe);
    destroy_simple_pipeline(backend.device, resources.cullMeshletPrepIndirect);
    destroy_simple_pipeline(backend.device, resources.cullTrianglesPipe);
}

void upload_culling_resources(VulkanBackend& backend, const PreparedData& prepared, CullResources& resources)
{
    if (prepared.cull_mesh_instance_params.empty())
        return;

    upload_buffer_data(backend.device, backend.vma_instance, resources.cullInstanceParamsBuffer,
                       prepared.cull_mesh_instance_params.data(),
                       prepared.cull_mesh_instance_params.size() * sizeof(CullMeshInstanceParams));
}

void update_culling_pass_descriptor_sets(VulkanBackend& backend, const PreparedData& prepared, CullResources& resources,
                                         const MeshCache& mesh_cache)
{
    for (const CullPassData& cull_pass : prepared.cull_passes)
    {
        Assert(cull_pass.pass_index < MaxCullPassCount);
        update_cull_meshlet_descriptor_sets(backend, resources, mesh_cache.meshletBuffer, cull_pass.pass_index);
        update_culling_descriptor_sets(backend, resources, mesh_cache.indexBuffer, mesh_cache.vertexBufferPosition,
                                       cull_pass.pass_index);
    }

    update_cull_prepare_indirect_descriptor_set(backend, resources, resources.cull_prepare_descriptor_set);
}

void record_culling_command_buffer(CommandBuffer& cmdBuffer, const PreparedData& prepared, CullResources& resources)
{
    REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Culling Pass", MP_DARKGOLDENROD);

    const u32 clear_value = 0;
    vkCmdFillBuffer(cmdBuffer.handle, resources.countersBuffer.handle, 0, VK_WHOLE_SIZE, clear_value);

    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        const GPUMemoryAccess  src = {VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
        const GPUMemoryAccess  dst = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                     VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT};
        const VkMemoryBarrier2 memoryBarrier = get_vk_memory_barrier(src, dst);
        const VkDependencyInfo dependencies = get_vk_memory_barrier_depency_info(1, &memoryBarrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Cull Meshes");

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.cullMeshletPipe.pipeline);

        for (const CullPassData& cull_pass : prepared.cull_passes)
        {
            vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    resources.cullMeshletPipe.pipelineLayout, 0, 1,
                                    &resources.cull_meshlet_descriptor_sets[cull_pass.pass_index], 0, nullptr);

            for (const CullCmd& command : cull_pass.cull_commands)
            {
                vkCmdPushConstants(cmdBuffer.handle, resources.cullMeshletPipe.pipelineLayout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(command.push_constants),
                                   &command.push_constants);

                const u32 group_count_x = div_round_up(command.push_constants.meshlet_count, MeshletCullThreadCount);
                vkCmdDispatch(cmdBuffer.handle, group_count_x, command.instance_count, 1);
            }
        }
    }

    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        const GPUMemoryAccess  src = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        const GPUMemoryAccess  dst = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        const VkMemoryBarrier2 memoryBarrier = get_vk_memory_barrier(src, dst);
        const VkDependencyInfo dependencies = get_vk_memory_barrier_depency_info(1, &memoryBarrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Indirect Prepare");

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.cullMeshletPrepIndirect.pipeline);

        vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                resources.cullMeshletPrepIndirect.pipelineLayout, 0, 1,
                                &resources.cull_prepare_descriptor_set, 0, nullptr);

        const u32 group_count_x = div_round_up(prepared.cull_passes.size(), PrepareIndirectDispatchThreadCount);
        vkCmdDispatch(cmdBuffer.handle, group_count_x, 1, 1);
    }

    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        const GPUMemoryAccess  src = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        const GPUMemoryAccess  dst = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        const VkMemoryBarrier2 memoryBarrier = get_vk_memory_barrier(src, dst);
        const VkDependencyInfo dependencies = get_vk_memory_barrier_depency_info(1, &memoryBarrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    {
        VulkanDebugLabelCmdBufferScope s(cmdBuffer.handle, "Cull Triangles");

        vkCmdBindPipeline(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, resources.cullTrianglesPipe.pipeline);

        for (const CullPassData& cull_pass : prepared.cull_passes)
        {
            vkCmdBindDescriptorSets(cmdBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    resources.cullTrianglesPipe.pipelineLayout, 0, 1,
                                    &resources.cull_triangles_descriptor_sets[cull_pass.pass_index], 0, nullptr);

            CullPushConstants consts;
            consts.output_size_ts = cull_pass.output_size_ts;

            vkCmdPushConstants(cmdBuffer.handle, resources.cullTrianglesPipe.pipelineLayout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(consts), &consts);

            vkCmdDispatchIndirect(cmdBuffer.handle, resources.indirectDispatchBuffer.handle,
                                  cull_pass.pass_index * sizeof(VkDispatchIndirectCommand));
        }
    }

    {
        REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "Barrier", MP_RED);

        const GPUMemoryAccess  src = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        const GPUMemoryAccess  dst = {VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                                     VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT};
        const VkMemoryBarrier2 memoryBarrier = get_vk_memory_barrier(src, dst);
        const VkDependencyInfo dependencies = get_vk_memory_barrier_depency_info(1, &memoryBarrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }
}

namespace
{
    constexpr VkIndexType get_vk_culling_index_type()
    {
        if constexpr (IndexSizeBytes == 2)
            return VK_INDEX_TYPE_UINT16;
        else
        {
            static_assert(IndexSizeBytes == 4, "Invalid index size");
            return VK_INDEX_TYPE_UINT32;
        }
    }
} // namespace

CullingDrawParams get_culling_draw_params(u32 pass_index)
{
    CullingDrawParams params;
    params.counter_buffer_offset = (pass_index * CountersCount + DrawCommandCounterOffset) * sizeof(u32);
    params.index_buffer_offset = pass_index * DynamicIndexBufferSizeBytes;
    params.index_type = get_vk_culling_index_type();
    params.command_buffer_offset = pass_index * MaxIndirectDrawCount * sizeof(VkDrawIndexedIndirectCommand);
    params.command_buffer_max_count = MaxIndirectDrawCount;

    return params;
}

} // namespace Reaper
