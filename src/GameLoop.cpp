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

#include "renderer/DebugGeometryCommandRecordAPI.h"
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

#include <cgltf.h>

#define ENABLE_TEST_SCENE 0
#define ENABLE_LINUX_CONTROLLER 0
#define ENABLE_GAME_SCENE 1
#define ENABLE_FREE_CAM 0
#define GLTF_TEST 1

namespace Neptune
{
struct Track
{
    std::vector<TrackSkeletonNode> skeleton_nodes;
    std::vector<TrackSkinning>     skinning;

    std::vector<StaticMeshColliderHandle> sim_handles;
    std::vector<Reaper::SceneMesh>        scene_meshes;
};

namespace
{
#if ENABLE_GAME_SCENE
    Track create_game_track(const GenerationInfo& gen_info, Reaper::VulkanBackend& backend, PhysicsSim& sim,
                            Reaper::SceneGraph& scene, Reaper::SceneMaterialHandle material_handle)
    {
        const std::string track_mesh_path("res/model/track_chunk_simple.obj");
        const float       track_mesh_length = 10.0f;
        Reaper::Mesh      unskinned_track_mesh = Reaper::load_obj(track_mesh_path);

        Track track;

        track.skeleton_nodes.resize(gen_info.chunk_count);

        generate_track_skeleton(gen_info, track.skeleton_nodes);

        track.skinning.resize(gen_info.chunk_count);

        generate_track_skinning(track.skeleton_nodes, track.skinning);

        std::vector<glm::fmat4x3> chunk_transforms(gen_info.chunk_count);
        std::vector<Reaper::Mesh> track_meshes;

        for (u32 i = 0; i < gen_info.chunk_count; i++)
        {
            Reaper::Mesh& track_mesh = track_meshes.emplace_back(duplicate_mesh(unskinned_track_mesh));

            const TrackSkeletonNode& track_node = track.skeleton_nodes[i];

            chunk_transforms[i] = track_node.in_transform_ms_to_ws;

            skin_track_chunk_mesh(track_node, track.skinning[i], track_mesh.positions, track_mesh_length);
        }

        track.sim_handles.resize(gen_info.chunk_count);

        // NOTE: bullet will hold pointers to the original mesh data without copy
        sim_create_static_collision_meshes(track.sim_handles, sim, track_meshes, chunk_transforms);

        std::vector<Reaper::MeshHandle> chunk_mesh_handles(track_meshes.size());
        load_meshes(backend, backend.resources->mesh_cache, track_meshes, chunk_mesh_handles);

        // Place static track in the scene
        for (u32 chunk_index = 0; chunk_index < gen_info.chunk_count; chunk_index++)
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
#endif
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
        const float mag = trigger_axis * 0.5f + 0.5f;
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

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2               work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!

        ImGui::SetNextWindowPos(ImVec2(work_pos.x + 300.f, work_pos.y), ImGuiCond_FirstUseEver);
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

#if ENABLE_GAME_SCENE
    void imgui_sim_debug(Neptune::PhysicsSim& sim)
    {
        ImGui::SliderFloat("simulation_substep_duration", &sim.vars.simulation_substep_duration, 1.f / 200.f,
                           1.f / 5.f);
        ImGui::SliderInt("max_simulation_substep_count", &sim.vars.max_simulation_substep_count, 1, 10);
        ImGui::SliderFloat("gravity_force_intensity", &sim.vars.gravity_force_intensity, 0.f, 100.f, "%.3f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("linear_friction", &sim.vars.linear_friction, 0.04f, 1.f, "%.3f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("angular_friction", &sim.vars.angular_friction, 0.04f, 1.f, "%.3f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::Checkbox("enable_suspension_forces", &sim.vars.enable_suspension_forces);
        ImGui::SliderFloat("max_suspension_force", &sim.vars.max_suspension_force, 0.f, 100000.f, "%.3f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("default_spring_stiffness", &sim.vars.default_spring_stiffness, 0.0f, 100.f, "%.3f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("default_damper_friction_compression", &sim.vars.default_damper_friction_compression, 0.0f,
                           100.f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("default_damper_friction_extension", &sim.vars.default_damper_friction_extension, 0.0f,
                           100.f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::Checkbox("enable_debug_geometry", &sim.vars.enable_debug_geometry);
        ImGui::BeginDisabled();
        ImGui::SliderFloat("suspension ratio 0", &sim.raycast_suspensions[0].length_ratio_last, 0.0f, 1.f, "%.3f");
        ImGui::SliderFloat("suspension ratio 1", &sim.raycast_suspensions[1].length_ratio_last, 0.0f, 1.f, "%.3f");
        ImGui::SliderFloat("suspension ratio 2", &sim.raycast_suspensions[2].length_ratio_last, 0.0f, 1.f, "%.3f");
        ImGui::SliderFloat("suspension ratio 3", &sim.raycast_suspensions[3].length_ratio_last, 0.0f, 1.f, "%.3f");
        ImGui::EndDisabled();
        ImGui::SliderFloat("steer_force", &sim.vars.steer_force, 0.01f, 100.f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("default_ship_stats.thrust", &sim.vars.default_ship_stats.thrust, 0.f, 1000.f);
        ImGui::SliderFloat("default_ship_stats.braking", &sim.vars.default_ship_stats.braking, 0.f, 100.f);
        ImGui::SliderFloat("default_ship_stats.handling", &sim.vars.default_ship_stats.handling, 0.1f, 10.f);
    }
#endif

    void imgui_process_button_press(ImGuiIO& io, Window::MouseButton::type button, bool is_pressed)
    {
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
        case Window::MouseButton::Invalid:
            break;
        }
    }

    // FIXME This is ugly and slow
    template <typename T>
    void load_gltf_mesh_attribute(const cgltf_accessor& accessor, std::vector<T>& attribute_output)
    {
        Assert(accessor.buffer_view);

        const cgltf_buffer_view& buffer_view = *accessor.buffer_view;
        const cgltf_buffer&      buffer = *buffer_view.buffer;

        Assert(accessor.stride > 0);

        attribute_output.resize(accessor.count);

        u8* buffer_start = static_cast<u8*>(buffer.data) + buffer_view.offset + accessor.offset;

        for (u32 i = 0; i < accessor.count; i++)
        {
            attribute_output[i] = *reinterpret_cast<T*>(buffer_start + i * accessor.stride);
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

    // Load common textures used in all scenes
    std::vector<std::string> default_texture_filesnames = {
        "res/texture/default_standard_material/albedo.png",
        "res/texture/default_standard_material/metalness_roughness.png",
        "res/texture/default_standard_material/normal.png",
        "res/texture/default_standard_material/ao.png",
    };

    std::vector<u32> default_texture_srgb(default_texture_filesnames.size(), false);
    default_texture_srgb[0] = true;

    const HandleSpan<TextureHandle> default_material_handle_span = alloc_material_textures(
        backend.resources->material_resources, static_cast<u32>(default_texture_filesnames.size()));

    load_png_textures_to_staging(backend, backend.resources->material_resources, default_texture_filesnames,
                                 default_material_handle_span, default_texture_srgb);

#if GLTF_TEST
    std::string   gltf_path = "res/model/sci_fi_helmet/";
    std::string   gltf_file = gltf_path + "SciFiHelmet.gltf";
    cgltf_options options = {};
    cgltf_data*   data = nullptr;

    Assert(cgltf_parse_file(&options, gltf_file.c_str(), &data) == cgltf_result_success);
    Assert(cgltf_load_buffers(&options, data, gltf_file.c_str()) == cgltf_result_success);
    Assert(cgltf_validate(data) == cgltf_result_success);

    std::span<cgltf_image>   gltf_images(data->images, data->images_count);
    std::vector<std::string> dds_filenames(gltf_images.size());

    for (u32 i = 0; i < gltf_images.size(); i++)
    {
        dds_filenames[i] = gltf_path + gltf_images[i].uri;
    }

    const HandleSpan<TextureHandle> dds_handle_span =
        alloc_material_textures(backend.resources->material_resources, static_cast<u32>(dds_filenames.size()));

    std::span<cgltf_material> gltf_materials(data->materials, data->materials_count);

    const HandleSpan<SceneMaterialHandle> scene_materials =
        alloc_scene_materials(scene, static_cast<u32>(data->materials_count));

    for (u32 i = 0; i < scene_materials.count; i++)
    {
        const cgltf_material& gltf_material = gltf_materials[i];

        auto      base_color_image = gltf_material.pbr_metallic_roughness.base_color_texture.texture->image;
        const u64 base_color_offset = base_color_image - data->images;
        auto metallic_roughness_image = gltf_material.pbr_metallic_roughness.metallic_roughness_texture.texture->image;
        const u64 metallic_roughness_offset = metallic_roughness_image - data->images;
        auto      normal_image = gltf_material.normal_texture.texture->image;
        const u64 normal_offset = normal_image - data->images;
        auto      ao_image = gltf_material.occlusion_texture.texture->image;
        const u64 ao_offset = ao_image - data->images;

        scene.scene_materials[scene_materials.offset + i] = SceneMaterial{
            .base_color_texture = TextureHandle(dds_handle_span.offset + base_color_offset),
            .metal_roughness_texture = TextureHandle(dds_handle_span.offset + metallic_roughness_offset),
            .normal_map_texture = TextureHandle(dds_handle_span.offset + normal_offset),
            .ao_texture = TextureHandle(dds_handle_span.offset + ao_offset),
        };
    }

    std::span<cgltf_mesh> gltf_meshes(data->meshes, data->meshes_count);
    std::vector<Mesh>     meshes(data->meshes_count);
    std::vector<u32>      mesh_gltf_material_handles(
        data->meshes_count); // FIXME Should be per primitive but we assume 1 prim per mesh

    for (u32 i = 0; i < meshes.size(); i++)
    {
        const cgltf_mesh& gltf_mesh = gltf_meshes[i];

        // FIXME Assume meshes only contain one primitive
        Assert(gltf_mesh.primitives_count == 1);
        const cgltf_primitive& gltf_primitive = *gltf_mesh.primitives;
        Assert(gltf_primitive.type == cgltf_primitive_type_triangles);
        Assert(gltf_primitive.indices != nullptr);
        Assert(gltf_primitive.attributes_count >= 4); // Need pos, uv, normals, tangents at minimum
        Assert(!gltf_primitive.has_draco_mesh_compression);

        // Record material handle local to the GLTF file
        const u64 material_index =
            (gltf_primitive.material - data->materials); // FIXME ptr arithmetic is kinda sad here
        mesh_gltf_material_handles[i] = static_cast<u32>(material_index);

        std::span<cgltf_attribute> attributes(gltf_primitive.attributes,
                                              static_cast<u32>(gltf_primitive.attributes_count));

        auto& mesh = meshes[i];

        Assert(gltf_primitive.indices->type == cgltf_type_scalar);
        Assert(gltf_primitive.indices->component_type == cgltf_component_type_r_32u);
        load_gltf_mesh_attribute(*gltf_primitive.indices, mesh.indexes);

        for (u32 j = 0; j < attributes.size(); j++)
        {
            const cgltf_attribute& gltf_attribute = attributes[j];
            const cgltf_accessor&  attribute_data = *gltf_attribute.data;

            if (gltf_attribute.index != 0)
                continue; // FIXME

            Assert(attribute_data.component_type == cgltf_component_type_r_32f);
            switch (gltf_attribute.type)
            {
            case cgltf_attribute_type_position: {
                Assert(attribute_data.type == cgltf_type_vec3);
                load_gltf_mesh_attribute(attribute_data, mesh.positions);
                break;
            }
            case cgltf_attribute_type_normal: {
                Assert(attribute_data.type == cgltf_type_vec3);
                // NOTE: Not necessary normalized;
                load_gltf_mesh_attribute(attribute_data, mesh.normals);
                break;
            }
            case cgltf_attribute_type_tangent: {
                Assert(attribute_data.type == cgltf_type_vec4);
                // NOTE: Not necessary normalized;
                load_gltf_mesh_attribute(attribute_data, mesh.tangents);
                break;
            }
            case cgltf_attribute_type_texcoord: {
                Assert(attribute_data.type == cgltf_type_vec2);
                load_gltf_mesh_attribute(attribute_data, mesh.uvs);
                break;
            }
            default:
                // FIXME Simply ignore the stream
                break;
            }
        }
    }

    std::vector<MeshHandle> gltf_mesh_handles(data->meshes_count);
    load_meshes(backend, backend.resources->mesh_cache, meshes, gltf_mesh_handles);

    cgltf_free(data);
#endif

    load_dds_textures_to_staging(backend, backend.resources->material_resources, dds_filenames, dds_handle_span);

#if ENABLE_TEST_SCENE
    // scene = create_test_scene_tiled_lighting(backend);
    scene = create_static_test_scene(backend);
#endif

#if ENABLE_GAME_SCENE
    Neptune::GenerationInfo track_gen_info = {};
    track_gen_info.chunk_count = 100;
    track_gen_info.radius_min_meter = 300.f;
    track_gen_info.radius_max_meter = 600.f;
    track_gen_info.chaos = 0.4f;

    const SceneMaterialHandle default_material_handle = alloc_scene_material(scene);
    scene.scene_materials[default_material_handle] = SceneMaterial{
        .base_color_texture = TextureHandle(default_material_handle_span.offset + 0),
        .metal_roughness_texture = TextureHandle(default_material_handle_span.offset + 1),
        .normal_map_texture = TextureHandle(default_material_handle_span.offset + 2),
        .ao_texture = TextureHandle(default_material_handle_span.offset + 3),
    };

    Neptune::Track game_track =
        Neptune::create_game_track(track_gen_info, backend, sim, scene, default_material_handle);

    const glm::fmat4x3 player_initial_transform = glm::translate(glm::mat4(1.0f), glm::vec3(1.1f, 0.8f, 0.f));
    Neptune::sim_create_player_rigid_body(sim, player_initial_transform);

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
                glm::rotate(glm::scale(glm::identity<glm::fmat4>(), glm::vec3(0.4f)), glm::pi<float>() * -0.5f, up_ws);

            player_scene_mesh.scene_node = create_scene_node(scene, mesh_local_transform, player_scene_node);
            player_scene_mesh.mesh_handle = gltf_mesh_handles[0];
            player_scene_mesh.material_handle = SceneMaterialHandle(scene_materials.offset + 0);
        }

        // Add lights
        const glm::vec3 light_target_ws = glm::vec3(0.f, 0.f, 0.f);

        {
            const glm::vec3    light_position_ws = glm::vec3(-1.f, 0.f, 0.f);
            const glm::fmat4x3 light_transform = glm::translate(glm::mat4(1.0f), glm::vec3(2.f, 0.f, 0.f))
                                                 * glm::inverse(glm::lookAt(light_position_ws, light_target_ws, up_ws));

            SceneLight& light = scene.scene_lights.emplace_back();
            light.color = glm::fvec3(0.03f, 0.21f, 0.61f);
            light.intensity = 20.f;
            light.radius = 42.f;
            light.scene_node = create_scene_node(scene, light_transform, player_scene_node);
            light.shadow_map_size = glm::uvec2(1024, 1024);
        }

        {
            const glm::fvec3   light_position_ws = glm::vec3(3.f, 3.f, 3.f);
            const glm::fmat4x3 light_transform = glm::inverse(glm::lookAt(light_position_ws, light_target_ws, up_ws));

            SceneLight& light = scene.scene_lights.emplace_back();
            light.color = glm::fvec3(1.f, 1.f, 1.f);
            light.intensity = 16.f;
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
                else if (event.type == Window::EventType::MouseButton)
                {
                    Window::Event::Message::MouseButton mouse_button = event.message.mouse_button;
                    Window::MouseButton::type           button = mouse_button.button;
                    bool                                press = mouse_button.press;

                    log_debug(root, "window: button index = {}, press = {}", Window::get_mouse_button_string(button),
                              press);

                    imgui_process_button_press(io, button, press);
                }
                else if (event.type == Window::EventType::MouseWheel)
                {
                    Window::Event::Message::MouseWheel mouse_wheel = event.message.mouse_wheel;

                    log_debug(root, "window: mouse wheel event, x = {}, y = {}", mouse_wheel.x_delta,
                              mouse_wheel.y_delta);

                    constexpr float ImGuiScrollMultiplier = 0.5f;

                    io.AddMouseWheelEvent(static_cast<float>(mouse_wheel.x_delta) * ImGuiScrollMultiplier,
                                          static_cast<float>(mouse_wheel.y_delta) * ImGuiScrollMultiplier);
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
                        log_warning(root, "window: key with unknown key_code '{}' {} detected",
                                    keypress.internal_key_code, is_pressed ? "press" : "release");
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
        input.throttle = controller_state.axes[GenericAxis::RT] * 0.5f + 0.5f;
        input.brake = controller_state.axes[GenericAxis::LT] * 0.5f + 0.5f;
        input.steer = controller_state.axes[GenericAxis::RSX];

        Neptune::sim_update(sim, game_track.skeleton_nodes, input, time_delta_secs);

        const glm::fmat4x3 player_transform = Neptune::get_player_transform(sim);
        glm::fvec3         player_translation = player_transform[3];

        player_scene_node->transform_matrix = player_transform;

        {
            constexpr u32 length_min = 1;
            constexpr u32 length_max = 100;

            static bool          show_window = true;
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImVec2               work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!

            ImGui::SetNextWindowPos(ImVec2(work_pos.x, work_pos.y + 300.f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

            if (ImGui::Begin("Physics", &show_window))
            {
                ImGui::InputFloat3("ship position", &player_translation[0], "%.3f", ImGuiInputTextFlags_ReadOnly);
                ImGui::Separator();
                ImGui::SliderScalar("chunk_count", ImGuiDataType_U32, &track_gen_info.chunk_count, &length_min,
                                    &length_max, "%u");
                ImGui::SliderFloat("radius_min_meter", &track_gen_info.radius_min_meter, 100.f, 2000.f);
                ImGui::SliderFloat("radius_max_meter", &track_gen_info.radius_max_meter, 100.f, 2000.f);
                ImGui::SliderFloat("chaos", &track_gen_info.chaos, 0.f, 1.f);

                if (ImGui::Button("Generate new track"))
                {
                    Neptune::destroy_game_track(game_track, backend, sim, scene);

                    // We don't handle fragmentation yet, so we recreate ALL renderer meshes
                    // FIXME since GLTF was added we don't do this properly anymore
                    clear_meshes(backend.resources->mesh_cache);

                    game_track =
                        Neptune::create_game_track(track_gen_info, backend, sim, scene, default_material_handle);
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

        std::vector<DebugGeometryUserCommand> debug_draw_commands;

        if (sim.vars.enable_debug_geometry)
        {
            debug_draw_commands.push_back(
                create_debug_command_box(player_transform, sim.consts.player_shape_half_extent, 0x000000FF));

            for (auto& suspension : sim.raycast_suspensions)
            {
                debug_draw_commands.push_back(create_debug_command_sphere(
                    glm::translate(glm::mat4(1.0f), suspension.position_start_ws), 0.05f, 0x000000FF));
                debug_draw_commands.push_back(create_debug_command_sphere(
                    glm::translate(glm::mat4(1.0f), suspension.position_end_ws), 0.05f, 0x000000FF));
                debug_draw_commands.push_back(create_debug_command_sphere(
                    glm::translate(glm::mat4(1.0f), glm::mix(suspension.position_start_ws, suspension.position_end_ws,
                                                             suspension.length_ratio_last)),
                    0.05f, 0x00FF00FF));
            }

            debug_draw_commands.push_back(create_debug_command_sphere(
                glm::translate(glm::mat4(1.0f), player_translation) * glm::fmat4(sim.last_gravity_frame), 1.f,
                0x00FF00FF));
            debug_draw_commands.push_back(create_debug_command_sphere(
                glm::translate(glm::mat4(1.0f), player_translation) * glm::fmat4(sim.last_gravity_frame), 0.95f,
                0x0000FFFF));
        }

        renderer_execute_frame(root, scene, audio_backend.audio_buffer, debug_draw_commands);

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

        Audio::write_wav(output_file, audio_backend.audio_buffer.data(),
                         static_cast<u32>(audio_backend.audio_buffer.size()), BitsPerChannel, SampleRate);

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
