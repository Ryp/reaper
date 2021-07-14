////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ExecuteFrame.h"

#include "vulkan/Backend.h"
#include "vulkan/BackendResources.h"
#include "vulkan/renderpass/TestGraphics.h"

#include "Camera.h"
#include "PrepareBuckets.h"

#include "common/Log.h"

namespace Reaper
{
// FIXME make scene const
void renderer_execute_frame(ReaperRoot& root, SceneGraph& scene, const CameraState& camera_state, u32 frameIndex,
                            float timeMs)
{
    VulkanBackend& backend = *root.renderer->backend;

    resize_swapchain(root, backend, *backend.resources);

    const VkExtent2D backbufferExtent = backend.presentInfo.surfaceExtent;
    const glm::uvec2 backbuffer_viewport_extent(backbufferExtent.width, backbufferExtent.height);
    const glm::mat4  view_matrix = compute_camera_view_matrix(camera_state);

    update_scene_graph(scene, timeMs, backbuffer_viewport_extent, view_matrix);

    log_debug(root, "vulkan: begin frame {}", frameIndex);

    PreparedData prepared;
    prepare_scene(scene, prepared, backend.resources->mesh_cache);

    backend_execute_frame(root, backend, backend.resources->gfxCmdBuffer, prepared, *backend.resources);
}
} // namespace Reaper
