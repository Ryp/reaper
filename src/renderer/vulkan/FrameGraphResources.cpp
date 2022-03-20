////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FrameGraphResources.h"

#include "Backend.h"
#include "common/ReaperRoot.h"

namespace Reaper
{
namespace
{
    void swap_resources(FrameGraphResources& resources)
    {
        std::swap(resources.textures, resources.textures_b);
        std::swap(resources.texture_views, resources.texture_views_b);
    }
} // namespace

void allocate_framegraph_resources(ReaperRoot& root, VulkanBackend& backend, FrameGraphResources& resources,
                                   const FrameGraph::FrameGraph& frame_graph)
{
    destroy_framegraph_resources(backend, resources); // FIXME Reuse previous resources here instead
    swap_resources(resources);

    using namespace FrameGraph;

    resources.textures.resize(frame_graph.Resources.size());
    for (u32 index = 0; index < frame_graph.Resources.size(); index++)
    {
        const Resource& resource = frame_graph.Resources[index];

        if (resource.IsUsed)
        {
            resources.textures[index] =
                create_image(root, backend.device, resource.Identifier, resource.Descriptor, backend.vma_instance);
        }
        else
        {
            resources.textures[index] = {};
        }
    }

    resources.texture_views.resize(frame_graph.ResourceUsages.size());
    for (u32 index = 0; index < frame_graph.ResourceUsages.size(); index++)
    {
        const ResourceUsage& usage = frame_graph.ResourceUsages[index];

        if (usage.IsUsed)
        {
            const ImageInfo&     image = resources.textures[usage.Resource];
            const GPUTextureView view = usage.Usage.view;

            resources.texture_views[index] = create_image_view(root, backend.device, image, view);
        }
        else
        {
            resources.texture_views[index] = VK_NULL_HANDLE;
        }
    }
}

void destroy_framegraph_resources(VulkanBackend& backend, FrameGraphResources& resources)
{
    for (ImageInfo& texture : resources.textures)
    {
        vmaDestroyImage(backend.vma_instance, texture.handle, texture.allocation);
        texture = {};
    }

    for (VkImageView& view : resources.texture_views)
    {
        vkDestroyImageView(backend.device, view, nullptr);
        view = VK_NULL_HANDLE;
    }
}

ImageInfo& get_frame_graph_texture(FrameGraphResources& resources, FrameGraph::ResourceHandle resource_handle)
{
    return resources.textures[resource_handle];
}

ImageInfo& get_frame_graph_texture(FrameGraphResources& resources, const FrameGraph::FrameGraph& frame_graph,
                                   FrameGraph::ResourceUsageHandle usage_handle)
{
    using namespace FrameGraph;

    const ResourceUsage& usage = GetResourceUsage(frame_graph, usage_handle);
    const ResourceHandle resource_handle = usage.Resource;

    return get_frame_graph_texture(resources, resource_handle);
}

VkImageView get_frame_graph_texture_view(FrameGraphResources& resources, FrameGraph::ResourceUsageHandle usage_handle)
{
    return resources.texture_views[usage_handle];
}
} // namespace Reaper
