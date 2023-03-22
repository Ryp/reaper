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
#include "neptune/sim/Test.h"
#include "neptune/trackgen/Track.h"

#include "Camera.h"
#include "Geometry.h"
#include "TestTiledLighting.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <chrono>
#include <thread>

#include <backends/imgui_impl_vulkan.h>

#include "renderer/window/Event.h"
#include "renderer/window/Window.h"

#define ENABLE_TEST_SCENE 1
#define ENABLE_CONTROLLER 0
#define ENABLE_GAME_SCENE 0
#define ENABLE_TEST_DRIVE 0
#define ENABLE_FREE_CAM 0

namespace Reaper
{
namespace
{
    void imgui_process_button_press(ImGuiIO& io, Window::MouseButton::type button, bool is_pressed)
    {
        constexpr float ImGuiScrollMultiplier = 0.5f;

        switch (button)
        {
        case Window::MouseButton::Left:
            io.AddMouseButtonEvent(0, is_pressed);
            break;
        case Window::MouseButton::Right:
            io.AddMouseButtonEvent(1, is_pressed);
            break;
        case Window::MouseButton::Middle:
            io.AddMouseButtonEvent(2, is_pressed);
            break;
        case Window::MouseButton::WheelUp:
            io.AddMouseWheelEvent(0.f, is_pressed ? ImGuiScrollMultiplier : 0.f);
            break;
        case Window::MouseButton::WheelDown:
            io.AddMouseWheelEvent(0.f, is_pressed ? -ImGuiScrollMultiplier : 0.f);
            break;
        case Window::MouseButton::WheelLeft:
            io.AddMouseWheelEvent(is_pressed ? ImGuiScrollMultiplier : 0.f, 0.f);
            break;
        case Window::MouseButton::WheelRight:
            io.AddMouseWheelEvent(is_pressed ? -ImGuiScrollMultiplier : 0.f, 0.f);
            break;
        case Window::MouseButton::Invalid:
            break;
        }
    }
} // namespace

void execute_game_loop(ReaperRoot& root)
{
    IWindow*       window = root.renderer->window;
    VulkanBackend& backend = *root.renderer->backend;
    AudioBackend&  audio_backend = *root.audio;

    renderer_start(root, backend, window);

    Neptune::PhysicsSim sim = Neptune::create_sim();
    Neptune::sim_start(&sim);

    SceneGraph scene;

#if ENABLE_TEST_SCENE
    scene = create_test_scene_tiled_lighting(root, backend);
#endif

#if ENABLE_GAME_SCENE
#    if ENABLE_TEST_DRIVE
    const std::string  assetFile("res/model/quad.obj");
    std::vector<Mesh>  meshes;
    const glm::fmat4x3 flat_quad_transform =
        glm::rotate(glm::fmat4(1.f), glm::pi<float>() * -0.5f, glm::fvec3(1.f, 0.f, 0.f));
    const glm::fvec3 flat_quad_scale = glm::fvec3(100.f, 100.f, 100.f);

    {
        std::ifstream file(assetFile);
        meshes.push_back(ModelLoader::loadOBJ(file));
    }

    Neptune::sim_register_static_collision_meshes(sim, meshes, nonstd::span(&flat_quad_transform, 1),
                                                      nonstd::span(&flat_quad_scale, 1));
#    else
    Neptune::Track game_track;

    Neptune::GenerationInfo genInfo = {};
    genInfo.length = 5;
    genInfo.width = 10.0f;
    genInfo.chaos = 1.0f;

    game_track.genInfo = genInfo;

    Neptune::generate_track_skeleton(genInfo, game_track.skeletonNodes);
    Neptune::generate_track_splines(game_track.skeletonNodes, game_track.splinesMS);
    Neptune::generate_track_skinning(game_track.skeletonNodes, game_track.splinesMS, game_track.skinning);

    const std::string         assetFile("res/model/track/chunk_simple.obj");
    std::vector<Mesh>         meshes(genInfo.length);
    std::vector<glm::fmat4x3> chunk_transforms(genInfo.length);

    for (u32 i = 0; i < genInfo.length; i++)
    {
        std::ifstream file(assetFile);
        meshes[i] = ModelLoader::loadOBJ(file);

        const Neptune::TrackSkeletonNode& track_node = game_track.skeletonNodes[i];

        chunk_transforms[i] = glm::translate(glm::mat4(1.0f), track_node.positionWS);

        Neptune::skin_track_chunk_mesh(track_node, game_track.skinning[i], meshes[i], 10.0f);
    }

    // NOTE: bullet will hold pointers to the original mesh data without copy
    Neptune::sim_register_static_collision_meshes(sim, meshes, chunk_transforms);
#    endif

    const glm::fmat4x3 player_initial_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.2f, 0.6f, 0.f));
    const glm::fvec3   player_shape_extent(0.2f, 0.2f, 0.2f);
    Neptune::sim_create_player_rigid_body(sim, player_initial_transform, player_shape_extent);

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

#    if ENABLE_TEST_DRIVE
        SceneMesh scene_mesh;
        scene_mesh.node_index =
            insert_scene_node(scene, glm::fmat4(flat_quad_transform) * glm::scale(glm::fmat4(1.f), flat_quad_scale));
        scene_mesh.mesh_handle = mesh_handles[0];
        scene_mesh.texture_handle = backend.resources->material_resources.texture_handles[0];

        insert_scene_mesh(scene, scene_mesh);
#    else
        // Place static track
        for (u32 chunk_index = 0; chunk_index < genInfo.length; chunk_index++)
        {
            SceneMesh scene_mesh;
            scene_mesh.node_index = insert_scene_node(scene, chunk_transforms[chunk_index]);
            scene_mesh.mesh_handle = mesh_handles[chunk_index]; // FIXME
            scene_mesh.texture_handle = backend.resources->material_resources.texture_handles[0];

            insert_scene_mesh(scene, scene_mesh);
        }
#    endif

        // Add lights
        const glm::vec3 light_target_ws = glm::vec3(0.f, 0.f, 0.f);

        {
            const glm::vec3    light_position_ws = glm::vec3(-1.f, 0.f, 0.f);
            const glm::fmat4x3 light_transform = glm::translate(glm::mat4(1.0f), glm::vec3(2.f, 0.f, 0.f))
                                                 * glm::inverse(glm::lookAt(light_position_ws, light_target_ws, up_ws));

            SceneLight light;
            light.color = glm::fvec3(0.03f, 0.21f, 0.61f);
            light.intensity = 6.f;
            light.radius = 42.f;
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
            light.radius = 42.f;
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
            light.radius = 42.f;
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

#if ENABLE_CONTROLLER
    LinuxController controller = create_controller("/dev/input/js0");
#endif

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

        const MouseState mouse_state = window->get_mouse_state();
        ImGuiIO&         io = ImGui::GetIO();
        io.DisplaySize =
            ImVec2((float)backend.presentInfo.surfaceExtent.width, (float)backend.presentInfo.surfaceExtent.height);
        io.AddMousePosEvent((float)mouse_state.pos_x, (float)mouse_state.pos_y);

        {
            REAPER_PROFILE_SCOPE("Window Events");

            std::vector<Window::Event> events;
            window->pumpEvents(events);

            for (const auto& event : events)
            {
                if (event.type == Window::EventType::Resize)
                {
                    const u32 width = event.message.resize.width;
                    const u32 height = event.message.resize.height;
                    log_debug(root, "window: resize event, width = {}, height = {}", width, height);

                    Assert(width > 0);
                    Assert(height > 0);

                    // FIXME Do not set for duplicate events
                    backend.new_swapchain_extent = {width, height};
                    backend.mustTransitionSwapchain = true;
                }
                else if (event.type == Window::EventType::ButtonPress)
                {
                    Window::Event::Message::ButtonPress buttonpress = event.message.buttonpress;
                    Window::MouseButton::type           button = buttonpress.button;
                    bool                                press = buttonpress.press;

                    log_debug(root, "window: button index = {}, press = {}", Window::get_mouse_button_string(button),
                              press);

                    imgui_process_button_press(io, button, press);
                }
                else if (event.type == Window::EventType::KeyPress)
                {
                    Window::Event::Message::KeyPress keypress = event.message.keypress;

                    if (keypress.key == Window::KeyCode::ESCAPE)
                    {
                        log_warning(root, "window: escape key press detected: now exiting...");
                        shouldExit = true;
                    }
                    else if (keypress.key == Window::KeyCode::UNKNOWN)
                    {
                        log_warning(root, "window: key with unknown key_code '{}' press detected", keypress.key_code);
                    }

                    break;
                }
                else if (event.type == Window::EventType::KeyRelease)
                {
                    Window::Event::Message::KeyPress keypress = event.message.keypress;

                    if (keypress.key == Window::KeyCode::UNKNOWN)
                    {
                        log_warning(root, "window: key with unknown key_code '{}' release detected", keypress.key_code);
                    }
                }
                else
                {
                    log_warning(root, "window: an unknown event has been caught and will not be handled");
                }
            }
        }

#if ENABLE_CONTROLLER
        const GenericControllerState controller_state = update_controller_state(controller);
#else
        const GenericControllerState controller_state = {};
#endif

        if (controller_state.buttons[GenericButton::X].pressed)
            toggle(backend.options.freeze_culling);

        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();

        static bool show_demo_window = false;
        if (show_demo_window)
        {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        ImGui::Render();

#if ENABLE_GAME_SCENE
        Neptune::ShipInput input;
        input.throttle = controller_state.axes[GenericAxis::RT] * 0.5 + 0.5;
        input.brake = controller_state.axes[GenericAxis::LT] * 0.5 + 0.5;
        input.steer = controller_state.axes[GenericAxis::LSX];

        log_debug(root, "sim: throttle = {}, braking = {}, steer = {}", input.throttle, input.brake, input.steer);

        Neptune::sim_update(sim, input, timeDtSecs);

        const glm::fmat4x3 player_transform = Neptune::get_player_transform(sim);
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

    const bool write_audio_to_file = false;
    if (write_audio_to_file)
    {
        // Write recorded audio to filesystem
        std::ofstream output_file("output.wav", std::ios::binary | std::ios::out);
        Assert(output_file.is_open());

        Audio::write_wav(output_file, audio_backend.audio_buffer.data(), audio_backend.audio_buffer.size(),
                         BitsPerChannel, SampleRate);

        output_file.close();
    }

#if ENABLE_CONTROLLER
    destroy_controller(controller);
#endif

    Neptune::destroy_sim(sim);

    renderer_stop(root, backend, window);
}
} // namespace Reaper
