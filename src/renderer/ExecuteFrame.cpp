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
#include "vulkan/api/AssertHelper.h"
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

    AssertVk(vkResetCommandPool(backend.device, backend.resources->gfxCommandPool, VK_FLAGS_NONE));

    const VkCommandBufferBeginInfo cmdBufferBeginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                         .pNext = nullptr,
                                                         .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                         .pInheritanceInfo = nullptr};

    AssertVk(vkBeginCommandBuffer(cmdBuffer.handle, &cmdBufferBeginInfo));

    // execute a gpu command to upload imgui font textures
    ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer.handle);

    AssertVk(vkEndCommandBuffer(cmdBuffer.handle));

    const VkCommandBufferSubmitInfo command_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmdBuffer.handle,
        .deviceMask = 0, // NOTE: Set to zero when not using device groups
    };

    const VkSubmitInfo2 submit_info_2 = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .waitSemaphoreInfoCount = 0,
        .pWaitSemaphoreInfos = nullptr,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &command_buffer_info,
        .signalSemaphoreInfoCount = 0,
        .pSignalSemaphoreInfos = nullptr,
    };

    log_debug(root, "vulkan: submit commands");
    AssertVk(vkQueueSubmit2(backend.graphics_queue, 1, &submit_info_2, VK_NULL_HANDLE));

    AssertVk(vkQueueWaitIdle(backend.graphics_queue));

    ImGui_ImplVulkan_DestroyFontUploadObjects();

    log_info(root, "window: map window");
    window->map();
}

void renderer_stop(ReaperRoot& root, VulkanBackend& backend, IWindow* window)
{
    vkDeviceWaitIdle(backend.device);

    log_info(root, "window: unmap window");
    window->unmap();

    destroy_backend_resources(backend);
}

void renderer_execute_frame(ReaperRoot& root, const SceneGraph& scene, std::vector<u8>& audio_output,
                            std::span<DebugGeometryUserCommand> debug_draw_commands)
{
    VulkanBackend& backend = *root.renderer->backend;

    resize_swapchain(root, backend);

    const float near_plane_distance = 0.1f;
    const float far_plane_distance = 1000.f;
    const float half_fov_horizontal_radian = glm::pi<float>() * 0.25f;

    const RendererViewport viewport =
        build_renderer_viewport({backend.render_extent.width, backend.render_extent.height});

    const RendererPerspectiveProjection perspective_projection =
        build_renderer_perspective_projection(viewport.aspect_ratio, near_plane_distance, far_plane_distance,
                                              half_fov_horizontal_radian, MainPassUseReverseZ);

    const glm::fmat4x3 main_camera_transform = get_scene_node_transform_slow(scene.camera_node);

    const RendererPerspectiveCamera main_camera =
        build_renderer_perspective_camera(main_camera_transform, perspective_projection, viewport);

    PreparedData prepared;
    prepare_scene(scene, prepared, backend.resources->mesh_cache, main_camera,
                  static_cast<u32>(audio_output.size() / 8));

    prepared.debug_draw_commands = debug_draw_commands;

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
