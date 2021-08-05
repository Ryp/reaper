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

#include "renderer/ExecuteFrame.h"
#include "renderer/PrepareBuckets.h"
#include "renderer/ResourceHandle.h"

#include "common/Log.h"
#include "core/Profile.h"
#include "input/DS4.h"
#include "math/Spline.h"
#include "mesh/ModelLoader.h"
#include "splinesonic/sim/Test.h"
#include "splinesonic/trackgen/Track.h"

#include "Camera.h"

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

    SplineSonic::PhysicsSim sim = SplineSonic::create_sim();
    SplineSonic::sim_start(&sim);

    SplineSonic::Track game_track;

    SplineSonic::GenerationInfo genInfo = {};
    genInfo.length = 100;
    genInfo.width = 10.0f;
    genInfo.chaos = 1.0f;

    game_track.genInfo = genInfo;

    SplineSonic::generate_track_skeleton(genInfo, game_track.skeletonNodes);
    SplineSonic::generate_track_splines(game_track.skeletonNodes, game_track.splinesMS);
    SplineSonic::generate_track_skinning(game_track.skeletonNodes, game_track.splinesMS, game_track.skinning);

    const std::string      assetFile("res/model/track/chunk_simple.obj");
    std::vector<Mesh>      meshes(genInfo.length);
    std::vector<glm::mat4> transforms(genInfo.length);

    for (u32 i = 0; i < genInfo.length; i++)
    {
        std::ifstream file(assetFile);
        meshes[i] = ModelLoader::loadOBJ(file);

        const SplineSonic::TrackSkeletonNode& track_node = game_track.skeletonNodes[i];

        transforms[i] = glm::translate(glm::mat4(1.0f), track_node.positionWS);

        SplineSonic::skin_track_chunk_mesh(track_node, game_track.skinning[i], meshes[i], 10.0f);
    }

    // NOTE: bullet will hold pointers to the original mesh data without copy
    SplineSonic::sim_register_static_collision_meshes(sim, meshes, transforms);

    std::ifstream ship_obj_file("res/model/fighter.obj");
    meshes.push_back(ModelLoader::loadOBJ(ship_obj_file));

    SplineSonic::sim_create_player_rigid_body(sim);

    std::vector<MeshHandle> mesh_handles(meshes.size());
    load_meshes(backend, backend.resources->mesh_cache, meshes, mesh_handles);

    std::vector<const char*> texture_filenames = {
        "res/texture/default.dds",
        "res/texture/bricks_diffuse.dds",
        "res/texture/bricks_specular.dds",
    };

    backend.resources->material_resources.texture_handles.resize(texture_filenames.size());
    load_textures(root, backend, backend.resources->material_resources, texture_filenames,
                  backend.resources->material_resources.texture_handles);

    SceneGraph scene;

    // Build scene
    {
        // Place static track
        for (u32 chunk_index = 0; chunk_index < genInfo.length; chunk_index++)
        {
            Node& node = scene.nodes.emplace_back();
            node.instance_id = chunk_index;
            node.mesh_handle = mesh_handles[chunk_index]; // FIXME
            node.texture_handle = backend.resources->material_resources.texture_handles[0];
            node.transform_matrix = transforms[chunk_index];
        }

        // Ship scene node
        {
            Node& node = scene.nodes.emplace_back();
            node.instance_id = genInfo.length;      // FIXME
            node.mesh_handle = mesh_handles.back(); // FIXME
            node.texture_handle = backend.resources->material_resources.texture_handles[1];

            const glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f)); // FIXME
            node.transform_matrix = glm::mat4x3(model);
        }

        build_scene_graph(scene);
    }

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
        REAPER_PROFILE_SCOPE("Frame", MP_YELLOW);

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

        SplineSonic::sim_update(sim, timeDtSecs);

        update_camera_state(camera_state, yaw_pitch_delta, forward_side_delta);

        Node& camera_node = scene.nodes[scene.camera.scene_node];
        camera_node.transform_matrix = compute_camera_view_matrix(camera_state);

        log_debug(root, "renderer: begin frame {}", frameIndex);

        renderer_execute_frame(root, scene);

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

    SplineSonic::destroy_sim(sim);

    renderer_stop(root, backend, window);
}
} // namespace Reaper
