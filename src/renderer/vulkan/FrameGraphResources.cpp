////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FrameGraphResources.h"

#include "Backend.h"
#include "common/ReaperRoot.h"
#include <profiling/Scope.h>

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
        .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_EVENT_CREATE_DEVICE_ONLY_BIT,
    };

    for (auto& event : resources.events)
    {
        Assert(vkCreateEvent(backend.device, &event_info, nullptr, &event) == VK_SUCCESS);
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
    REAPER_PROFILE_SCOPE_FUNC();

    destroy_framegraph_volatile_resources(backend, resources); // FIXME Reuse previous resources here instead
    swap_resources(resources);

    using namespace FrameGraph;

    resources.textures.resize(framegraph.TextureResources.size());
    resources.buffers.resize(framegraph.BufferResources.size());

    // Create buffers
    for (u32 index = 0; index < framegraph.BufferResources.size(); index++)
    {
        const Resource& resource = framegraph.BufferResources[index];

        if (resource.is_used)
        {
            resources.buffers[index] = create_buffer(root, backend.device, resource.debug_name,
                                                     resource.properties.buffer, backend.vma_instance);
        }
        else
        {
            resources.buffers[index] = {};
        }
    }

    // Create textures
    for (u32 index = 0; index < framegraph.TextureResources.size(); index++)
    {
        const Resource& resource = framegraph.TextureResources[index];

        if (resource.is_used)
        {
            resources.textures[index] = create_image(root, backend.device, resource.debug_name,
                                                     resource.properties.texture, backend.vma_instance);
        }
        else
        {
            resources.textures[index] = {};
        }
    }

    resources.texture_views.resize(framegraph.ResourceUsages.size());
    for (u32 index = 0; index < framegraph.ResourceUsages.size(); index++)
    {
        const ResourceUsage& usage = framegraph.ResourceUsages[index];
        const ResourceHandle resource_handle = usage.resource_handle;

        if (resource_handle.is_texture && usage.is_used)
        {
            const ImageInfo& image = resources.textures[usage.resource_handle.index];

            resources.texture_views[index] = create_image_view(root, backend.device, image, usage.view.texture);
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
    Assert(resource_handle.is_texture, "Wrong handle");

    const ImageInfo& texture = resources.textures[resource_handle.index];
    Assert(texture.handle != VK_NULL_HANDLE);

    return texture.handle;
}

FrameGraphTexture get_frame_graph_texture(FrameGraphResources& resources, const FrameGraph::FrameGraph& framegraph,
                                          FrameGraph::ResourceUsageHandle usage_handle)
{
    using namespace FrameGraph;

    const ResourceUsage& usage = GetResourceUsage(framegraph, usage_handle);
    const ResourceHandle resource_handle = usage.resource_handle;
    Assert(resource_handle.is_texture, "Wrong handle");

    const ImageInfo&   texture = resources.textures[resource_handle.index];
    const VkImageView& view_handle = resources.texture_views[usage_handle];

    Assert(texture.handle != VK_NULL_HANDLE);
    Assert(view_handle != VK_NULL_HANDLE);

    return FrameGraphTexture{
        .properties = texture.properties,
        .view = usage.view.texture,
        .handle = texture.handle,
        .view_handle = view_handle,
        .image_layout = usage.access.image_layout,
    };
}

VkBuffer get_frame_graph_buffer_handle(FrameGraphResources& resources, FrameGraph::ResourceHandle resource_handle)
{
    Assert(!resource_handle.is_texture, "Wrong handle");

    const BufferInfo& buffer = resources.buffers[resource_handle.index];
    Assert(buffer.handle != VK_NULL_HANDLE);

    return buffer.handle;
};

FrameGraphBuffer get_frame_graph_buffer(FrameGraphResources& resources, const FrameGraph::FrameGraph& framegraph,
                                        FrameGraph::ResourceUsageHandle usage_handle)
{
    using namespace FrameGraph;

    const ResourceUsage& usage = GetResourceUsage(framegraph, usage_handle);
    const ResourceHandle resource_handle = usage.resource_handle;
    Assert(!resource_handle.is_texture, "Wrong handle");

    const BufferInfo& buffer = resources.buffers[usage.resource_handle.index];

    return FrameGraphBuffer{
        .properties = buffer.properties,
        .view = usage.view.buffer,
        .handle = buffer.handle,
    };
}

} // namespace Reaper
