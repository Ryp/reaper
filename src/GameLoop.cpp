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
#include "math/Spline.h"
#include "mesh/ModelLoader.h"
#include "splinesonic/trackgen/Track.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <chrono>
#include <thread>

namespace Reaper
{
void execute_game_loop(ReaperRoot& root, VulkanBackend& backend)
{
    IWindow* window = root.renderer->window;

    renderer_start(root, backend, window);

    SplineSonic::TrackGen::Track testTrack;

    SplineSonic::TrackGen::GenerationInfo genInfo = {};
    genInfo.length = 100;
    genInfo.width = 10.0f;
    genInfo.chaos = 1.0f;

    testTrack.genInfo = genInfo;

    SplineSonic::TrackGen::GenerateTrackSkeleton(genInfo, testTrack.skeletonNodes);
    SplineSonic::TrackGen::GenerateTrackSplines(testTrack.skeletonNodes, testTrack.splinesMS);
    SplineSonic::TrackGen::GenerateTrackSkinning(testTrack.skeletonNodes, testTrack.splinesMS, testTrack.skinning);

    const std::string assetFile("res/model/track/chunk_simple.obj");
    std::vector<Mesh> meshes(genInfo.length);

    for (u32 i = 0; i < genInfo.length; i++)
    {
        std::ifstream file(assetFile);
        meshes[i] = ModelLoader::loadOBJ(file);

        SplineSonic::TrackGen::SkinTrackChunkMesh(testTrack.skeletonNodes[i], testTrack.skinning[i], meshes[i], 10.0f);
    }

    std::vector<MeshHandle> mesh_handles(meshes.size());
    load_meshes(backend, backend.resources->mesh_cache, meshes, mesh_handles);

    std::vector<const char*> texture_filenames = {
        "res/texture/default.dds",
        "res/texture/bricks_diffuse.dds",
        "res/texture/bricks_specular.dds",
    };

    //
    backend.resources->material_resources.texture_handles.resize(texture_filenames.size());
    load_textures(root, backend, backend.resources->material_resources, texture_filenames,
                  backend.resources->material_resources.texture_handles);

    SceneGraph scene;

    Assert(!mesh_handles.empty());
    for (u32 mesh_index = 0; mesh_index < mesh_handles.size(); mesh_index++)
    {
        // const SplineSonic::TrackGen::TrackSkeletonNode track_node = testTrack.skeletonNodes[mesh_index];

        Node& node = scene.nodes.emplace_back();
        node.instance_id = mesh_index;
        node.mesh_handle = mesh_handles[mesh_index];                                    // FIXME
        node.texture_handle = backend.resources->material_resources.texture_handles[0]; // FIXME

        const glm::mat4 model = glm::mat4(1.0f); // FIXME baked ws transform in ms vertices
        // const glm::mat4 model = glm::translate(glm::mat4(1.0f), track_node.positionWS);
        node.transform_matrix = glm::mat4x3(model);
    }

    build_scene_graph(scene);

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

        const auto  timeDeltaMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFrameStart);
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

        renderer_execute_frame(root, scene, camera_state, frameIndex);

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
