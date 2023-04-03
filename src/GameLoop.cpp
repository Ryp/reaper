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

#define ENABLE_TEST_SCENE 0
#define ENABLE_LINUX_CONTROLLER 0
#define ENABLE_GAME_SCENE 1
#define ENABLE_FREE_CAM 1

namespace Neptune
{
struct Track
{
    std::vector<TrackSkeletonNode>        skeletonNodes;
    std::vector<Reaper::Math::Spline>     splinesMS;
    std::vector<TrackSkinning>            skinning;
    std::vector<StaticMeshColliderHandle> sim_handles;
};

namespace
{
    Track create_game_track(const GenerationInfo& gen_info, Reaper::VulkanBackend& backend, PhysicsSim& sim,
                            Reaper::SceneGraph& scene, Reaper::TextureHandle texture_handle)
    {
        Track track;

        track.skeletonNodes.resize(gen_info.length);

        generate_track_skeleton(gen_info, track.skeletonNodes);

        track.splinesMS.resize(gen_info.length);

        generate_track_splines(track.skeletonNodes, track.splinesMS);

        track.skinning.resize(gen_info.length);

        generate_track_skinning(track.skeletonNodes, track.splinesMS, track.skinning);

        const std::string         assetFile("res/model/track/chunk_simple.obj");
        std::vector<glm::fmat4x3> chunk_transforms(gen_info.length);
        std::vector<Mesh>         track_meshes(gen_info.length);

        for (u32 i = 0; i < gen_info.length; i++)
        {
            std::ifstream file(assetFile);
            track_meshes[i] = ModelLoader::loadOBJ(file);

            const TrackSkeletonNode& track_node = track.skeletonNodes[i];

            chunk_transforms[i] = glm::translate(glm::mat4(1.0f), track_node.positionWS);

            skin_track_chunk_mesh(track_node, track.skinning[i], track_meshes[i], 10.0f);
        }

        track.sim_handles.resize(gen_info.length);

        // NOTE: bullet will hold pointers to the original mesh data without copy
        sim_create_static_collision_meshes(track.sim_handles, sim, track_meshes, chunk_transforms);

        std::vector<Reaper::MeshHandle> chunk_mesh_handles(track_meshes.size());
        load_meshes(backend, backend.resources->mesh_cache, track_meshes, chunk_mesh_handles);

        // Place static track in the scene
        for (u32 chunk_index = 0; chunk_index < gen_info.length; chunk_index++)
        {
            Reaper::SceneMesh scene_mesh;
            scene_mesh.node_index = insert_scene_node(scene, chunk_transforms[chunk_index]);
            scene_mesh.mesh_handle = chunk_mesh_handles[chunk_index];
            scene_mesh.texture_handle = texture_handle;

            insert_scene_mesh(scene, scene_mesh);
        }

        return track;
    }

    void destroy_game_track(Track& track, Reaper::VulkanBackend& backend, PhysicsSim& sim, Reaper::SceneGraph& scene)
    {
        sim_destroy_static_collision_meshes(track.sim_handles, sim);

        // FIXME Unload texture data (cpu/gpu)
        // FIXME Unload mesh data (cpu/sim/render)
        static_cast<void>(backend);
        static_cast<void>(scene);
    }
} // namespace
} // namespace Neptune

namespace Reaper
{
namespace
{
    constexpr ImU32 ImGuiWhite = IM_COL32(255, 255, 255, 255);
    constexpr ImU32 ImGuiGreen = IM_COL32(0, 255, 0, 255);
    constexpr ImU32 ImGuiGamepadBg = IM_COL32(0, 30, 0, 255);

    void imgui_draw_gamepad_axis(float size_px, float axis_x, float axis_y)
    {
        const float half_size_px = size_px * 0.5f;

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList*  draw_list = ImGui::GetWindowDrawList();

        // Draw safe region
        draw_list->AddRectFilled(pos, ImVec2(pos.x + size_px, pos.y + size_px), ImGuiGamepadBg);

        // Draw frame and cross lines
        draw_list->AddRect(pos, ImVec2(pos.x + size_px, pos.y + size_px), ImGuiWhite);
        draw_list->AddLine(ImVec2(pos.x + half_size_px, pos.y), ImVec2(pos.x + half_size_px, pos.y + size_px),
                           ImGuiWhite);
        draw_list->AddLine(ImVec2(pos.x, pos.y + half_size_px), ImVec2(pos.x + size_px, pos.y + half_size_px),
                           ImGuiWhite);
        draw_list->AddCircle(ImVec2(pos.x + half_size_px, pos.y + half_size_px), half_size_px, ImGuiWhite, 32);

        // Current position
        draw_list->AddCircleFilled(
            ImVec2(pos.x + axis_x * half_size_px + half_size_px, pos.y + axis_y * half_size_px + half_size_px), 10,
            ImGuiWhite);
    }

    void imgui_draw_gamepad_trigger(float w_px, float h_px, float trigger_axis)
    {
        const float mag = trigger_axis * 0.5 + 0.5;
        const float current_y = h_px * (1.0f - mag);

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList*  draw_list = ImGui::GetWindowDrawList();

        // Draw safe region
        draw_list->AddRectFilled(pos, ImVec2(pos.x + w_px, pos.y + h_px), ImGuiGamepadBg);

        // Draw frame
        draw_list->AddRect(pos, ImVec2(pos.x + w_px, pos.y + h_px), ImGuiWhite);

        // Current position
        draw_list->AddLine(ImVec2(pos.x, pos.y + current_y), ImVec2(pos.x + w_px, pos.y + current_y), ImGuiWhite, 8.0f);
    }

    void imgui_controller_debug(const GenericControllerState& controller_state)
    {
        static bool show_window = true;

        const float          pad = 100.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2               work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
                                                           //
        ImGui::SetNextWindowPos(ImVec2(work_pos.x + pad, work_pos.y + pad), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

        if (ImGui::Begin("Controller Axes", &show_window))
        {
            const float size_px = 200.f;
            const float w_px = 40.f;

            ImGui::BeginChild("LeftStick", ImVec2(size_px, size_px));
            imgui_draw_gamepad_axis(size_px, controller_state.axes[GenericAxis::LSX],
                                    controller_state.axes[GenericAxis::LSY]);
            ImGui::EndChild();
            ImGui::SameLine(size_px + 20.f);

            ImGui::BeginChild("LeftTrigger", ImVec2(w_px, size_px));
            imgui_draw_gamepad_trigger(w_px, size_px, controller_state.axes[GenericAxis::LT]);
            ImGui::EndChild();
            ImGui::SameLine(size_px + w_px + 2.f * 20.f);

            // Right pad.
            ImGui::BeginChild("RightStick", ImVec2(size_px, size_px));
            imgui_draw_gamepad_axis(size_px, controller_state.axes[GenericAxis::RSX],
                                    controller_state.axes[GenericAxis::RSY]);
            ImGui::EndChild();
            ImGui::SameLine(size_px * 2.f + w_px + 3.f * 20.f);

            ImGui::BeginChild("RightTrigger", ImVec2(w_px, size_px));
            imgui_draw_gamepad_trigger(w_px, size_px, controller_state.axes[GenericAxis::RT]);
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
    std::vector<const char*> texture_filenames = {
        "res/texture/default.dds",
        "res/texture/bricks_diffuse.dds",
        "res/texture/bricks_specular.dds",
    };

    backend.resources->material_resources.texture_handles.resize(texture_filenames.size());
    load_textures(root, backend, backend.resources->material_resources, texture_filenames,
                  backend.resources->material_resources.texture_handles);

    Neptune::GenerationInfo track_gen_info = {};
    track_gen_info.length = 5;
    track_gen_info.width = 12.0f;
    track_gen_info.chaos = 0.0f;

    Neptune::Track game_track = Neptune::create_game_track(track_gen_info, backend, sim, scene,
                                                           backend.resources->material_resources.texture_handles[0]);

    const glm::fmat4x3 player_initial_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.2f, 0.6f, 0.f));
    const glm::fvec3   player_shape_extent(0.2f, 0.2f, 0.2f);
    Neptune::sim_create_player_rigid_body(sim, player_initial_transform, player_shape_extent);

    std::vector<Mesh> meshes;
    std::ifstream     ship_obj_file("res/model/fighter.obj");

    meshes.push_back(ModelLoader::loadOBJ(ship_obj_file));

    std::vector<MeshHandle> mesh_handles(meshes.size());
    load_meshes(backend, backend.resources->mesh_cache, meshes, mesh_handles);

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

    GenericControllerState last_controller_state = create_generic_controller_state();

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
                    const Window::KeyCode::type      key = keypress.key;
                    const bool                       is_pressed = keypress.press;

                    if (is_pressed && key == Window::KeyCode::ESCAPE)
                    {
                        log_warning(root, "window: escape key press detected: now exiting...");
                        shouldExit = true;
                    }
                    else if (key == Window::KeyCode::A) // FIXME key auto-repeat will ruin this for us
                    {
                        controller_state.axes[GenericAxis::LSX] = is_pressed ? -1.f : 0.f;
                    }
                    else if (key == Window::KeyCode::D)
                    {
                        controller_state.axes[GenericAxis::LSX] = is_pressed ? 1.f : 0.f;
                    }
                    else if (key == Window::KeyCode::W)
                    {
                        controller_state.axes[GenericAxis::LSY] = is_pressed ? -1.f : 0.f;
                    }
                    else if (key == Window::KeyCode::S)
                    {
                        controller_state.axes[GenericAxis::LSY] = is_pressed ? 1.f : 0.f;
                    }
                    else if (key == Window::KeyCode::ARROW_LEFT)
                    {
                        controller_state.axes[GenericAxis::RSX] = is_pressed ? -1.f : 0.f;
                    }
                    else if (key == Window::KeyCode::ARROW_RIGHT)
                    {
                        controller_state.axes[GenericAxis::RSX] = is_pressed ? 1.f : 0.f;
                    }
                    else if (key == Window::KeyCode::ARROW_UP)
                    {
                        controller_state.axes[GenericAxis::RSY] = is_pressed ? -1.f : 0.f;
                        controller_state.axes[GenericAxis::RT] = is_pressed ? 1.f : -1.f;
                    }
                    else if (key == Window::KeyCode::ARROW_DOWN)
                    {
                        controller_state.axes[GenericAxis::RSY] = is_pressed ? 1.f : 0.f;
                        controller_state.axes[GenericAxis::LT] = is_pressed ? 1.f : -1.f;
                    }

                    if (key == Window::KeyCode::Invalid)
                    {
                        log_warning(root, "window: key with unknown key_code '{}' {} detected", keypress.key_code,
                                    is_pressed ? "press" : "release");
                    }
                    else
                    {
                        log_debug(root, "window: key '{}' {} detected", Window::get_keyboard_key_string(key),
                                  is_pressed ? "press" : "release");
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
        glm::fvec3         player_translation = player_transform[3];

        log_debug(root, "sim: player pos = ({}, {}, {})", player_translation[0], player_translation[1],
                  player_translation[2]);

        SceneNode& player_scene_node = scene.nodes[player_scene_node_index];
        player_scene_node.transform_matrix = player_transform;
#endif

        {
            const u32  length_min = 1;
            const u32  length_max = 100;
            static u32 track_gen_length = track_gen_info.length;
            static f32 track_gen_width = track_gen_info.width;
            static f32 track_gen_chaos = track_gen_info.chaos;

            static bool show_window = true;

            const float          pad = 100.0f;
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImVec2               work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
                                                               //
            ImGui::SetNextWindowPos(ImVec2(work_pos.x + pad, work_pos.y + pad), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

            if (ImGui::Begin("Physics", &show_window))
            {
                ImGui::InputFloat3("ship position", &player_translation[0], "%.3f", ImGuiInputTextFlags_ReadOnly);

                ImGui::Separator();
                ImGui::SliderScalar("length", ImGuiDataType_U32, &track_gen_length, &length_min, &length_max, "%u");
                ImGui::SliderFloat("witdh", &track_gen_width, 1.f, 10.f);
                ImGui::SliderFloat("chaos", &track_gen_chaos, 0.f, 1.f);

                if (ImGui::Button("Generate new track"))
                {
                    Neptune::destroy_game_track(game_track, backend, sim, scene);

                    game_track = Neptune::create_game_track(track_gen_info, backend, sim, scene,
                                                            backend.resources->material_resources.texture_handles[0]);
                }
            }

            ImGui::End();
        }

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

    Neptune::destroy_game_track(game_track, backend, sim, scene);

#if ENABLE_LINUX_CONTROLLER
    destroy_controller(controller);
#endif

    Neptune::destroy_sim(sim);

    renderer_stop(root, backend, window);
}
} // namespace Reaper
