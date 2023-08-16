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
        std::swap(resources.default_texture_views, resources.default_texture_views_b);
        std::swap(resources.additional_texture_views, resources.additional_texture_views_b);
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

    // Create buffers
    resources.buffers.resize(framegraph.BufferResources.size());

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
    resources.textures.resize(framegraph.TextureResources.size());
    resources.default_texture_views.resize(framegraph.TextureResources.size());

    for (u32 index = 0; index < framegraph.TextureResources.size(); index++)
    {
        const Resource& resource = framegraph.TextureResources[index];

        if (resource.is_used)
        {
            GPUTexture& new_texture = resources.textures[index];
            new_texture = create_image(root, backend.device, resource.debug_name, resource.properties.texture,
                                       backend.vma_instance);

            resources.default_texture_views[index] =
                create_image_view(root, backend.device, new_texture, resource.default_view.texture);
        }
        else
        {
            resources.textures[index] = {.handle = VK_NULL_HANDLE, .allocation = {}};
            resources.default_texture_views[index] = VK_NULL_HANDLE;
        }
    }

    // Create additional texture views
    resources.additional_texture_views.resize(framegraph.TextureViews.size());

    for (u32 index = 0; index < framegraph.ResourceUsages.size(); index++)
    {
        const ResourceUsage&      usage = framegraph.ResourceUsages[index];
        const ResourceHandle      resource_handle = usage.resource_handle;
        const ResourceViewHandles view_handles = usage.additional_views;

        nonstd::span<const GPUTextureView> texture_views_info =
            nonstd::make_span(framegraph.TextureViews.data() + view_handles.offset, view_handles.count);

        nonstd::span<VkImageView> texture_views =
            nonstd::make_span(resources.additional_texture_views.data() + view_handles.offset, view_handles.count);

        if (resource_handle.is_texture && usage.is_used)
        {
            const GPUTexture& texture = resources.textures[usage.resource_handle.index];

            for (u32 i = 0; i < texture_views.size(); i++)
            {
                texture_views[i] = create_image_view(root, backend.device, texture, texture_views_info[i]);
            }
        }
        else
        {
            for (u32 i = 0; i < texture_views.size(); i++)
            {
                texture_views[i] = VK_NULL_HANDLE;
            }
        }
    }
}

void destroy_framegraph_volatile_resources(VulkanBackend& backend, FrameGraphResources& resources)
{
    for (GPUBuffer& buffer : resources.buffers)
    {
        vmaDestroyBuffer(backend.vma_instance, buffer.handle, buffer.allocation);
        buffer = {};
    }

    for (GPUTexture& texture : resources.textures)
    {
        vmaDestroyImage(backend.vma_instance, texture.handle, texture.allocation);
        texture = {};
    }

    for (VkImageView& view : resources.default_texture_views)
    {
        vkDestroyImageView(backend.device, view, nullptr);
        view = VK_NULL_HANDLE;
    }

    for (VkImageView& view : resources.additional_texture_views)
    {
        vkDestroyImageView(backend.device, view, nullptr);
        view = VK_NULL_HANDLE;
    }
}

VkImage get_frame_graph_texture_handle(FrameGraphResources& resources, FrameGraph::ResourceHandle resource_handle)
{
    Assert(resource_handle.is_texture, "Wrong handle");

    const GPUTexture& texture = resources.textures[resource_handle.index];
    Assert(texture.handle != VK_NULL_HANDLE);

    return texture.handle;
}

FrameGraphTexture get_frame_graph_texture(FrameGraphResources& resources, const FrameGraph::FrameGraph& framegraph,
                                          FrameGraph::ResourceUsageHandle usage_handle)
{
    using namespace FrameGraph;

    const ResourceUsage& usage = GetResourceUsage(framegraph, usage_handle);
    const ResourceHandle resource_handle = usage.resource_handle;
    const Resource&      resource = GetResource(framegraph, resource_handle);
    Assert(resource_handle.is_texture, "Wrong handle");

    const GPUTexture&  texture = resources.textures[resource_handle.index];
    const VkImageView& default_view_handle = resources.default_texture_views[resource_handle.index];

    Assert(texture.handle != VK_NULL_HANDLE);
    Assert(default_view_handle != VK_NULL_HANDLE);

    nonstd::span<const VkImageView> additional_views = nonstd::make_span(
        resources.additional_texture_views.data() + usage.additional_views.offset, usage.additional_views.count);

    return FrameGraphTexture{
        .properties = resource.properties.texture,
        .view = resource.default_view.texture, // FIXME
        .handle = texture.handle,
        .default_view_handle = default_view_handle,
        .additional_views = additional_views,
        .image_layout = usage.access.image_layout,
    };
}

VkBuffer get_frame_graph_buffer_handle(FrameGraphResources& resources, FrameGraph::ResourceHandle resource_handle)
{
    Assert(!resource_handle.is_texture, "Wrong handle");

    const GPUBuffer& buffer = resources.buffers[resource_handle.index];
    Assert(buffer.handle != VK_NULL_HANDLE);

    return buffer.handle;
};

FrameGraphBuffer get_frame_graph_buffer(FrameGraphResources& resources, const FrameGraph::FrameGraph& framegraph,
                                        FrameGraph::ResourceUsageHandle usage_handle)
{
    using namespace FrameGraph;

    const ResourceUsage& usage = GetResourceUsage(framegraph, usage_handle);
    const ResourceHandle resource_handle = usage.resource_handle;
    const Resource&      resource = GetResource(framegraph, resource_handle);
    Assert(!resource_handle.is_texture, "Wrong handle");

    const GPUBuffer& buffer = resources.buffers[usage.resource_handle.index];

    return FrameGraphBuffer{
        // FIXME
        .properties = resource.properties.buffer,
        .default_view = resource.default_view.buffer,
        .handle = buffer.handle,
    };
}

} // namespace Reaper
