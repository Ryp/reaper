////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ExecuteFrame.h"

#include "window/Window.h"
#include "vulkan/Backend.h"
#include "vulkan/BackendResources.h"
#include "vulkan/renderpass/Constants.h"
#include "vulkan/renderpass/TestGraphics.h"
#include "vulkan/renderpass/TiledLightingCommon.h"

#include "Camera.h"
#include "PrepareBuckets.h"

#include "common/Log.h"

#include <backends/imgui_impl_vulkan.h>
#include <glm/gtc/constants.hpp>

namespace Reaper
{
void renderer_start(ReaperRoot& root, VulkanBackend& backend, IWindow* window)
{
    create_backend_resources(root, backend);

    log_debug(root, "imgui: upload fonts");

    CommandBuffer& cmdBuffer = backend.resources->gfxCmdBuffer;

    Assert(vkResetCommandPool(backend.device, backend.resources->gfxCommandPool, VK_FLAGS_NONE) == VK_SUCCESS);

    const VkCommandBufferBeginInfo cmdBufferBeginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                         .pNext = nullptr,
                                                         .flags = VK_FLAGS_NONE,
                                                         .pInheritanceInfo = nullptr};

    Assert(vkBeginCommandBuffer(cmdBuffer.handle, &cmdBufferBeginInfo) == VK_SUCCESS);

    // execute a gpu command to upload imgui font textures
    ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer.handle);

    Assert(vkEndCommandBuffer(cmdBuffer.handle) == VK_SUCCESS);

    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuffer.handle, 0, nullptr};
    log_debug(root, "vulkan: submit commands");
    Assert(vkQueueSubmit(backend.deviceInfo.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS);

    Assert(vkDeviceWaitIdle(backend.device) == VK_SUCCESS);

    ImGui_ImplVulkan_DestroyFontUploadObjects();

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

void renderer_execute_frame(ReaperRoot& root, const SceneGraph& scene, std::vector<u8>& audio_output)
{
    VulkanBackend& backend = *root.renderer->backend;

#if REAPER_WINDOWS_HDR_TEST
    static bool fullscreen = false;
    if (fullscreen == false)
    {
        VkResult acquireFullscreenResult =
            vkAcquireFullScreenExclusiveModeEXT(backend.device, backend.presentInfo.swapchain);

        if (acquireFullscreenResult == VK_SUCCESS)
        {
            fullscreen = true;
            // FIXME Trigger resize to check for new formats
            backend.new_swapchain_extent = backend.presentInfo.surface_extent;
            log_info(root, "vulkan: FULLSCREEN!");
        }
        else
        {
            log_error(root, "vulkan: UNABLE TO SET FULLSCREEN!");
        }
    }
#endif

    resize_swapchain(root, backend);

    const VkExtent2D backbufferExtent = backend.presentInfo.surface_extent;
    const glm::uvec2 backbuffer_viewport_extent(backbufferExtent.width, backbufferExtent.height);

    const float near_plane_distance = 0.1f;
    const float far_plane_distance = 100.f;
    const float half_fov_horizontal_radian = glm::pi<float>() * 0.25f;

    const RendererViewport viewport = build_renderer_viewport(backbuffer_viewport_extent);

    const RendererPerspectiveProjection perspective_projection =
        build_renderer_perspective_projection(viewport.aspect_ratio, near_plane_distance, far_plane_distance,
                                              half_fov_horizontal_radian, MainPassUseReverseZ);

    const glm::fmat4x3 main_camera_transform = get_scene_node_transform_slow(scene.camera_node);

    const RendererPerspectiveCamera main_camera =
        build_renderer_perspective_camera(main_camera_transform, perspective_projection, viewport);

    PreparedData prepared;
    prepare_scene(scene, prepared, backend.resources->mesh_cache, main_camera,
                  static_cast<u32>(audio_output.size() / 8));

    TiledLightingFrame tiled_lighting_frame; // FIXME use frame allocator
    prepare_tile_lighting_frame(scene, main_camera, tiled_lighting_frame);

    ImDrawData* imgui_draw_data = ImGui::GetDrawData();

    backend_execute_frame(root, backend, backend.resources->gfxCmdBuffer, prepared, tiled_lighting_frame,
                          *backend.resources, imgui_draw_data);

    if (false) // Re-enable when we're playing with GPU-based sound again
    {
        const auto& gpu_audio_buffer = backend.resources->audio_resources.frame_audio_data;
        audio_output.insert(audio_output.end(), gpu_audio_buffer.begin(), gpu_audio_buffer.end());
    }
}
} // namespace Reaper
