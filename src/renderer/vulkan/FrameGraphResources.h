////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Buffer.h"
#include "Image.h"
#include "renderer/graph/FrameGraph.h"

#include <vector>

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;

struct FrameGraphResources
{
    static const u32 EventCount = 10;
    // Persistent
    // NOTE: you need to have as many events as concurrent synchronization barriers.
    // For the first implem we can just have as many events as barriers.
    std::array<VkEvent, EventCount> events;

    // Buffer
    std::vector<BufferInfo> buffers;

    // Volatile stuff
    std::vector<ImageInfo>   textures;
    std::vector<VkImageView> texture_views;

    std::vector<ImageInfo>   textures_b;
    std::vector<VkImageView> texture_views_b;
};

FrameGraphResources create_framegraph_resources(ReaperRoot& root, VulkanBackend& backend);
void                destroy_framegraph_resources(VulkanBackend& backend, FrameGraphResources& resources);

void allocate_framegraph_volatile_resources(ReaperRoot& root, VulkanBackend& backend, FrameGraphResources& resources,
                                            const FrameGraph::FrameGraph& frame_graph);
void destroy_framegraph_volatile_resources(VulkanBackend& backend, FrameGraphResources& resources);

ImageInfo& get_frame_graph_texture(FrameGraphResources& resources, FrameGraph::ResourceHandle resource_handle);

VkImageView get_frame_graph_texture_view(FrameGraphResources& resources, FrameGraph::ResourceUsageHandle usage_handle);

BufferInfo& get_frame_graph_buffer(FrameGraphResources& resources, FrameGraph::ResourceHandle resource_handle);
} // namespace Reaper
