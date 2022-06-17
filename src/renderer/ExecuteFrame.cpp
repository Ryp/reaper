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
#include "vulkan/renderpass/TestGraphics.h"

#include "PrepareBuckets.h"

#include "common/Log.h"

#include <backends/imgui_impl_vulkan.h>

namespace Reaper
{
void renderer_start(ReaperRoot& root, VulkanBackend& backend, IWindow* window)
{
    create_backend_resources(root, backend);

    log_debug(root, "imgui: upload fonts");

    CommandBuffer& cmdBuffer = backend.resources->gfxCmdBuffer;
    Assert(vkResetCommandBuffer(cmdBuffer.handle, 0) == VK_SUCCESS);

    VkCommandBufferBeginInfo cmdBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
        0,      // Not caring yet
        nullptr // No inheritance yet
    };

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

    resize_swapchain(root, backend);

    const VkExtent2D backbufferExtent = backend.presentInfo.surfaceExtent;
    const glm::uvec2 backbuffer_viewport_extent(backbufferExtent.width, backbufferExtent.height);

    PreparedData prepared;
    prepare_scene(scene, prepared, backend.resources->mesh_cache, backbuffer_viewport_extent,
                  static_cast<u32>(audio_output.size() / 8));

    ImDrawData* imgui_draw_data = ImGui::GetDrawData();

    backend_execute_frame(root, backend, backend.resources->gfxCmdBuffer, prepared, *backend.resources,
                          imgui_draw_data);

    const auto& gpu_audio_buffer = backend.resources->audio_resources.frame_audio_data;
    audio_output.insert(audio_output.end(), gpu_audio_buffer.begin(), gpu_audio_buffer.end());
}
} // namespace Reaper
