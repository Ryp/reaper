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
#include "input/LinuxController.h"
#include "math/Spline.h"
#include "mesh/ModelLoader.h"
#include "profiling/Scope.h"
#include "splinesonic/sim/Test.h"
#include "splinesonic/trackgen/Track.h"

#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <chrono>
#include <thread>

#define ENABLE_TEST_SCENE 0
#define ENABLE_GAME_SCENE 1
#define ENABLE_FREE_CAM 0

namespace Reaper
{
namespace
{
#if ENABLE_TEST_SCENE
    SceneGraph create_static_test_scene_graph(MeshHandle mesh_handle, TextureHandle texture_handle)
    {
        SceneGraph scene;
        scene.camera.scene_node = insert_scene_node(scene, glm::translate(glm::mat4(1.0f), glm::vec3(-10.f, 3.f, 3.f)));

        constexpr i32 asteroid_count = 4;

        // Place static track
        for (i32 i = 0; i < asteroid_count; i++)
        {
            for (i32 j = 0; j < asteroid_count; j++)
            {
                for (i32 k = 0; k < asteroid_count; k++)
                {
                    glm::fvec3   position_ws(static_cast<float>(i * 2), static_cast<float>(j * 2),
                                             static_cast<float>(k * 2));
                    glm::fmat4x3 transform = glm::translate(glm::mat4(1.0f), position_ws);

                    SceneMesh scene_mesh;
                    scene_mesh.node_index = insert_scene_node(scene, transform);
                    scene_mesh.mesh_handle = mesh_handle;
                    scene_mesh.texture_handle = texture_handle;

                    insert_scene_mesh(scene, scene_mesh);
                }
            }
        }

        const glm::vec3    light_target_ws = glm::vec3(0.f, 0.f, 0.f);
        const glm::vec3    up_ws = glm::vec3(0.f, 1.f, 0.f);
        const glm::vec3    light_position_ws = glm::vec3(-4.f, -4.f, -4.f);
        const glm::fmat4x3 light_transform = glm::inverse(glm::lookAt(light_position_ws, light_target_ws, up_ws));

        SceneLight light;
        light.color = glm::fvec3(1.f, 1.f, 1.f);
        light.intensity = 60.f;
        light.scene_node = insert_scene_node(scene, light_transform);

#    if 0
        light.shadow_map_size = glm::uvec2(1024, 1024);
#    else
        light.shadow_map_size = glm::uvec2(0, 0);
#    endif

        insert_scene_light(scene, light);

        return scene;
    }
#endif
} // namespace

void execute_game_loop(ReaperRoot& root)
{
    IWindow*       window = root.renderer->window;
    VulkanBackend& backend = *root.renderer->backend;
    AudioBackend&  audio_backend = *root.audio;

    renderer_start(root, backend, window);

    SplineSonic::PhysicsSim sim = SplineSonic::create_sim();
    SplineSonic::sim_start(&sim);

    SceneGraph scene;

#if ENABLE_TEST_SCENE
    std::vector<Mesh> meshes;
    std::ifstream     obj_file("res/model/asteroid.obj");
    meshes.push_back(ModelLoader::loadOBJ(obj_file));

    std::vector<MeshHandle> mesh_handles(meshes.size());
    load_meshes(backend, backend.resources->mesh_cache, meshes, mesh_handles);

    std::vector<const char*> texture_filenames = {"res/texture/default.dds"};

    backend.resources->material_resources.texture_handles.resize(texture_filenames.size());
    load_textures(root, backend, backend.resources->material_resources, texture_filenames,
                  backend.resources->material_resources.texture_handles);

    scene =
        create_static_test_scene_graph(mesh_handles.front(), backend.resources->material_resources.texture_handles[0]);
#endif

#if ENABLE_GAME_SCENE
    SplineSonic::Track game_track;

    SplineSonic::GenerationInfo genInfo = {};
    genInfo.length = 5;
    genInfo.width = 10.0f;
    genInfo.chaos = 1.0f;

    game_track.genInfo = genInfo;

    SplineSonic::generate_track_skeleton(genInfo, game_track.skeletonNodes);
    SplineSonic::generate_track_splines(game_track.skeletonNodes, game_track.splinesMS);
    SplineSonic::generate_track_skinning(game_track.skeletonNodes, game_track.splinesMS, game_track.skinning);

    const std::string         assetFile("res/model/track/chunk_simple.obj");
    std::vector<Mesh>         meshes(genInfo.length);
    std::vector<glm::fmat4x3> chunk_transforms(genInfo.length);

    for (u32 i = 0; i < genInfo.length; i++)
    {
        std::ifstream file(assetFile);
        meshes[i] = ModelLoader::loadOBJ(file);

        const SplineSonic::TrackSkeletonNode& track_node = game_track.skeletonNodes[i];

        chunk_transforms[i] = glm::translate(glm::mat4(1.0f), track_node.positionWS);

        SplineSonic::skin_track_chunk_mesh(track_node, game_track.skinning[i], meshes[i], 10.0f);
    }

    // NOTE: bullet will hold pointers to the original mesh data without copy
    SplineSonic::sim_register_static_collision_meshes(sim, meshes, chunk_transforms);

    const glm::fmat4x3 player_initial_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.2f, 0.6f, 0.f));
    const glm::fvec3   player_shape_extent(0.2f, 0.1f, 0.1f);
    SplineSonic::sim_create_player_rigid_body(sim, player_initial_transform, player_shape_extent);

    std::ifstream ship_obj_file("res/model/fighter.obj");
    meshes.push_back(ModelLoader::loadOBJ(ship_obj_file));

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
    u32 player_scene_node_index;
    {
        const glm::vec3 up_ws = glm::vec3(0.f, 1.f, 0.f);

        // Ship scene node
        {
            // NOTE: one node is for the physics object, and the mesh is a child node
            player_scene_node_index = insert_scene_node(scene, player_initial_transform);

#    if ENABLE_FREE_CAM
            const glm::fvec3 camera_position = glm::vec3(-5.f, 0.f, 0.f);
            const glm::fvec3 camera_local_target = glm::vec3(0.f, 0.f, 0.f);
            const u32        camera_parent_index = InvalidNodeIndex;
#    else
            const glm::fvec3 camera_position = glm::vec3(-2.f, 1.f, 0.f);
            const glm::fvec3 camera_local_target = glm::vec3(1.f, 0.f, 0.f);
            const u32        camera_parent_index = player_scene_node_index;
#    endif

            const glm::fmat4x3 camera_local_transform =
                glm::inverse(glm::lookAt(camera_position, camera_local_target, up_ws));
            scene.camera.scene_node = insert_scene_node(scene, camera_local_transform, camera_parent_index);

            const glm::fmat4x3 mesh_local_transform =
                glm::rotate(glm::scale(glm::fmat4(1.f), glm::vec3(0.05f)), glm::pi<float>() * -0.5f, up_ws);

            SceneMesh scene_mesh;
            scene_mesh.node_index = insert_scene_node(scene, mesh_local_transform, player_scene_node_index);
            scene_mesh.mesh_handle = mesh_handles.back(); // FIXME
            scene_mesh.texture_handle = backend.resources->material_resources.texture_handles[1];

            insert_scene_mesh(scene, scene_mesh);
        }

        // Place static track
        for (u32 chunk_index = 0; chunk_index < genInfo.length; chunk_index++)
        {
            SceneMesh scene_mesh;
            scene_mesh.node_index = insert_scene_node(scene, chunk_transforms[chunk_index]);
            scene_mesh.mesh_handle = mesh_handles[chunk_index]; // FIXME
            scene_mesh.texture_handle = backend.resources->material_resources.texture_handles[0];

            insert_scene_mesh(scene, scene_mesh);
        }

        // Add lights
        const glm::vec3 light_target_ws = glm::vec3(0.f, 0.f, 0.f);

        {
            const glm::vec3    light_position_ws = glm::vec3(-1.f, 0.f, 0.f);
            const glm::fmat4x3 light_transform = glm::translate(glm::mat4(1.0f), glm::vec3(2.f, 0.f, 0.f))
                                                 * glm::inverse(glm::lookAt(light_position_ws, light_target_ws, up_ws));

            SceneLight light;
            light.color = glm::fvec3(0.03f, 0.21f, 0.61f);
            light.intensity = 6.f;
            light.scene_node = insert_scene_node(scene, light_transform, player_scene_node_index);
            light.shadow_map_size = glm::uvec2(1024, 1024);

            insert_scene_light(scene, light);
        }

        {
            const glm::fvec3   light_position_ws = glm::vec3(3.f, 3.f, 3.f);
            const glm::fmat4x3 light_transform = glm::inverse(glm::lookAt(light_position_ws, light_target_ws, up_ws));

            SceneLight light;
            light.color = glm::fvec3(0.61f, 0.21f, 0.03f);
            light.intensity = 6.f;
            light.scene_node = insert_scene_node(scene, light_transform);
            light.shadow_map_size = glm::uvec2(512, 512);

            insert_scene_light(scene, light);
        }

        {
            const glm::vec3    light_position_ws = glm::vec3(0.f, 3.f, -3.f);
            const glm::fmat4x3 light_transform = glm::inverse(glm::lookAt(light_position_ws, light_target_ws, up_ws));

            SceneLight light;
            light.color = glm::fvec3(0.03f, 0.8f, 0.21f);
            light.intensity = 6.f;
            light.scene_node = insert_scene_node(scene, light_transform);
            light.shadow_map_size = glm::uvec2(256, 256);

            insert_scene_light(scene, light);
        }
    }
#endif

    backend.mustTransitionSwapchain = true;

    const u64 MaxFrameCount = 0;
    bool      saveMyLaptop = false;
    bool      shouldExit = false;
    u64       frameIndex = 0;

    LinuxController controller = create_controller("/dev/input/js0");

#if ENABLE_FREE_CAM
    SceneNode& camera_node = scene.nodes[scene.camera.scene_node];

    // Try to match the camera state with the initial transform of the scene node
    CameraState camera_state = {};
    camera_state.position = camera_node.transform_matrix[3];
#endif

    const auto startTime = std::chrono::system_clock::now();
    auto       lastFrameStart = startTime;

    while (!shouldExit)
    {
        REAPER_PROFILE_SCOPE("Frame");

        const auto currentTime = std::chrono::system_clock::now();

        const auto  timeDeltaMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFrameStart);
        const float timeDtSecs = static_cast<float>(timeDeltaMs.count()) * 0.001f;

        shouldExit = vulkan_process_window_events(root, backend, window);

        const GenericControllerState controller_state = update_controller_state(controller);

        if (controller_state.buttons[GenericButton::X].pressed)
            toggle(backend.options.freeze_culling);

#if ENABLE_GAME_SCENE
        SplineSonic::sim_update(sim, timeDtSecs);

        const glm::fmat4x3 player_transform = SplineSonic::get_player_transform(sim);
        const glm::fvec3   player_translation = player_transform[3];

        log_debug(root, "sim: player pos = ({}, {}, {})", player_translation[0], player_translation[1],
                  player_translation[2]);

        SceneNode& player_scene_node = scene.nodes[player_scene_node_index];
        player_scene_node.transform_matrix = player_transform;
#endif

#if ENABLE_FREE_CAM
        const glm::vec2 yaw_pitch_delta =
            glm::vec2(controller_state.axes[GenericAxis::RSY], -controller_state.axes[GenericAxis::RSX]) * timeDtSecs;
        const glm::vec2 forward_side_delta =
            glm::vec2(controller_state.axes[GenericAxis::LSY], controller_state.axes[GenericAxis::LSX]) * timeDtSecs;

        update_camera_state(camera_state, yaw_pitch_delta, forward_side_delta);

        camera_node.transform_matrix = glm::inverse(compute_camera_view_matrix(camera_state));
#endif

        log_debug(root, "renderer: begin frame {}", frameIndex);

        renderer_execute_frame(root, scene, audio_backend.audio_buffer);

        audio_execute_frame(root, audio_backend);

        if (saveMyLaptop)
        {
            REAPER_PROFILE_SCOPE("Battery saver wait");
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

    destroy_controller(controller);

    SplineSonic::destroy_sim(sim);

    renderer_stop(root, backend, window);
}
} // namespace Reaper
