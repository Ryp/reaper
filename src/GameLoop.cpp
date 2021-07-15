////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GameLoop.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/BackendResources.h"
#include "renderer/vulkan/renderpass/TestGraphics.h"

#include "renderer/Camera.h"
#include "renderer/ExecuteFrame.h"
#include "renderer/PrepareBuckets.h"
#include "renderer/ResourceHandle.h"

#include "common/Log.h"
#include "core/Profile.h"
#include "input/DS4.h"

#include <array>
#include <chrono>
#include <thread>

namespace Reaper
{
void execute_game_loop(ReaperRoot& root, VulkanBackend& backend)
{
    IWindow* window = root.renderer->window;

    renderer_start(root, backend, window);

    std::vector<const char*> mesh_filenames = {
        "res/model/teapot.obj",
        "res/model/suzanne.obj",
        "res/model/dragon.obj",
    };

    std::vector<const char*> texture_filenames = {
        "res/texture/default.dds",
        "res/texture/bricks_diffuse.dds",
        "res/texture/bricks_specular.dds",
    };

    std::vector<MeshHandle> mesh_handles(mesh_filenames.size());
    load_meshes(backend, backend.resources->mesh_cache, mesh_filenames, mesh_handles);

    //
    backend.resources->material_resources.texture_handles.resize(texture_filenames.size());
    load_textures(root, backend, backend.resources->material_resources, texture_filenames,
                  backend.resources->material_resources.texture_handles);

    SceneGraph scene;
    build_scene_graph(scene, mesh_handles, backend.resources->material_resources.texture_handles);

    backend.mustTransitionSwapchain = true;

    const u64 MaxFrameCount = 0;
    bool      saveMyLaptop = true;
    bool      shouldExit = false;
    u64       frameIndex = 0;

    DS4 ds4("/dev/input/js0");

    CameraState camera_state = {};
    camera_state.position = glm::vec3(-5.f, 0.f, 0.f);

    const auto startTime = std::chrono::system_clock::now();
    auto       lastFrameStart = startTime;

    while (!shouldExit)
    {
        REAPER_PROFILE_SCOPE("Vulkan", MP_YELLOW);

        const auto currentTime = std::chrono::system_clock::now();

        const auto  timeSecs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
        const auto  timeDeltaMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFrameStart);
        const float timeMs = static_cast<float>(timeSecs.count()) * 0.001f;
        const float timeDtSecs = static_cast<float>(timeDeltaMs.count()) * 0.001f;

        shouldExit = vulkan_process_window_events(root, backend, window);

        ds4.update();

        if (ds4.isPressed(DS4::Square))
            toggle(backend.options.freeze_culling);

        const glm::vec2 yaw_pitch_delta =
            glm::vec2(ds4.getAxis(DS4::RightAnalogY), -ds4.getAxis(DS4::RightAnalogX)) * timeDtSecs;
        const glm::vec2 forward_side_delta =
            glm::vec2(ds4.getAxis(DS4::LeftAnalogY), ds4.getAxis(DS4::LeftAnalogX)) * timeDtSecs;

        update_camera_state(camera_state, yaw_pitch_delta, forward_side_delta);

        renderer_execute_frame(root, scene, camera_state, frameIndex, timeMs);

        if (saveMyLaptop)
        {
            REAPER_PROFILE_SCOPE("Battery saver wait", MP_GREEN);
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
        }

        frameIndex++;
        if (frameIndex == MaxFrameCount)
            shouldExit = true;

        lastFrameStart = currentTime;
    }

    renderer_stop(root, backend, window);
}
} // namespace Reaper
