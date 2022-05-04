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

#include "core/Profile.h"

namespace Reaper
{
// FIXME We have no way of merging events for now (dependencies struct)
void record_framegraph_barriers(CommandBuffer& cmdBuffer, const FrameGraph::FrameGraphSchedule& schedule,
                                const FrameGraph::FrameGraph& framegraph, FrameGraphResources& resources,
                                nonstd::span<const FrameGraph::BarrierEvent> barriers, bool before)
{
    if (barriers.empty())
        return;

    using namespace FrameGraph;
    REAPER_PROFILE_SCOPE_GPU(cmdBuffer.mlog, "FrameGraphBarrier", MP_RED);

    for (const auto& barrier_event : barriers)
    {
        const u32     barrier_handle = barrier_event.barrier_handle;
        const Barrier barrier = schedule.barriers[barrier_handle];

        std::vector<VkImageMemoryBarrier2>  imageBarriers;
        std::vector<VkBufferMemoryBarrier2> bufferBarriers;

        const ResourceUsage& dst_usage = GetResourceUsage(framegraph, barrier.dst.usage_handle);
        const Resource&      resource = GetResource(framegraph, dst_usage);

        if (resource.is_texture)
        {
            VkImage texture = get_frame_graph_texture_handle(resources, dst_usage.Resource);
            imageBarriers.emplace_back(
                get_vk_image_barrier(texture, dst_usage.Usage.texture_view, barrier.src.access, barrier.dst.access));
        }
        else
        {
            VkBuffer buffer = get_frame_graph_buffer_handle(resources, dst_usage.Resource);
            bufferBarriers.emplace_back(
                get_vk_buffer_barrier(buffer, dst_usage.Usage.buffer_view, barrier.src.access, barrier.dst.access));
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

        if (barrier_event.type == BarrierType::SingleBefore && before)
        {
            vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
        }
        else if (barrier_event.type == BarrierType::SingleAfter && !before)
        {
            vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
        }
        else if (barrier_event.type == BarrierType::SplitBegin && !before)
        {
            vkCmdSetEvent2(cmdBuffer.handle, resources.events[barrier_handle], &dependencies);
        }
        else if (barrier_event.type == BarrierType::SplitEnd && before)
        {
            vkCmdWaitEvents2(cmdBuffer.handle, 1, &resources.events[barrier_handle], &dependencies);
        }
    }
}
} // namespace Reaper
