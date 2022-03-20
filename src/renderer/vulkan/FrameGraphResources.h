////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Image.h"
#include "renderer/graph/FrameGraph.h"

#include <vector>

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;

struct FrameGraphResources
{
    std::vector<ImageInfo>   textures;
    std::vector<VkImageView> texture_views;

    std::vector<ImageInfo>   textures_b;
    std::vector<VkImageView> texture_views_b;
};

void allocate_framegraph_resources(ReaperRoot& root, VulkanBackend& backend, FrameGraphResources& resources,
                                   const FrameGraph::FrameGraph& frame_graph);
void destroy_framegraph_resources(VulkanBackend& backend, FrameGraphResources& resources);

ImageInfo& get_frame_graph_texture(FrameGraphResources& resources, FrameGraph::ResourceHandle resource_handle);

ImageInfo& get_frame_graph_texture(FrameGraphResources& resources, const FrameGraph::FrameGraph& frame_graph,
                                   FrameGraph::ResourceUsageHandle usage_handle);

VkImageView get_frame_graph_texture_view(FrameGraphResources& resources, FrameGraph::ResourceUsageHandle usage_handle);
} // namespace Reaper
