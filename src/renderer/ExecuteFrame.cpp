////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ExecuteFrame.h"

#include "window/Window.h"
#include "vulkan/Backend.h"
#include "vulkan/BackendResources.h"
#include "vulkan/renderpass/TestGraphics.h"

#include "Camera.h"
#include "PrepareBuckets.h"

#include "common/Log.h"

namespace Reaper
{
void renderer_start(ReaperRoot& root, VulkanBackend& backend, IWindow* window)
{
    create_backend_resources(root, backend);

    log_info(root, "window: map window");
    window->map();
}

void renderer_stop(ReaperRoot& root, VulkanBackend& backend, IWindow* window)
{
    vkQueueWaitIdle(backend.deviceInfo.presentQueue);

    log_info(root, "window: unmap window");
    window->unmap();

    destroy_backend_resources(backend);
}

void renderer_execute_frame(ReaperRoot& root, const SceneGraph& scene, u32 frameIndex)
{
    VulkanBackend& backend = *root.renderer->backend;

    resize_swapchain(root, backend, *backend.resources);

    const VkExtent2D backbufferExtent = backend.presentInfo.surfaceExtent;
    const glm::uvec2 backbuffer_viewport_extent(backbufferExtent.width, backbufferExtent.height);

    log_debug(root, "vulkan: begin frame {}", frameIndex);

    PreparedData prepared;
    prepare_scene(scene, prepared, backend.resources->mesh_cache, backbuffer_viewport_extent);

    backend_execute_frame(root, backend, backend.resources->gfxCmdBuffer, prepared, *backend.resources);
}
} // namespace Reaper
