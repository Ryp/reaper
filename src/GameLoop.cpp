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
#include "neptune/sim/Test.h"
#include "neptune/trackgen/Track.h"
#include "profiling/Scope.h"

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
#define ENABLE_LINUX_CONTROLLER 0
#define ENABLE_GAME_SCENE 0
#define ENABLE_TEST_DRIVE 0
#define ENABLE_FREE_CAM 1

namespace Reaper
{
namespace
{
    void imgui_draw_gamepad_axis(float size_px, float axis_x, float axis_y)
    {
        const ImU32 white_color = IM_COL32(255, 255, 255, 255);
        const float half_size_px = size_px * 0.5f;

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList*  draw_list = ImGui::GetWindowDrawList();

        // Draw safe region
        draw_list->AddRectFilled(pos, ImVec2(pos.x + size_px, pos.y + size_px), IM_COL32(0, 30, 0, 255));

        // Draw frame and cross lines
        draw_list->AddRect(pos, ImVec2(pos.x + size_px, pos.y + size_px), white_color);
        draw_list->AddLine(ImVec2(pos.x + half_size_px, pos.y), ImVec2(pos.x + half_size_px, pos.y + size_px),
                           white_color);
        draw_list->AddLine(ImVec2(pos.x, pos.y + half_size_px), ImVec2(pos.x + size_px, pos.y + half_size_px),
                           white_color);
        draw_list->AddCircle(ImVec2(pos.x + half_size_px, pos.y + half_size_px), half_size_px, white_color, 32);

        // Current position
        draw_list->AddCircleFilled(
            ImVec2(pos.x + axis_x * half_size_px + half_size_px, pos.y + axis_y * half_size_px + half_size_px), 10,
            white_color);
    }

    void imgui_controller_debug(const GenericControllerState& controller_state)
    {
        static bool show_window = true;
        static int  corner = 0;

        ImGuiWindowFlags window_flags =
            0; // ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

        if (corner != -1)
        {
            const float          PAD = 100.0f;
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImVec2               work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
            ImVec2               work_size = viewport->WorkSize;
            ImVec2               window_pos, window_pos_pivot;
            window_pos.x = (corner & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
            window_pos.y = (corner & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
            window_pos_pivot.x = (corner & 1) ? 1.0f : 0.0f;
            window_pos_pivot.y = (corner & 2) ? 1.0f : 0.0f;
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        }

        ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

        if (ImGui::Begin("Controller Axes", &show_window, window_flags))
        {
            const float size_px = 200.f;

            // Left pad.
            ImGui::BeginChild("LeftStick", ImVec2(size_px, size_px));
            imgui_draw_gamepad_axis(size_px, controller_state.axes[GenericAxis::LSX],
                                    controller_state.axes[GenericAxis::LSY]);
            ImGui::EndChild();
            ImGui::SameLine(size_px + 20.f);

            // Right pad.
            ImGui::BeginChild("RightStick", ImVec2(size_px, size_px));
            imgui_draw_gamepad_axis(size_px, controller_state.axes[GenericAxis::RSX],
                                    controller_state.axes[GenericAxis::RSY]);
            ImGui::EndChild();
        }

        ImGui::End();
    }

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

    GenericControllerState last_controller_state = {};

#if ENABLE_LINUX_CONTROLLER
    LinuxController controller = create_controller("/dev/input/js0", last_controller_state);
#endif

#if ENABLE_FREE_CAM
    SceneNode& camera_node = scene.nodes[scene.camera.scene_node];

    // Try to match the camera state with the initial transform of the scene node
    CameraState camera_state = {};
    camera_state.position = camera_node.transform_matrix[3];
#endif

    const auto startTime = std::chrono::system_clock::now();
    auto       last_frame_start = startTime;

    while (!shouldExit)
    {
        REAPER_PROFILE_SCOPE("Frame");

        const auto currentTime = std::chrono::system_clock::now();

        const auto time_delta_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - last_frame_start);
        const float time_delta_secs = static_cast<float>(time_delta_ms.count()) * 0.001f;

        const MouseState mouse_state = window->get_mouse_state();
        ImGuiIO&         io = ImGui::GetIO();
        io.DisplaySize =
            ImVec2((float)backend.presentInfo.surfaceExtent.width, (float)backend.presentInfo.surfaceExtent.height);
        io.AddMousePosEvent((float)mouse_state.pos_x, (float)mouse_state.pos_y);

        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();

#if ENABLE_LINUX_CONTROLLER
        GenericControllerState controller_state = update_controller_state(controller, last_controller_state);
#else
        GenericControllerState controller_state = last_controller_state;
#endif

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
                    bool                             is_pressed = keypress.press;

                    if (keypress.press && keypress.key == Window::KeyCode::ESCAPE)
                    {
                        log_warning(root, "window: escape key press detected: now exiting...");
                        shouldExit = true;
                    }
                    else if (keypress.key == Window::KeyCode::A)
                    {
                        controller_state.axes[GenericAxis::LSX] += is_pressed ? -1.f : 1.f;
                    }
                    else if (keypress.key == Window::KeyCode::D)
                    {
                        controller_state.axes[GenericAxis::LSX] += is_pressed ? 1.f : -1.f;
                    }
                    else if (keypress.key == Window::KeyCode::W)
                    {
                        controller_state.axes[GenericAxis::LSY] += is_pressed ? -1.f : 1.f;
                    }
                    else if (keypress.key == Window::KeyCode::S)
                    {
                        controller_state.axes[GenericAxis::LSY] += is_pressed ? 1.f : -1.f;
                    }
                    else if (keypress.key == Window::KeyCode::ARROW_LEFT)
                    {
                        controller_state.axes[GenericAxis::RSX] += is_pressed ? -1.f : 1.f;
                    }
                    else if (keypress.key == Window::KeyCode::ARROW_RIGHT)
                    {
                        controller_state.axes[GenericAxis::RSX] += is_pressed ? 1.f : -1.f;
                    }
                    else if (keypress.key == Window::KeyCode::ARROW_UP)
                    {
                        controller_state.axes[GenericAxis::RSY] += is_pressed ? -1.f : 1.f;
                    }
                    else if (keypress.key == Window::KeyCode::ARROW_DOWN)
                    {
                        controller_state.axes[GenericAxis::RSY] += is_pressed ? 1.f : -1.f;
                    }

                    if (keypress.key == Window::KeyCode::Invalid)
                    {
                        log_warning(root, "window: key with unknown key_code '{}' {} detected", keypress.key_code,
                                    keypress.press ? "press" : "release");
                    }
                    else
                    {
                        // FIXME debug
                        log_warning(root, "window: key '{}' {} detected", Window::get_keyboard_key_string(keypress.key),
                                    keypress.press ? "press" : "release");
                    }

                    break;
                }
                else
                {
                    log_warning(root, "window: an unknown event has been caught and will not be handled");
                }
            }
        }

        // Up until now you can manually mutate the controller state
        compute_controller_transient_state(last_controller_state, controller_state);
        // From that point on you should treat this data as immutable

        if (controller_state.buttons[GenericButton::X].pressed)
            toggle(backend.options.freeze_culling);

        imgui_controller_debug(controller_state);

        backend_debug_ui(backend);

#if ENABLE_GAME_SCENE
        Neptune::ShipInput input;
        input.throttle = controller_state.axes[GenericAxis::RT] * 0.5 + 0.5;
        input.brake = controller_state.axes[GenericAxis::LT] * 0.5 + 0.5;
        input.steer = controller_state.axes[GenericAxis::LSX];

        log_debug(root, "sim: throttle = {}, braking = {}, steer = {}", input.throttle, input.brake, input.steer);

        Neptune::sim_update(sim, input, time_delta_secs);

        const glm::fmat4x3 player_transform = Neptune::get_player_transform(sim);
        const glm::fvec3   player_translation = player_transform[3];

        log_debug(root, "sim: player pos = ({}, {}, {})", player_translation[0], player_translation[1],
                  player_translation[2]);

        SceneNode& player_scene_node = scene.nodes[player_scene_node_index];
        player_scene_node.transform_matrix = player_transform;
#endif

#if ENABLE_FREE_CAM
        const glm::vec2 yaw_pitch_delta =
            glm::vec2(controller_state.axes[GenericAxis::RSX], -controller_state.axes[GenericAxis::RSY])
            * time_delta_secs;
        const glm::vec2 forward_side_delta =
            glm::vec2(controller_state.axes[GenericAxis::LSY], controller_state.axes[GenericAxis::LSX])
            * time_delta_secs;

        update_camera_state(camera_state, yaw_pitch_delta, forward_side_delta);

        camera_node.transform_matrix = glm::inverse(compute_camera_view_matrix(camera_state));
#endif

        ImGui::Render();

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

        last_frame_start = currentTime;
        last_controller_state = controller_state;
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

#if ENABLE_LINUX_CONTROLLER
    destroy_controller(controller);
#endif

    Neptune::destroy_sim(sim);

    renderer_stop(root, backend, window);
}
} // namespace Reaper
