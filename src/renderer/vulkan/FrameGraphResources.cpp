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

FrameGraphResources create_framegraph_resources(ReaperRoot& /*root*/, VulkanBackend& backend)
{
    FrameGraphResources resources = {};

    const VkEventCreateInfo event_info = {
        VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
        nullptr,
        VK_EVENT_CREATE_DEVICE_ONLY_BIT,
    };

    for (auto& event : resources.events)
    {
        vkCreateEvent(backend.device, &event_info, nullptr, &event);
    }

    // Volatile stuff is created later
    return resources;
}

void destroy_framegraph_resources(VulkanBackend& backend, FrameGraphResources& resources)
{
    destroy_framegraph_volatile_resources(backend, resources);

    for (auto& event : resources.events)
    {
        vkDestroyEvent(backend.device, event, nullptr);
    }
}

void allocate_framegraph_volatile_resources(ReaperRoot& root, VulkanBackend& backend, FrameGraphResources& resources,
                                            const FrameGraph::FrameGraph& frame_graph)
{
    destroy_framegraph_volatile_resources(backend, resources); // FIXME Reuse previous resources here instead
    swap_resources(resources);

    using namespace FrameGraph;

    resources.textures.resize(frame_graph.Resources.size()); // FIXME
    resources.buffers.resize(frame_graph.Resources.size());  // FIXME

    for (u32 index = 0; index < frame_graph.Resources.size(); index++)
    {
        const Resource& resource = frame_graph.Resources[index];

        if (resource.IsUsed)
        {
            if (resource.is_texture)
            {
                resources.textures[index] = create_image(root, backend.device, resource.debug_name,
                                                         resource.properties.texture, backend.vma_instance);
                resources.buffers[index] = {};
            }
            else
            {
                resources.buffers[index] = create_buffer(root, backend.device, resource.debug_name,
                                                         resource.properties.buffer, backend.vma_instance);
                resources.textures[index] = {};
            }
        }
        else
        {
            resources.textures[index] = {};
            resources.buffers[index] = {};
        }
    }

    resources.texture_views.resize(frame_graph.ResourceUsages.size());
    for (u32 index = 0; index < frame_graph.ResourceUsages.size(); index++)
    {
        const ResourceUsage& usage = frame_graph.ResourceUsages[index];
        const Resource&      resource = frame_graph.Resources[usage.Resource];

        if (resource.is_texture && usage.IsUsed)
        {
            const ImageInfo&     image = resources.textures[usage.Resource];
            const GPUTextureView view = usage.Usage.texture_view;

            resources.texture_views[index] = create_image_view(root, backend.device, image, view);
        }
        else
        {
            resources.texture_views[index] = VK_NULL_HANDLE;
        }
    }
}

void destroy_framegraph_volatile_resources(VulkanBackend& backend, FrameGraphResources& resources)
{
    for (BufferInfo& buffer : resources.buffers)
    {
        vmaDestroyBuffer(backend.vma_instance, buffer.handle, buffer.allocation);
        buffer = {};
    }

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

VkImageView get_frame_graph_texture_view(FrameGraphResources& resources, FrameGraph::ResourceUsageHandle usage_handle)
{
    return resources.texture_views[usage_handle];
}

BufferInfo& get_frame_graph_buffer(FrameGraphResources& resources, FrameGraph::ResourceHandle resource_handle)
{
    return resources.buffers[resource_handle];
};
} // namespace Reaper
