////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FrameGraphPass.h"

#include "renderer/graph/FrameGraph.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/FrameGraphResources.h"
#include "renderer/vulkan/GpuProfile.h"

namespace Reaper
{
// FIXME We have no way of merging events for now (dependencies struct)
void record_framegraph_barriers(CommandBuffer& cmdBuffer, const FrameGraph::FrameGraphSchedule& schedule,
                                const FrameGraph::FrameGraph& framegraph, const FrameGraphResources& resources,
                                FrameGraph::RenderPassHandle render_pass_handle, bool before)
{
    const auto barrier_events = get_barriers_to_execute(schedule, render_pass_handle, before);

    if (barrier_events.empty())
        return;

    using namespace FrameGraph;
    REAPER_GPU_SCOPE_COLOR(cmdBuffer, "FrameGraph Barrier", Color::Red);

    for (const auto& barrier_event : barrier_events)
    {
        const u32     barrier_handle = barrier_event.barrier_handle;
        const Barrier barrier = schedule.barriers[barrier_handle];

        std::vector<VkImageMemoryBarrier2>  imageBarriers;
        std::vector<VkBufferMemoryBarrier2> bufferBarriers;

        const ResourceUsage& dst_usage = GetResourceUsage(framegraph, barrier.dst.usage_handle);
        const ResourceHandle resource_handle = dst_usage.resource_handle;
        const Resource&      resource = GetResource(framegraph, resource_handle);

        if (resource_handle.is_texture)
        {
            VkImage texture = get_frame_graph_texture_handle(resources, resource_handle);
            imageBarriers.emplace_back(get_vk_image_barrier(texture, resource.default_view.texture.subresource,
                                                            to_texture_access(barrier.src.access),
                                                            to_texture_access(barrier.dst.access)));
        }
        else
        {
            VkBuffer buffer = get_frame_graph_buffer_handle(resources, resource_handle);
            bufferBarriers.emplace_back(get_vk_buffer_barrier(buffer, resource.default_view.buffer,
                                                              to_buffer_access(barrier.src.access),
                                                              to_buffer_access(barrier.dst.access)));
        }

        const VkDependencyInfo dependencies = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = VK_FLAGS_NONE,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = static_cast<u32>(bufferBarriers.size()),
            .pBufferMemoryBarriers = bufferBarriers.data(),
            .imageMemoryBarrierCount = static_cast<u32>(imageBarriers.size()),
            .pImageMemoryBarriers = imageBarriers.data(),
        };

        if ((barrier_event.barrier_type & BarrierType::Immediate) != 0)
        {
            vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
        }
        else if (barrier_event.barrier_type == BarrierType::SplitBegin)
        {
            Assert(barrier_handle < resources.events.size());
            vkCmdSetEvent2(cmdBuffer.handle, resources.events[barrier_handle], &dependencies);
        }
        else if (barrier_event.barrier_type == BarrierType::SplitEnd)
        {
            Assert(barrier_handle < resources.events.size());
            vkCmdWaitEvents2(cmdBuffer.handle, 1, &resources.events[barrier_handle], &dependencies);
            vkCmdResetEvent2(cmdBuffer.handle, resources.events[barrier_handle],
                             barrier.dst.access.stage_mask | VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT); // FIXME
        }
    }
}
} // namespace Reaper
