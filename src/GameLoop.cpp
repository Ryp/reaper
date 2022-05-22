////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GameLoop.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/BackendResources.h"
#include "renderer/vulkan/renderpass/TestGraphics.h"

#include "renderer/ExecuteFrame.h"
#include "renderer/PrepareBuckets.h"
#include "renderer/ResourceHandle.h"

#include "audio/AudioBackend.h"
#include "audio/WaveFormat.h"
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
void execute_game_loop(ReaperRoot& root)
{
    IWindow*       window = root.renderer->window;
    VulkanBackend& backend = *root.renderer->backend;
    AudioBackend&  audio_backend = *root.audio;

    renderer_start(root, backend, window);

    SplineSonic::PhysicsSim sim = SplineSonic::create_sim();
    SplineSonic::sim_start(&sim);

    SplineSonic::Track game_track;

    SplineSonic::GenerationInfo genInfo = {};
    genInfo.length = 5;
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

    // Build scene
    SceneGraph scene;
    {
        // Add camera
        {
            scene.camera.scene_node = insert_scene_node(scene, glm::mat4x3(1.0f));
        }
        // Place static track
        for (u32 chunk_index = 0; chunk_index < genInfo.length; chunk_index++)
        {
            SceneMesh scene_mesh;
            scene_mesh.node_index = insert_scene_node(scene, transforms[chunk_index]);
            scene_mesh.mesh_handle = mesh_handles[chunk_index]; // FIXME
            scene_mesh.texture_handle = backend.resources->material_resources.texture_handles[0];

            insert_scene_mesh(scene, scene_mesh);
        }

        // Ship scene node
        {
            const glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f)); // FIXME

            SceneMesh scene_mesh;
            scene_mesh.node_index = insert_scene_node(scene, glm::fmat4x3(model));
            scene_mesh.mesh_handle = mesh_handles.back(); // FIXME
            scene_mesh.texture_handle = backend.resources->material_resources.texture_handles[1];

            insert_scene_mesh(scene, scene_mesh);
        }

        // Add lights
        const glm::mat4 light_projection_matrix = build_perspective_matrix(0.1f, 100.f, 1.f, glm::pi<float>() * 0.25f);
        const glm::vec3 light_target_ws = glm::vec3(0.f, 0.f, 0.f);
        const glm::vec3 up_ws = glm::vec3(0.f, 1.f, 0.f);

        {
            const glm::vec3    light_position_ws = glm::vec3(-2.f, 2.f, 2.f);
            const glm::fmat4x3 light_transform = glm::lookAt(light_position_ws, light_target_ws, up_ws);

            SceneLight light;
            light.color = glm::fvec3(0.03f, 0.21f, 0.61f);
            light.intensity = 6.f;
            light.scene_node = insert_scene_node(scene, light_transform);
            light.projection_matrix = light_projection_matrix;
            light.shadow_map_size = glm::uvec2(1024, 1024);

            insert_scene_light(scene, light);
        }

        {
            const glm::fvec3   light_position_ws = glm::vec3(-2.f, -2.f, -2.f);
            const glm::fmat4x3 light_transform = glm::lookAt(light_position_ws, light_target_ws, up_ws);

            SceneLight light;
            light.color = glm::fvec3(0.61f, 0.21f, 0.03f);
            light.intensity = 6.f;
            light.scene_node = insert_scene_node(scene, light_transform);
            light.projection_matrix = light_projection_matrix;
            light.shadow_map_size = glm::uvec2(512, 512);

            insert_scene_light(scene, light);
        }

        {
            const glm::vec3    light_position_ws = glm::vec3(0.f, -2.f, 2.f);
            const glm::fmat4x3 light_transform = glm::lookAt(light_position_ws, light_target_ws, up_ws);

            SceneLight light;
            light.color = glm::fvec3(0.03f, 0.8f, 0.21f);
            light.intensity = 6.f;
            light.scene_node = insert_scene_node(scene, light_transform);
            light.projection_matrix = light_projection_matrix;
            light.shadow_map_size = glm::uvec2(256, 256);

            insert_scene_light(scene, light);
        }
    }

    backend.mustTransitionSwapchain = true;

    const u64 MaxFrameCount = 0;
    bool      saveMyLaptop = true;
    bool      shouldExit = false;
    u64       frameIndex = 0;

    DS4 ds4("/dev/input/js0");
    ds4.connect();

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

        renderer_execute_frame(root, scene, audio_backend.audio_buffer);

        audio_execute_frame(root, audio_backend);

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

    const bool write_audio_to_file = true;
    if (write_audio_to_file)
    {
        // Write recorded audio to filesystem
        std::ofstream output_file("output.wav", std::ios::binary | std::ios::out);
        Assert(output_file.is_open());

        Audio::write_wav(output_file, audio_backend.audio_buffer.data(), audio_backend.audio_buffer.size(),
                         BitsPerChannel, SampleRate);

        output_file.close();
    }

    SplineSonic::destroy_sim(sim);

    renderer_stop(root, backend, window);
}
} // namespace Reaper
