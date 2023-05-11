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
                                const FrameGraph::FrameGraph& framegraph, FrameGraphResources& resources,
                                FrameGraph::RenderPassHandle render_pass_handle, bool before)
{
    const auto barrier_events = get_barriers_to_execute(schedule, render_pass_handle, before);

    if (barrier_events.empty())
        return;

    using namespace FrameGraph;
    REAPER_GPU_SCOPE_COLOR(cmdBuffer, "FrameGraph Barrier", MP_RED);

    for (const auto& barrier_event : barrier_events)
    {
        const u32     barrier_handle = barrier_event.barrier_handle;
        const Barrier barrier = schedule.barriers[barrier_handle];

        std::vector<VkImageMemoryBarrier2>  imageBarriers;
        std::vector<VkBufferMemoryBarrier2> bufferBarriers;

        const ResourceUsage& dst_usage = GetResourceUsage(framegraph, barrier.dst.usage_handle);
        const ResourceHandle resource_handle = dst_usage.resource_handle;

        if (resource_handle.is_texture)
        {
            VkImage texture = get_frame_graph_texture_handle(resources, dst_usage.resource_handle);
            imageBarriers.emplace_back(
                get_vk_image_barrier(texture, dst_usage.usage.texture_view, barrier.src.access, barrier.dst.access));
        }
        else
        {
            VkBuffer buffer = get_frame_graph_buffer_handle(resources, dst_usage.resource_handle);
            bufferBarriers.emplace_back(
                get_vk_buffer_barrier(buffer, dst_usage.usage.buffer_view, barrier.src.access, barrier.dst.access));
        }

        const VkDependencyInfo dependencies = VkDependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                                               nullptr,
                                                               VK_FLAGS_NONE,
                                                               0,
                                                               nullptr,
                                                               static_cast<u32>(bufferBarriers.size()),
                                                               bufferBarriers.data(),
                                                               static_cast<u32>(imageBarriers.size()),
                                                               imageBarriers.data()};

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
            vkCmdResetEvent2(cmdBuffer.handle, resources.events[barrier_handle], barrier.dst.access.stage_mask);
        }
    }
}
} // namespace Reaper
