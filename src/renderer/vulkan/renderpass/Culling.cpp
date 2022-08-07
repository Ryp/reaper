////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Culling.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Barrier.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/ComputeHelper.h"
#include "renderer/vulkan/Debug.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/Pipeline.h"
#include "renderer/vulkan/ShaderModules.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/shader/share/meshlet.hlsl"
#include "renderer/shader/share/meshlet_culling.hlsl"

namespace Reaper
{
constexpr u32 IndexSizeBytes = 4;
constexpr u32 MaxCullPassCount = 4;
constexpr u32 MaxCullInstanceCount = 512 * MaxCullPassCount;
constexpr u32 MaxSurvivingMeshletsPerPass = 200;

// Worst case if all meshlets of all passes aren't culled.
// This shouldn't happen, we can probably cut this by half and raise a warning when we cross the limit.
constexpr u64 DynamicIndexBufferSizeBytes =
    MaxSurvivingMeshletsPerPass * MaxCullPassCount * MeshletMaxTriangleCount * 3 * IndexSizeBytes;
constexpr u32 MaxIndirectDrawCountPerPass = MaxSurvivingMeshletsPerPass;

CullResources create_culling_resources(ReaperRoot& root, VulkanBackend& backend, const ShaderModules& shader_modules)
{
    CullResources resources;

    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptorSetLayout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                           sizeof(CullMeshletPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&descriptorSetLayout, 1),
                                                                 nonstd::span(&cullPushConstantRange, 1));

        VkPipeline pipeline = create_compute_pipeline(backend.device, pipelineLayout, shader_modules.cull_meshlet_cs);

        resources.cullMeshletPipe = SimplePipeline{pipeline, pipelineLayout, descriptorSetLayout};
    }

    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptorSetLayout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                           sizeof(CullMeshletPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&descriptorSetLayout, 1),
                                                                 nonstd::span(&cullPushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipelineLayout, shader_modules.prepare_fine_culling_indirect_cs);

        resources.cullMeshletPrepIndirect = SimplePipeline{pipeline, pipelineLayout, descriptorSetLayout};
    }

    {
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };

        VkDescriptorSetLayout descriptorSetLayout =
            create_descriptor_set_layout(backend.device, descriptorSetLayoutBinding);

        const VkPushConstantRange cullPushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullPushConstants)};

        VkPipelineLayout pipelineLayout = create_pipeline_layout(backend.device, nonstd::span(&descriptorSetLayout, 1),
                                                                 nonstd::span(&cullPushConstantRange, 1));

        VkPipeline pipeline =
            create_compute_pipeline(backend.device, pipelineLayout, shader_modules.cull_triangle_batch_cs);

        resources.cullTrianglesPipe = SimplePipeline{pipeline, pipelineLayout, descriptorSetLayout};
    }

    resources.cullInstanceParamsBuffer = create_buffer(
        root, backend.device, "Culling instance constants",
        DefaultGPUBufferProperties(MaxCullInstanceCount, sizeof(CullMeshInstanceParams), GPUBufferUsage::StorageBuffer),
        backend.vma_instance, MemUsage::CPU_To_GPU);

    resources.countersBuffer =
        create_buffer(root, backend.device, "Meshlet counters",
                      DefaultGPUBufferProperties(CountersCount * MaxCullPassCount, sizeof(u32),
                                                 GPUBufferUsage::IndirectBuffer | GPUBufferUsage::TransferSrc
                                                     | GPUBufferUsage::TransferDst | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);

    resources.countersBufferCPU = create_buffer(
        root, backend.device, "Meshlet counters CPU",
        DefaultGPUBufferProperties(CountersCount * MaxCullPassCount, sizeof(u32), GPUBufferUsage::TransferDst),
        backend.vma_instance, MemUsage::CPU_Only);

    resources.dynamicMeshletBuffer =
        create_buffer(root, backend.device, "Dynamic meshlet offsets",
                      DefaultGPUBufferProperties(MaxSurvivingMeshletsPerPass * MaxCullPassCount, sizeof(MeshletOffsets),
                                                 GPUBufferUsage::StorageBuffer),
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
        DefaultGPUBufferProperties(MaxIndirectDrawCountPerPass * MaxCullPassCount, sizeof(VkDrawIndexedIndirectCommand),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    Assert(MaxIndirectDrawCountPerPass < backend.physicalDeviceProperties.limits.maxDrawIndirectCount);

    resources.cull_meshlet_descriptor_sets.resize(4);
    resources.cull_triangles_descriptor_sets.resize(4);

    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, resources.cullMeshletPipe.descSetLayout,
                             resources.cull_meshlet_descriptor_sets);
    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool,
                             resources.cullMeshletPrepIndirect.descSetLayout,
                             nonstd::span(&resources.cull_prepare_descriptor_set, 1));
    allocate_descriptor_sets(backend.device, backend.global_descriptor_pool, resources.cullTrianglesPipe.descSetLayout,
                             resources.cull_triangles_descriptor_sets);

    const VkEventCreateInfo event_info = {
        VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
        nullptr,
        VK_FLAGS_NONE,
    };
    Assert(vkCreateEvent(backend.device, &event_info, nullptr, &resources.countersReadyEvent) == VK_SUCCESS);
    VulkanSetDebugName(backend.device, resources.countersReadyEvent, "Counters ready event");

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
    vmaDestroyBuffer(backend.vma_instance, resources.countersBufferCPU.handle, resources.countersBufferCPU.allocation);
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

    vkDestroyEvent(backend.device, resources.countersReadyEvent, nullptr);
}

void upload_culling_resources(VulkanBackend& backend, const PreparedData& prepared, CullResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.cull_mesh_instance_params.empty())
        return;

    upload_buffer_data(backend.device, backend.vma_instance, resources.cullInstanceParamsBuffer,
                       prepared.cull_mesh_instance_params.data(),
                       prepared.cull_mesh_instance_params.size() * sizeof(CullMeshInstanceParams));
}

void update_culling_pass_descriptor_sets(DescriptorWriteHelper& write_helper, const PreparedData& prepared,
                                         CullResources& resources, const MeshCache& mesh_cache)
{
    append_write(write_helper, resources.cull_prepare_descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 resources.countersBuffer.handle);
    append_write(write_helper, resources.cull_prepare_descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 resources.indirectDispatchBuffer.handle);

    for (const CullPassData& cull_pass : prepared.cull_passes)
    {
        const u32 pass_index = cull_pass.pass_index;
        Assert(pass_index < MaxCullPassCount);

        const GPUBufferView counter_buffer_view = get_buffer_view(
            resources.countersBuffer.properties, BufferSubresource{pass_index * CountersCount, CountersCount});
        const GPUBufferView dynamic_meshlet_view =
            get_buffer_view(resources.dynamicMeshletBuffer.properties,
                            BufferSubresource{pass_index * MaxSurvivingMeshletsPerPass, MaxSurvivingMeshletsPerPass});

        {
            VkDescriptorSet descriptor_set = resources.cull_meshlet_descriptor_sets[pass_index];
            append_write(write_helper, descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         mesh_cache.meshletBuffer.handle);
            append_write(write_helper, descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         resources.cullInstanceParamsBuffer.handle);
            append_write(write_helper, descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         resources.countersBuffer.handle, counter_buffer_view.offset_bytes,
                         counter_buffer_view.size_bytes);
            append_write(write_helper, descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         resources.dynamicMeshletBuffer.handle, dynamic_meshlet_view.offset_bytes,
                         dynamic_meshlet_view.size_bytes);
        }

        {
            const GPUBufferView dynamic_index_view = get_buffer_view(
                resources.dynamicIndexBuffer.properties,
                BufferSubresource{pass_index * DynamicIndexBufferSizeBytes, DynamicIndexBufferSizeBytes});
            const GPUBufferView indirect_draw_view = get_buffer_view(
                resources.indirectDrawBuffer.properties,
                BufferSubresource{pass_index * MaxIndirectDrawCountPerPass, MaxIndirectDrawCountPerPass});

            VkDescriptorSet descriptor_set = resources.cull_triangles_descriptor_sets[pass_index];
            append_write(write_helper, descriptor_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         resources.dynamicMeshletBuffer.handle, dynamic_meshlet_view.offset_bytes,
                         dynamic_meshlet_view.size_bytes);
            append_write(write_helper, descriptor_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         mesh_cache.indexBuffer.handle);
            append_write(write_helper, descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         mesh_cache.vertexBufferPosition.handle);
            append_write(write_helper, descriptor_set, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         resources.cullInstanceParamsBuffer.handle);
            append_write(write_helper, descriptor_set, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         resources.dynamicIndexBuffer.handle, dynamic_index_view.offset_bytes,
                         dynamic_index_view.size_bytes);
            append_write(write_helper, descriptor_set, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         resources.indirectDrawBuffer.handle, indirect_draw_view.offset_bytes,
                         indirect_draw_view.size_bytes);
            append_write(write_helper, descriptor_set, 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         resources.countersBuffer.handle, counter_buffer_view.offset_bytes,
                         counter_buffer_view.size_bytes);
        }
    }
}

void record_culling_command_buffer(ReaperRoot& root, CommandBuffer& cmdBuffer, const PreparedData& prepared,
                                   CullResources& resources)
{
    u64              total_meshlet_count = 0;
    std::vector<u64> meshlet_count_per_pass;

    const u32 clear_value = 0;
    vkCmdFillBuffer(cmdBuffer.handle, resources.countersBuffer.handle, 0, VK_WHOLE_SIZE, clear_value);

    {
        REAPER_GPU_SCOPE_COLOR(cmdBuffer, "Barrier", MP_RED);

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
            u64& pass_meshlet_count = meshlet_count_per_pass.emplace_back();
            pass_meshlet_count = 0;

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

                pass_meshlet_count += command.push_constants.meshlet_count;
            }

            total_meshlet_count += pass_meshlet_count;
        }
    }

    {
        REAPER_GPU_SCOPE_COLOR(cmdBuffer, "Barrier", MP_RED);

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
        REAPER_GPU_SCOPE_COLOR(cmdBuffer, "Barrier", MP_RED);

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
        REAPER_GPU_SCOPE_COLOR(cmdBuffer, "Barrier", MP_RED);

        const GPUMemoryAccess  src = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        const GPUMemoryAccess  dst = {VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                                      VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT};
        const VkMemoryBarrier2 memoryBarrier = get_vk_memory_barrier(src, dst);
        const VkDependencyInfo dependencies = get_vk_memory_barrier_depency_info(1, &memoryBarrier);

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
    }

    {
        REAPER_GPU_SCOPE(cmdBuffer, "Copy counters");

        VkBufferCopy2 region = {};
        region.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        region.pNext = nullptr;
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size =
            resources.countersBuffer.properties.element_count * resources.countersBuffer.properties.element_size_bytes;

        const VkCopyBufferInfo2 copy = {VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2, nullptr, resources.countersBuffer.handle,
                                        resources.countersBufferCPU.handle,   1,       &region};

        vkCmdCopyBuffer2(cmdBuffer.handle, &copy);

        vkCmdResetEvent2(cmdBuffer.handle, resources.countersReadyEvent, VK_PIPELINE_STAGE_2_TRANSFER_BIT);

        const GPUResourceAccess src = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
        const GPUResourceAccess dst = {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};
        const GPUBufferView     view = default_buffer_view(resources.countersBufferCPU.properties);

        VkBufferMemoryBarrier2 bufferBarrier =
            get_vk_buffer_barrier(resources.countersBufferCPU.handle, view, src, dst);

        const VkDependencyInfo dependencies = VkDependencyInfo{
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, VK_FLAGS_NONE, 0, nullptr, 1, &bufferBarrier, 0, nullptr};

        vkCmdSetEvent2(cmdBuffer.handle, resources.countersReadyEvent, &dependencies);
    }

    log_debug(root, "CPU mesh stats:");
    for (auto meshlet_count : meshlet_count_per_pass)
    {
        log_debug(root, "- pass total submitted meshlets = {}, approx. triangles = {}", meshlet_count,
                  meshlet_count * MeshletMaxTriangleCount);
    }
    log_debug(root, "- total submitted meshlets = {}, approx. triangles = {}", total_meshlet_count,
              total_meshlet_count * MeshletMaxTriangleCount);
}

std::vector<CullingStats> get_gpu_culling_stats(VulkanBackend& backend, const PreparedData& prepared,
                                                CullResources& resources)
{
    VmaAllocationInfo allocation_info;
    vmaGetAllocationInfo(backend.vma_instance, resources.countersBufferCPU.allocation, &allocation_info);

    void* mapped_data_ptr = nullptr;

    Assert(vkMapMemory(backend.device, allocation_info.deviceMemory, allocation_info.offset, allocation_info.size, 0,
                       &mapped_data_ptr)
           == VK_SUCCESS);

    VkMappedMemoryRange staging_range = {};
    staging_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    staging_range.memory = allocation_info.deviceMemory;
    staging_range.offset = allocation_info.offset;
    staging_range.size = VK_WHOLE_SIZE;
    vkInvalidateMappedMemoryRanges(backend.device, 1, &staging_range);

    Assert(mapped_data_ptr);

    std::vector<CullingStats> stats;

    for (u32 i = 0; i < prepared.cull_passes.size(); i++)
    {
        CullingStats& s = stats.emplace_back();
        s.pass_index = i;
        s.surviving_meshlet_count = static_cast<u32*>(mapped_data_ptr)[i * CountersCount + MeshletCounterOffset];
        s.surviving_triangle_count = static_cast<u32*>(mapped_data_ptr)[i * CountersCount + TriangleCounterOffset];
        s.indirect_draw_command_count =
            static_cast<u32*>(mapped_data_ptr)[i * CountersCount + DrawCommandCounterOffset];
    }

    vkUnmapMemory(backend.device, allocation_info.deviceMemory);

    return stats;
}

namespace
{
    VkIndexType get_vk_culling_index_type()
    {
        if (IndexSizeBytes == 2)
            return VK_INDEX_TYPE_UINT16;
        else if (IndexSizeBytes == 4)
            return VK_INDEX_TYPE_UINT32;
        else
        {
            AssertUnreachable();
        }

        return VK_INDEX_TYPE_NONE_KHR;
    }
} // namespace

CullingDrawParams get_culling_draw_params(u32 pass_index)
{
    CullingDrawParams params;
    params.counter_buffer_offset = (pass_index * CountersCount + DrawCommandCounterOffset) * sizeof(u32);
    params.index_buffer_offset = pass_index * DynamicIndexBufferSizeBytes;
    params.index_type = get_vk_culling_index_type();
    params.command_buffer_offset = pass_index * MaxIndirectDrawCountPerPass * sizeof(VkDrawIndexedIndirectCommand);
    params.command_buffer_max_count = MaxIndirectDrawCountPerPass;

    return params;
}
} // namespace Reaper
