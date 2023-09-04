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
#include "neptune/sim/PhysicsSim.h"
#include "neptune/sim/PhysicsSimUpdate.h"
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
#define ENABLE_FREE_CAM 0

namespace Neptune
{
struct Track
{
    std::vector<TrackSkeletonNode>    skeleton_nodes;
    std::vector<Reaper::Math::Spline> splines_ms;
    std::vector<TrackSkinning>        skinning;

    std::vector<StaticMeshColliderHandle> sim_handles;
    std::vector<Reaper::SceneMesh>        scene_meshes;
};

namespace
{
    Track create_game_track(const GenerationInfo& gen_info, Reaper::VulkanBackend& backend, PhysicsSim& sim,
                            Reaper::SceneGraph& scene, Reaper::SceneMaterialHandle material_handle)
    {
        Track track;

        track.skeleton_nodes.resize(gen_info.length);

        generate_track_skeleton(gen_info, track.skeleton_nodes);

        track.splines_ms.resize(gen_info.length);

        generate_track_splines(track.skeleton_nodes, track.splines_ms);

        track.skinning.resize(gen_info.length);

        generate_track_skinning(track.skeleton_nodes, track.splines_ms, track.skinning);

        const std::string         assetFile("res/model/track/chunk_simple.obj");
        std::vector<glm::fmat4x3> chunk_transforms(gen_info.length);
        std::vector<Mesh>         track_meshes;

        for (u32 i = 0; i < gen_info.length; i++)
        {
            std::ifstream file(assetFile);
            Mesh&         track_mesh = track_meshes.emplace_back(ModelLoader::loadOBJ(file));

            const TrackSkeletonNode& track_node = track.skeleton_nodes[i];

            chunk_transforms[i] = track_node.in_transform_ms_to_ws;

            skin_track_chunk_mesh(track_node, track.skinning[i], track_mesh, 10.0f);
        }

        track.sim_handles.resize(gen_info.length);

        // NOTE: bullet will hold pointers to the original mesh data without copy
        sim_create_static_collision_meshes(track.sim_handles, sim, track_meshes, chunk_transforms);

        std::vector<Reaper::MeshHandle> chunk_mesh_handles(track_meshes.size());
        load_meshes(backend, backend.resources->mesh_cache, track_meshes, chunk_mesh_handles);

        // Place static track in the scene
        for (u32 chunk_index = 0; chunk_index < gen_info.length; chunk_index++)
        {
            Reaper::SceneMesh& scene_mesh = track.scene_meshes.emplace_back();
            scene_mesh.scene_node = create_scene_node(scene, chunk_transforms[chunk_index]);
            scene_mesh.mesh_handle = chunk_mesh_handles[chunk_index];
            scene_mesh.material_handle = material_handle;
        }

        return track;
    }

    void destroy_game_track(Track& track, Reaper::VulkanBackend& backend, PhysicsSim& sim, Reaper::SceneGraph& scene)
    {
        sim_destroy_static_collision_meshes(track.sim_handles, sim);

        for (auto scene_mesh : track.scene_meshes)
        {
            destroy_scene_node(scene, scene_mesh.scene_node);
        }

        // FIXME Unload texture data (cpu/gpu)
        // FIXME Unload mesh data (cpu/render)
        static_cast<void>(backend);
    }
} // namespace
} // namespace Neptune

namespace Reaper
{
namespace
{
    constexpr ImU32 ImGuiWhite = IM_COL32(255, 255, 255, 255);
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

    void imgui_sim_debug(Neptune::PhysicsSim& sim)
    {
        ImGui::SliderFloat("simulation_substep_duration", &sim.vars.simulation_substep_duration, 1.f / 200.f,
                           1.f / 5.f);
        ImGui::SliderInt("max_simulation_substep_count", &sim.vars.max_simulation_substep_count, 1, 10);
        ImGui::SliderFloat("gravity_force_intensity", &sim.vars.gravity_force_intensity, 0.f, 100.f, "%.3f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("linear_friction", &sim.vars.linear_friction, 0.f, 100.f, "%.3f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("quadratic_friction", &sim.vars.quadratic_friction, 0.f, 10.f, "%.3f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("angular_friction", &sim.vars.angular_friction, 0.f, 100.f, "%.3f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("max_suspension_force", &sim.vars.max_suspension_force, 0.f, 100000.f, "%.3f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("default_spring_stiffness", &sim.vars.default_spring_stiffness, 0.0f, 100.f, "%.3f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("default_damper_friction_compression", &sim.vars.default_damper_friction_compression, 0.0f,
                           100.f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("default_damper_friction_extension", &sim.vars.default_damper_friction_extension, 0.0f,
                           100.f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::BeginDisabled();
        ImGui::SliderFloat("suspension ratio 0", &sim.raycast_suspensions[0].length_ratio_last, 0.0f, 1.f, "%.3f");
        ImGui::SliderFloat("suspension ratio 1", &sim.raycast_suspensions[1].length_ratio_last, 0.0f, 1.f, "%.3f");
        ImGui::SliderFloat("suspension ratio 2", &sim.raycast_suspensions[2].length_ratio_last, 0.0f, 1.f, "%.3f");
        ImGui::SliderFloat("suspension ratio 3", &sim.raycast_suspensions[3].length_ratio_last, 0.0f, 1.f, "%.3f");
        ImGui::EndDisabled();
        ImGui::SliderFloat("default_ship_stats.thrust", &sim.vars.default_ship_stats.thrust, 0.f, 1000.f);
        ImGui::SliderFloat("default_ship_stats.braking", &sim.vars.default_ship_stats.braking, 0.f, 100.f);
        ImGui::SliderFloat("default_ship_stats.handling", &sim.vars.default_ship_stats.handling, 0.1f, 10.f);
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
    // scene = create_test_scene_tiled_lighting(root, backend);
    scene = create_static_test_scene(root, backend);
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

    const SceneMaterialHandle track_material_handle = SceneMaterialHandle(scene.scene_materials.size()); // FIXME
    scene.scene_materials.emplace_back(SceneMaterial{
        .base_color_texture = backend.resources->material_resources.texture_handles[0],
        .metal_roughness_texture = InvalidTextureHandle,
        .normal_map_texture = InvalidTextureHandle,
    });

    Neptune::Track game_track = Neptune::create_game_track(track_gen_info, backend, sim, scene, track_material_handle);

    const glm::fmat4x3 player_initial_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.4f, 0.8f, 0.f));
    const glm::fvec3   player_shape_half_extent(0.4f, 0.3f, 0.3f);
    Neptune::sim_create_player_rigid_body(sim, player_initial_transform, player_shape_half_extent);

    std::vector<Mesh> ship_meshes;
    std::ifstream     ship_obj_file("res/model/fighter.obj");

    ship_meshes.push_back(ModelLoader::loadOBJ(ship_obj_file));

    MeshHandle ship_mesh_handle;
    load_meshes(backend, backend.resources->mesh_cache, ship_meshes, std::span(&ship_mesh_handle, 1));

    const SceneMaterialHandle player_material_handle = SceneMaterialHandle(scene.scene_materials.size()); // FIXME
    scene.scene_materials.emplace_back(SceneMaterial{
        .base_color_texture = backend.resources->material_resources.texture_handles[1],
        .metal_roughness_texture = InvalidTextureHandle,
        .normal_map_texture = InvalidTextureHandle,
    });

    // Build scene
    SceneNode* player_scene_node = nullptr;
    SceneMesh  player_scene_mesh;

    {
        const glm::vec3 up_ws = glm::vec3(0.f, 1.f, 0.f);

        // Ship scene node
        {
            // NOTE: one node is for the physics object, and the mesh is a child node
            player_scene_node = create_scene_node(scene, player_initial_transform);

#    if ENABLE_FREE_CAM
            const glm::fvec3 camera_position = glm::vec3(-5.f, 0.f, 0.f);
            const glm::fvec3 camera_local_target = glm::vec3(0.f, 0.f, 0.f);
            SceneNode*       camera_parent_node = nullptr;
#    else
            const glm::fvec3 camera_position = glm::vec3(-2.0f, 0.8f, 0.f);
            const glm::fvec3 camera_local_target = glm::vec3(1.f, 0.4f, 0.f);
            SceneNode*       camera_parent_node = player_scene_node;
#    endif

            const glm::fmat4x3 camera_local_transform =
                glm::inverse(glm::lookAt(camera_position, camera_local_target, up_ws));
            scene.camera_node = create_scene_node(scene, camera_local_transform, camera_parent_node);

            const glm::fmat4x3 mesh_local_transform =
                glm::rotate(glm::scale(glm::identity<glm::fmat4>(), glm::vec3(0.05f)), glm::pi<float>() * -0.5f, up_ws);

            player_scene_mesh.scene_node = create_scene_node(scene, mesh_local_transform, player_scene_node);
            player_scene_mesh.mesh_handle = ship_mesh_handle;
            player_scene_mesh.material_handle = player_material_handle;
        }

        // Add lights
        const glm::vec3 light_target_ws = glm::vec3(0.f, 0.f, 0.f);

        {
            const glm::vec3    light_position_ws = glm::vec3(-1.f, 0.f, 0.f);
            const glm::fmat4x3 light_transform = glm::translate(glm::mat4(1.0f), glm::vec3(2.f, 0.f, 0.f))
                                                 * glm::inverse(glm::lookAt(light_position_ws, light_target_ws, up_ws));

            SceneLight& light = scene.scene_lights.emplace_back();
            light.color = glm::fvec3(0.03f, 0.21f, 0.61f);
            light.intensity = 6.f;
            light.radius = 42.f;
            light.scene_node = create_scene_node(scene, light_transform, player_scene_node);
            light.shadow_map_size = glm::uvec2(1024, 1024);
        }

        {
            const glm::fvec3   light_position_ws = glm::vec3(3.f, 3.f, 3.f);
            const glm::fmat4x3 light_transform = glm::inverse(glm::lookAt(light_position_ws, light_target_ws, up_ws));

            SceneLight& light = scene.scene_lights.emplace_back();
            light.color = glm::fvec3(0.61f, 0.21f, 0.03f);
            light.intensity = 6.f;
            light.radius = 42.f;
            light.scene_node = create_scene_node(scene, light_transform);
            light.shadow_map_size = glm::uvec2(512, 512);
        }

        {
            const glm::vec3    light_position_ws = glm::vec3(0.f, 3.f, -3.f);
            const glm::fmat4x3 light_transform = glm::inverse(glm::lookAt(light_position_ws, light_target_ws, up_ws));

            SceneLight& light = scene.scene_lights.emplace_back();
            light.color = glm::fvec3(0.03f, 0.8f, 0.21f);
            light.intensity = 6.f;
            light.radius = 42.f;
            light.scene_node = create_scene_node(scene, light_transform);
            light.shadow_map_size = glm::uvec2(256, 256);
        }
    }
#endif

    const u64 MaxFrameCount = 0;
    bool      saveMyLaptop = false;
    bool      shouldExit = false;
    u64       frameIndex = 0;

    GenericControllerState last_controller_state = create_generic_controller_state();

#if ENABLE_LINUX_CONTROLLER
    LinuxController controller = create_controller("/dev/input/js0", last_controller_state);
#endif

#if ENABLE_FREE_CAM
    // Try to match the camera state with the initial transform of the scene node
    CameraState camera_state = {};
    camera_state.position = scene.camera_node->transform_matrix[3];
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
            ImVec2((float)backend.presentInfo.surface_extent.width, (float)backend.presentInfo.surface_extent.height);
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
            toggle(backend.options.freeze_meshlet_culling);

        imgui_controller_debug(controller_state);

        backend_debug_ui(backend);

#if ENABLE_GAME_SCENE
        Neptune::ShipInput input;
        input.throttle = controller_state.axes[GenericAxis::RT] * 0.5 + 0.5;
        input.brake = controller_state.axes[GenericAxis::LT] * 0.5 + 0.5;
        input.steer = controller_state.axes[GenericAxis::RSX];

        Neptune::sim_update(sim, game_track.skeleton_nodes, input, time_delta_secs);

        const glm::fmat4x3 player_transform = Neptune::get_player_transform(sim);
        glm::fvec3         player_translation = player_transform[3];

        player_scene_node->transform_matrix = player_transform;

        {
            constexpr u32 length_min = 1;
            constexpr u32 length_max = 100;

            static bool          show_window = true;
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
                ImGui::SliderScalar("length", ImGuiDataType_U32, &track_gen_info.length, &length_min, &length_max,
                                    "%u");
                ImGui::SliderFloat("witdh", &track_gen_info.width, 1.f, 10.f);
                ImGui::SliderFloat("chaos", &track_gen_info.chaos, 0.f, 1.f);

                if (ImGui::Button("Generate new track"))
                {
                    Neptune::destroy_game_track(game_track, backend, sim, scene);

                    // We don't handle fragmentation yet, so we recreate ALL renderer meshes
                    clear_meshes(backend.resources->mesh_cache);

                    game_track = Neptune::create_game_track(track_gen_info, backend, sim, scene, track_material_handle);

                    // We kill the ship mesh as well, so let's rebuild it
                    load_meshes(backend, backend.resources->mesh_cache, ship_meshes, std::span(&ship_mesh_handle, 1));

                    // Patch the scene to use the right mesh handle, otherwise we might crash!
                    player_scene_mesh.mesh_handle = ship_mesh_handle;
                }

                ImGui::Separator();
                imgui_sim_debug(sim);
            }

            ImGui::End();
        }
#endif

#if ENABLE_FREE_CAM
        const glm::vec2 yaw_pitch_delta =
            glm::vec2(controller_state.axes[GenericAxis::RSX], -controller_state.axes[GenericAxis::RSY])
            * time_delta_secs;
        const glm::vec2 forward_side_delta =
            glm::vec2(controller_state.axes[GenericAxis::LSY], controller_state.axes[GenericAxis::LSX])
            * time_delta_secs;

        update_camera_state(camera_state, yaw_pitch_delta, forward_side_delta);

        scene.camera_node->transform_matrix = glm::inverse(compute_camera_view_matrix(camera_state));
#endif

        ImGui::Render();

        log_debug(root, "renderer: begin frame {}", frameIndex);

#if ENABLE_GAME_SCENE
        // Scene meshes are rebuilt every frame
        scene.scene_meshes.clear();
        scene.scene_meshes.insert(scene.scene_meshes.end(), game_track.scene_meshes.begin(),
                                  game_track.scene_meshes.end());
        scene.scene_meshes.push_back(player_scene_mesh);
#endif

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

#if ENABLE_GAME_SCENE
    Neptune::destroy_game_track(game_track, backend, sim, scene);
#endif

#if ENABLE_LINUX_CONTROLLER
    destroy_controller(controller);
#endif

    Neptune::destroy_sim(sim);

    renderer_stop(root, backend, window);
}
} // namespace Reaper
