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
                                            const FrameGraph::FrameGraph& framegraph)
{
    destroy_framegraph_volatile_resources(backend, resources); // FIXME Reuse previous resources here instead
    swap_resources(resources);

    using namespace FrameGraph;

    resources.textures.resize(framegraph.Resources.size()); // FIXME
    resources.buffers.resize(framegraph.Resources.size());  // FIXME

    for (u32 index = 0; index < framegraph.Resources.size(); index++)
    {
        const Resource& resource = framegraph.Resources[index];

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

    resources.texture_views.resize(framegraph.ResourceUsages.size());
    for (u32 index = 0; index < framegraph.ResourceUsages.size(); index++)
    {
        const ResourceUsage& usage = framegraph.ResourceUsages[index];
        const Resource&      resource = GetResource(framegraph, usage);

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

VkImage get_frame_graph_texture_handle(FrameGraphResources& resources, FrameGraph::ResourceHandle resource_handle)
{
    return resources.textures[resource_handle].handle;
}

FrameGraphTexture get_frame_graph_texture(FrameGraphResources& resources, const FrameGraph::FrameGraph& framegraph,
                                          FrameGraph::ResourceUsageHandle usage_handle)
{
    using namespace FrameGraph;

    const ResourceUsage& usage = GetResourceUsage(framegraph, usage_handle);
    const ImageInfo&     texture = resources.textures[usage.Resource];
    const VkImageView&   view_handle = resources.texture_views[usage_handle];

    return FrameGraphTexture{texture.properties, usage.Usage.texture_view, texture.handle, view_handle};
}

VkBuffer get_frame_graph_buffer_handle(FrameGraphResources& resources, FrameGraph::ResourceHandle resource_handle)
{
    return resources.buffers[resource_handle].handle;
};

FrameGraphBuffer get_frame_graph_buffer(FrameGraphResources& resources, const FrameGraph::FrameGraph& framegraph,
                                        FrameGraph::ResourceUsageHandle usage_handle)
{
    using namespace FrameGraph;

    const ResourceUsage& usage = GetResourceUsage(framegraph, usage_handle);
    const BufferInfo&    buffer = resources.buffers[usage.Resource];

    return FrameGraphBuffer{buffer.properties, usage.Usage.buffer_view, buffer.handle};
}

} // namespace Reaper
