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

#include <array>
#include <vector>

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;

struct FrameGraphTexture
{
    GPUTextureProperties properties;
    GPUTextureView       view;

    VkImage       handle;
    VkImageView   view_handle;
    VkImageLayout image_layout;
};

struct FrameGraphBuffer
{
    GPUBufferProperties properties;
    GPUBufferView       view;

    VkBuffer handle;
};

struct FrameGraphResources
{
    static const u32 EventCount = 100;
    // Persistent
    // NOTE: you need to have as many events as concurrent synchronization barriers.
    // For the first implem we can just have as many events as barriers.
    std::array<VkEvent, EventCount> events;

    // Volatile
    std::vector<BufferInfo> buffers;

    std::vector<ImageInfo> textures;
    std::vector<ImageInfo> textures_b;

    std::vector<VkImageView> texture_views;
    std::vector<VkImageView> texture_views_b;
};

FrameGraphResources create_framegraph_resources(ReaperRoot& root, VulkanBackend& backend);
void                destroy_framegraph_resources(VulkanBackend& backend, FrameGraphResources& resources);

void allocate_framegraph_volatile_resources(ReaperRoot& root, VulkanBackend& backend, FrameGraphResources& resources,
                                            const FrameGraph::FrameGraph& framegraph);
void destroy_framegraph_volatile_resources(VulkanBackend& backend, FrameGraphResources& resources);

VkImage get_frame_graph_texture_handle(FrameGraphResources& resources, FrameGraph::ResourceHandle resource_handle);
FrameGraphTexture get_frame_graph_texture(FrameGraphResources& resources, const FrameGraph::FrameGraph& framegraph,
                                          FrameGraph::ResourceUsageHandle usage_handle);

VkBuffer get_frame_graph_buffer_handle(FrameGraphResources& resources, FrameGraph::ResourceHandle resource_handle);
FrameGraphBuffer get_frame_graph_buffer(FrameGraphResources& resources, const FrameGraph::FrameGraph& framegraph,
                                        FrameGraph::ResourceUsageHandle usage_handle);
} // namespace Reaper
