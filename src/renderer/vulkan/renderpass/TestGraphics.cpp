////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TestGraphics.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/BackendResources.h"
#include "renderer/vulkan/Barrier.h"
#include "renderer/vulkan/CommandBuffer.h"
#include "renderer/vulkan/Debug.h"
#include "renderer/vulkan/DescriptorSet.h"
#include "renderer/vulkan/FrameSync.h"
#include "renderer/vulkan/GpuProfile.h"
#include "renderer/vulkan/MaterialResources.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/Swapchain.h"
#include "renderer/vulkan/api/AssertHelper.h"
#include "renderer/vulkan/api/VulkanStringConversion.h"
#include "renderer/vulkan/renderpass/Constants.h"
#include "renderer/vulkan/renderpass/ForwardPassConstants.h"
#include "renderer/vulkan/renderpass/FrameGraphPass.h"
#include "renderer/vulkan/renderpass/HZBPass.h"
#include "renderer/vulkan/renderpass/TiledLightingCommon.h"
#include "renderer/vulkan/renderpass/VisibilityBufferConstants.h"

#include "renderer/Mesh2.h"
#include "renderer/texture/GPUTextureProperties.h"
#include "renderer/texture/GPUTextureView.h"

#include "renderer/graph/FrameGraphBuilder.h"
#include "renderer/graph/GraphDebug.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/BitTricks.h"
#include "core/memory/Allocator.h"
#include "profiling/Scope.h"

#include "renderer/shader/debug_geometry/debug_geometry_private.share.hlsl"
#include "renderer/shader/histogram/reduce_histogram.share.hlsl"
#include "renderer/shader/tiled_lighting/tiled_lighting.share.hlsl"

#include <vulkan_loader/Vulkan.h>

#include <array>

#include <imgui.h>

namespace Reaper
{
namespace
{
    void log_barriers(ReaperRoot& root, const FrameGraph::FrameGraph& framegraph,
                      const FrameGraph::FrameGraphSchedule& schedule)
    {
        using namespace FrameGraph;

        log_debug(root, "framegraph: scheduling {} barrier events", schedule.barrier_events.size());

        log_debug(root, "framegraph: schedule has {} barriers", schedule.barriers.size());
        for (auto event : schedule.barrier_events)
        {
            const char* render_pass_name = framegraph.RenderPasses[event.render_pass_handle].debug_name;

            const Barrier&       barrier = schedule.barriers[event.barrier_handle];
            const ResourceUsage& src_usage = framegraph.ResourceUsages[barrier.src.usage_handle];
            const ResourceHandle resource_handle = src_usage.resource_handle;
            const Resource&      resource = GetResource(framegraph, src_usage);

            const char* resource_name = resource.debug_name;
            log_debug(root, "framegraph: pass '{}', resource '{}', barrier type = '{}'", render_pass_name,
                      resource_name, BarrierType::to_string(event.barrier_type));
            if (resource_handle.is_texture)
            {
                log_debug(root, "    - src layout = '{}', dst layout = '{}'",
                          vk_to_string(barrier.src.access.image_layout), vk_to_string(barrier.dst.access.image_layout));
            }
        }
    }
} // namespace

void resize_swapchain(ReaperRoot& root, VulkanBackend& backend)
{
    REAPER_PROFILE_SCOPE_FUNC();

    // Resize swapchain if necessary
    if (backend.new_swapchain_extent.width != 0 || backend.new_swapchain_extent.height != 0)
    {
        vkQueueWaitIdle(backend.present_queue); // FIXME

        Assert(backend.new_swapchain_extent.height > 0);
        resize_vulkan_wm_swapchain(root, backend, backend.presentInfo, backend.new_swapchain_extent);

        set_backend_render_resolution(backend);

        backend.new_swapchain_extent = {.width = 0, .height = 0};
    }
}

void backend_debug_ui(VulkanBackend& backend)
{
    static bool show_app_simple_overlay = true;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
                                    | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
                                    | ImGuiWindowFlags_NoNav;

    const float          pad = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2               work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
    ImGui::SetNextWindowPos(ImVec2(work_pos.x + pad, work_pos.y + pad), ImGuiCond_Always);
    window_flags |= ImGuiWindowFlags_NoMove;

    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
    if (ImGui::Begin("Example: Simple overlay", &show_app_simple_overlay, window_flags))
    {
        ImGui::Checkbox("Freeze culling [BROKEN]", &backend.options.freeze_meshlet_culling); // FIXME
        ImGui::Checkbox("Enable debug tile culling", &backend.options.enable_debug_tile_lighting);
    }
    ImGui::End();
}

void backend_execute_frame(ReaperRoot& root, VulkanBackend& backend, CommandBuffer& cmdBuffer,
                           const PreparedData& prepared, const TiledLightingFrame& tiled_lighting_frame,
                           BackendResources& resources, ImDrawData* imgui_draw_data)
{
    VkResult acquireResult;
    u64      acquireTimeoutUs = 1000000000;
    uint32_t current_swapchain_index;

    constexpr u32 MaxAcquireTryCount = 10;

    for (u32 acquireTryCount = 0; acquireTryCount < MaxAcquireTryCount; acquireTryCount++)
    {
        log_debug(root, "vulkan: acquiring frame try #{}", acquireTryCount);
        acquireResult = vkAcquireNextImageKHR(backend.device, backend.presentInfo.swapchain, acquireTimeoutUs,
                                              backend.semaphore_swapchain_image_available, VK_NULL_HANDLE,
                                              &current_swapchain_index);

        if (acquireResult != VK_NOT_READY)
            break;
    }

    switch (acquireResult)
    {
    case VK_NOT_READY: // Should be good after X tries
        log_debug(root, "vulkan: vkAcquireNextImageKHR not ready");
        break;
    case VK_SUBOPTIMAL_KHR:
        log_debug(root, "vulkan: vkAcquireNextImageKHR suboptimal");
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        log_warning(root, "vulkan: vkAcquireNextImageKHR out of date");
        return;
    default:
        Assert(acquireResult == VK_SUCCESS);
        break;
    }

    log_debug(root, "vulkan: swapchain image index = {}", current_swapchain_index);

    {
        VkFence draw_fence = resources.frame_sync_resources.draw_fence;

        VkResult waitResult;
        log_debug(root, "vulkan: wait for fence");

        do
        {
            REAPER_PROFILE_SCOPE_COLOR("Wait for fence", Color::Red);

            const u64 waitTimeoutNs = 1 * 1000 * 1000 * 1000;
            waitResult = vkWaitForFences(backend.device, 1, &draw_fence, VK_TRUE, waitTimeoutNs);

            if (waitResult != VK_SUCCESS)
            {
                log_debug(root, "- return result {}", vk_to_string(waitResult));
            }
        } while (waitResult != VK_SUCCESS);

#if defined(REAPER_USE_TRACY)
        FrameMark;
#endif

        AssertVk(vkGetFenceStatus(backend.device, draw_fence));

        log_debug(root, "vulkan: reset fence");
        AssertVk(vkResetFences(backend.device, 1, &draw_fence));
    }

    const VkExtent2D render_extent = backend.render_extent;

    FrameGraph::FrameGraph framegraph;

    using namespace FrameGraph;

    Builder builder(framegraph);

    const CullMeshletsFrameGraphRecord meshlet_pass = create_cull_meshlet_frame_graph_record(builder);

    DebugGeometryClearFrameGraphRecord debug_geometry_clear;
    debug_geometry_clear.pass_handle = builder.create_render_pass("Debug Geometry Clear");

    const GPUBufferProperties debug_geometry_counter_properties = DefaultGPUBufferProperties(
        1, sizeof(u32), GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst);

    debug_geometry_clear.draw_counter = builder.create_buffer(
        debug_geometry_clear.pass_handle, "Debug Indirect draw counter buffer", debug_geometry_counter_properties,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    const GPUBufferProperties debug_geometry_user_commands_properties = DefaultGPUBufferProperties(
        DebugGeometryCountMax, sizeof(DebugGeometryUserCommand), GPUBufferUsage::StorageBuffer);

    // Technically we shouldn't create an usage here, the first client of the debug geometry API should call
    // create_buffer() with the right data. But it makes it slightly simpler this way for the user API so I'm taking
    // the trade-off and paying for an extra useless barrier.
    debug_geometry_clear.user_commands_buffer = builder.create_buffer(
        debug_geometry_clear.pass_handle, "Debug geometry user command buffer", debug_geometry_user_commands_properties,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    const ShadowFrameGraphRecord shadow = create_shadow_map_pass_record(builder, meshlet_pass, prepared);

    const VisBufferFrameGraphRecord vis_buffer_record =
        create_vis_buffer_pass_record(builder, meshlet_pass, render_extent);

    const HZBReduceFrameGraphRecord hzb_reduce =
        create_hzb_pass_record(builder, vis_buffer_record.render.depth, tiled_lighting_frame);

    const LightRasterFrameGraphRecord light_raster_record = create_tiled_lighting_raster_pass_record(
        builder, tiled_lighting_frame, hzb_reduce.hzb_properties, hzb_reduce.hzb_texture);

    const TiledLightingFrameGraphRecord tiled_lighting =
        create_tiled_lighting_pass_record(builder, vis_buffer_record, shadow, light_raster_record);

    const TiledLightingDebugFrameGraphRecord tiled_lighting_debug_record =
        create_tiled_lighting_debug_pass_record(builder, tiled_lighting, render_extent);

    ForwardFrameGraphRecord forward;
    forward.pass_handle = builder.create_render_pass("Forward");

    forward.scene_hdr = builder.create_texture(
        forward.pass_handle, "Scene HDR",
        default_texture_properties(render_extent.width, render_extent.height, ForwardHDRColorFormat,
                                   GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled),
        GPUTextureAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL});

    forward.depth = builder.write_texture(forward.pass_handle, vis_buffer_record.render.depth,
                                          GPUTextureAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                           VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL});

    for (auto shadow_map_usage_handle : shadow.shadow_maps)
    {
        forward.shadow_maps.push_back(
            builder.read_texture(forward.pass_handle, shadow_map_usage_handle,
                                 GPUTextureAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL}));
    }

    forward.meshlet_counters = builder.read_buffer(
        forward.pass_handle, meshlet_pass.cull_triangles.meshlet_counters,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    forward.meshlet_indirect_draw_commands = builder.read_buffer(
        forward.pass_handle, meshlet_pass.cull_triangles.meshlet_indirect_draw_commands,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    forward.meshlet_visible_index_buffer =
        builder.read_buffer(forward.pass_handle, meshlet_pass.cull_triangles.meshlet_visible_index_buffer,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT});

    forward.visible_meshlet_buffer =
        builder.read_buffer(forward.pass_handle, meshlet_pass.cull_triangles.visible_meshlet_buffer,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    GUIFrameGraphRecord gui;
    gui.pass_handle = builder.create_render_pass("GUI");

    gui.output = builder.create_texture(
        gui.pass_handle, "GUI SDR",

        default_texture_properties(backend.presentInfo.surface_extent.width, backend.presentInfo.surface_extent.height,
                                   GUIFormat, GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled),
        GPUTextureAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL});

    HistogramClearFrameGraphRecord histogram_clear;
    histogram_clear.pass_handle = builder.create_render_pass("Histogram Clear");

    const GPUBufferProperties histogram_buffer_properties = DefaultGPUBufferProperties(
        HistogramRes, sizeof(u32), GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst);

    histogram_clear.histogram_buffer =
        builder.create_buffer(histogram_clear.pass_handle, "Histogram Buffer", histogram_buffer_properties,
                              GPUBufferAccess{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});

    HistogramFrameGraphRecord histogram;
    histogram.pass_handle = builder.create_render_pass("Histogram");

    histogram.scene_hdr =
        builder.read_texture(histogram.pass_handle, forward.scene_hdr,
                             GPUTextureAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

    histogram.histogram_buffer =
        builder.write_buffer(histogram.pass_handle, histogram_clear.histogram_buffer,
                             GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                             VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT});

    DebugGeometryComputeFrameGraphRecord debug_geometry_build_cmds;
    debug_geometry_build_cmds.pass_handle = builder.create_render_pass("Debug Geometry Build Commands");

    debug_geometry_build_cmds.draw_counter =
        builder.read_buffer(debug_geometry_build_cmds.pass_handle, debug_geometry_clear.draw_counter,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    debug_geometry_build_cmds.user_commands_buffer =
        builder.read_buffer(debug_geometry_build_cmds.pass_handle, debug_geometry_clear.user_commands_buffer,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    const GPUBufferProperties debug_geometry_command_properties =
        DefaultGPUBufferProperties(DebugGeometryCountMax, sizeof(VkDrawIndexedIndirectCommand),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer);

    debug_geometry_build_cmds.draw_commands = builder.create_buffer(
        debug_geometry_build_cmds.pass_handle, "Debug Indirect draw command buffer", debug_geometry_command_properties,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    const GPUBufferProperties debug_geometry_instance_properties =
        DefaultGPUBufferProperties(DebugGeometryCountMax, sizeof(DebugGeometryInstance), GPUBufferUsage::StorageBuffer);

    debug_geometry_build_cmds.instance_buffer = builder.create_buffer(
        debug_geometry_build_cmds.pass_handle, "Debug geometry instance buffer", debug_geometry_instance_properties,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT});

    // Debug geometry draw
    DebugGeometryDrawFrameGraphRecord debug_geometry_draw;
    debug_geometry_draw.pass_handle = builder.create_render_pass("Debug Geometry Draw");

    debug_geometry_draw.scene_hdr = builder.write_texture(
        debug_geometry_draw.pass_handle,
        tiled_lighting.lighting,
        GPUTextureAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});

    debug_geometry_draw.scene_depth = builder.read_texture(
        debug_geometry_draw.pass_handle, forward.depth,
        GPUTextureAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                         VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL});

    debug_geometry_draw.draw_counter = builder.read_buffer(
        debug_geometry_draw.pass_handle, debug_geometry_clear.draw_counter,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    debug_geometry_draw.draw_commands = builder.read_buffer(
        debug_geometry_draw.pass_handle, debug_geometry_build_cmds.draw_commands,
        GPUBufferAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});

    debug_geometry_draw.instance_buffer =
        builder.read_buffer(debug_geometry_draw.pass_handle, debug_geometry_build_cmds.instance_buffer,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    SwapchainFrameGraphRecord swapchain;
    swapchain.pass_handle = builder.create_render_pass("Swapchain", true);

    swapchain.scene_hdr = builder.read_texture(swapchain.pass_handle, forward.scene_hdr,
                                               GPUTextureAccess{.stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                                .access_mask = VK_ACCESS_2_SHADER_READ_BIT,
                                                                .image_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

    swapchain.lighting_result =
        builder.read_texture(swapchain.pass_handle, debug_geometry_draw.scene_hdr,
                             GPUTextureAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

    swapchain.gui =
        builder.read_texture(swapchain.pass_handle, gui.output,
                             GPUTextureAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

    swapchain.histogram =
        builder.read_buffer(swapchain.pass_handle, histogram.histogram_buffer,
                            GPUBufferAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});

    swapchain.tile_debug =
        builder.read_texture(swapchain.pass_handle, tiled_lighting_debug_record.output,
                             GPUTextureAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});

    const AudioFrameGraphRecord audio_pass = create_audio_frame_graph_data(builder);

    builder.build();

    // DumpFrameGraph(framegraph);

    const FrameGraphSchedule schedule = compute_schedule(framegraph);

    log_barriers(root, framegraph, schedule);

    allocate_framegraph_volatile_resources(backend, resources.framegraph_resources, framegraph);

    DescriptorWriteHelper descriptor_write_helper(200, 200);

    update_shadow_map_resources(descriptor_write_helper, resources.frame_storage_allocator, prepared,
                                resources.shadow_map_resources, resources.mesh_cache.vertexBufferPosition);

    update_vis_buffer_pass_resources(framegraph, resources.framegraph_resources, vis_buffer_record,
                                     descriptor_write_helper, resources.frame_storage_allocator,
                                     resources.vis_buffer_pass_resources, prepared, resources.samplers_resources,
                                     resources.material_resources, resources.mesh_cache);

    update_tiled_lighting_raster_pass_resources(framegraph, resources.framegraph_resources, light_raster_record,
                                                descriptor_write_helper, resources.frame_storage_allocator,
                                                resources.tiled_raster_resources, tiled_lighting_frame);

    upload_lighting_pass_frame_resources(resources.frame_storage_allocator, prepared, resources.lighting_resources);
    upload_tiled_lighting_pass_frame_resources(backend, prepared, resources.tiled_lighting_resources);
    upload_forward_pass_frame_resources(backend, prepared, resources.forward_pass_resources);
    upload_debug_geometry_build_cmds_pass_frame_resources(backend, prepared, resources.debug_geometry_resources);
    upload_audio_frame_resources(backend, prepared, resources.audio_resources);

    update_meshlet_culling_passes_resources(framegraph, resources.framegraph_resources, meshlet_pass,
                                            descriptor_write_helper, resources.frame_storage_allocator, prepared,
                                            resources.meshlet_culling_resources, resources.mesh_cache);

    update_hzb_pass_descriptor_set(
        descriptor_write_helper, resources.hzb_pass_resources, resources.samplers_resources,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, hzb_reduce.depth),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, hzb_reduce.hzb_texture));

    std::vector<FrameGraphTexture> forward_shadow_map_views;
    for (auto handle : forward.shadow_maps)
    {
        forward_shadow_map_views.emplace_back(
            get_frame_graph_texture(resources.framegraph_resources, framegraph, handle));
    }

    update_forward_pass_descriptor_sets(
        descriptor_write_helper, resources.forward_pass_resources,
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, forward.visible_meshlet_buffer),
        resources.samplers_resources, resources.material_resources, resources.mesh_cache, resources.lighting_resources,
        forward_shadow_map_views);

    std::vector<FrameGraphTexture> tiled_shadow_maps;
    for (auto handle : tiled_lighting.shadow_maps)
    {
        tiled_shadow_maps.emplace_back(get_frame_graph_texture(resources.framegraph_resources, framegraph, handle));
    }

    update_tiled_lighting_pass_descriptor_sets(
        descriptor_write_helper, resources.lighting_resources, resources.tiled_lighting_resources,
        resources.samplers_resources,
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, tiled_lighting.light_list),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tiled_lighting.gbuffer_rt0),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tiled_lighting.gbuffer_rt1),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tiled_lighting.depth),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tiled_lighting.lighting),
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, tiled_lighting.tile_debug_texture),
        tiled_shadow_maps);

    update_tiled_lighting_debug_pass_descriptor_sets(
        descriptor_write_helper, resources.tiled_lighting_resources,
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, tiled_lighting_debug_record.tile_debug),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tiled_lighting_debug_record.output));

    update_histogram_pass_descriptor_set(
        descriptor_write_helper, resources.histogram_pass_resources, resources.samplers_resources,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, histogram.scene_hdr),
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, histogram.histogram_buffer));

    update_debug_geometry_build_cmds_pass_descriptor_sets(
        descriptor_write_helper, resources.debug_geometry_resources,
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, debug_geometry_build_cmds.draw_counter),
        get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                               debug_geometry_build_cmds.user_commands_buffer),
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, debug_geometry_build_cmds.draw_commands),
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, debug_geometry_build_cmds.instance_buffer));

    update_debug_geometry_draw_pass_descriptor_sets(
        descriptor_write_helper, resources.debug_geometry_resources,
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, debug_geometry_draw.instance_buffer));

    update_swapchain_pass_descriptor_set(
        descriptor_write_helper, resources.swapchain_pass_resources, resources.samplers_resources,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, swapchain.scene_hdr),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, swapchain.lighting_result),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, swapchain.gui),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, swapchain.tile_debug));

    update_audio_render_descriptor_set(
        descriptor_write_helper, resources.audio_resources,
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, audio_pass.render.audio_buffer));

    storage_allocator_commit_to_gpu(backend, resources.frame_storage_allocator);

    descriptor_write_helper.flush_descriptor_write_helper(backend.device);

    const GPUTextureAccess swapchain_access_initial = {.stage_mask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                                       .access_mask = VK_ACCESS_2_NONE,
                                                       .image_layout = VK_IMAGE_LAYOUT_UNDEFINED};

    const GPUTextureAccess swapchain_access_present = {.stage_mask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                                       .access_mask = VK_ACCESS_2_NONE,
                                                       .image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

    const GPUTextureAccess swapchain_access_render = {.stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                      .access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                      .image_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL};

    log_debug(root, "vulkan: record command buffer");
    AssertVk(vkResetCommandPool(backend.device, backend.resources->gfxCommandPool, VK_FLAGS_NONE));

    const VkCommandBufferBeginInfo cmdBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    AssertVk(vkBeginCommandBuffer(cmdBuffer.handle, &cmdBufferBeginInfo));

    {
        REAPER_GPU_SCOPE(cmdBuffer, "GPU Frame");

        if (backend.presentInfo.queue_swapchain_transition)
        {
            REAPER_GPU_SCOPE(cmdBuffer, "Barrier");

            std::vector<VkImageMemoryBarrier2> imageBarriers;

            for (u32 swapchainImageIndex = 0; swapchainImageIndex < static_cast<u32>(backend.presentInfo.images.size());
                 swapchainImageIndex++)
            {
                const GPUTextureAccess src = swapchain_access_initial;
                const GPUTextureAccess dst = (swapchainImageIndex == current_swapchain_index)
                                                 ? swapchain_access_render
                                                 : swapchain_access_present;

                const GPUTextureSubresource subresource = default_texture_subresource_one_color_mip();

                imageBarriers.emplace_back(
                    get_vk_image_barrier(backend.presentInfo.images[swapchainImageIndex], subresource, src, dst));
            }

            const VkDependencyInfo dependencies = get_vk_image_barrier_depency_info(imageBarriers);

            vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);

            backend.presentInfo.queue_swapchain_transition = false;
        }
        else
        {
            const VkImageMemoryBarrier2 barrier = get_vk_image_barrier(
                backend.presentInfo.images[current_swapchain_index], default_texture_subresource_one_color_mip(),
                swapchain_access_present, swapchain_access_render);

            const VkDependencyInfo dependencies = get_vk_image_barrier_depency_info(std::span(&barrier, 1));

            vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Material Upload");
            record_material_upload_command_buffer(resources.material_resources.staging, cmdBuffer);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Meshlet Culling Clear");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       meshlet_pass.clear.pass_handle, true);

            FrameGraphBuffer meshlet_counters =
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, meshlet_pass.clear.meshlet_counters);

            const u32 clear_value = 0;
            vkCmdFillBuffer(cmdBuffer.handle, meshlet_counters.handle, 0, VK_WHOLE_SIZE, clear_value);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       meshlet_pass.clear.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Cull Meshlets");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       meshlet_pass.cull_meshlets.pass_handle, true);

            record_meshlet_culling_command_buffer(root, cmdBuffer, prepared, resources.meshlet_culling_resources);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       meshlet_pass.cull_meshlets.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Cull Meshlet Triangles Prepare");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       meshlet_pass.cull_triangles_prepare.pass_handle, true);

            record_triangle_culling_prepare_command_buffer(cmdBuffer, prepared, resources.meshlet_culling_resources);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       meshlet_pass.cull_triangles_prepare.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Cull Meshlet Triangles");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       meshlet_pass.cull_triangles.pass_handle, true);

            record_triangle_culling_command_buffer(
                cmdBuffer, prepared, resources.meshlet_culling_resources,
                get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                       meshlet_pass.cull_triangles.indirect_dispatch_buffer));

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       meshlet_pass.cull_triangles.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Meshlet Debug");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       meshlet_pass.debug.pass_handle, true);

            record_meshlet_culling_debug_command_buffer(cmdBuffer, resources.meshlet_culling_resources,
                                                        get_frame_graph_buffer(resources.framegraph_resources,
                                                                               framegraph,
                                                                               meshlet_pass.debug.meshlet_counters));

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       meshlet_pass.debug.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Debug Geometry Clear");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       debug_geometry_clear.pass_handle, true);

            const u32        clear_value = 0;
            FrameGraphBuffer draw_counter =
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, debug_geometry_clear.draw_counter);

            vkCmdFillBuffer(cmdBuffer.handle, draw_counter.handle, draw_counter.default_view.offset_bytes,
                            draw_counter.default_view.size_bytes, clear_value);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       debug_geometry_clear.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Shadow");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       shadow.pass_handle, true);

            std::vector<FrameGraphTexture> shadow_maps;
            for (auto handle : shadow.shadow_maps)
            {
                shadow_maps.emplace_back(get_frame_graph_texture(resources.framegraph_resources, framegraph, handle));
            }

            record_shadow_map_command_buffer(
                cmdBuffer, prepared, resources.shadow_map_resources, shadow_maps,
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, shadow.meshlet_counters),
                get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                       shadow.meshlet_indirect_draw_commands),
                get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                       shadow.meshlet_visible_index_buffer));

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       shadow.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Visibility");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       vis_buffer_record.render.pass_handle, true);

            record_vis_buffer_pass_command_buffer(
                cmdBuffer, prepared, resources.vis_buffer_pass_resources,
                get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                       vis_buffer_record.render.meshlet_counters),
                get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                       vis_buffer_record.render.meshlet_indirect_draw_commands),
                get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                       vis_buffer_record.render.meshlet_visible_index_buffer),
                get_frame_graph_texture(resources.framegraph_resources, framegraph,
                                        vis_buffer_record.render.vis_buffer),
                get_frame_graph_texture(resources.framegraph_resources, framegraph, vis_buffer_record.render.depth));

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       vis_buffer_record.render.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "HZB Reduce");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       hzb_reduce.pass_handle, true);

            record_hzb_command_buffer(
                cmdBuffer,
                resources.hzb_pass_resources,
                VkExtent2D{.width = vis_buffer_record.scene_depth_properties.width,
                           .height = vis_buffer_record.scene_depth_properties.height},
                VkExtent2D{.width = hzb_reduce.hzb_properties.width, .height = hzb_reduce.hzb_properties.height});

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       hzb_reduce.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Visibility Fill GBuffer");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       vis_buffer_record.fill_gbuffer.pass_handle, true);

            record_fill_gbuffer_pass_command_buffer(cmdBuffer, resources.vis_buffer_pass_resources, render_extent);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       vis_buffer_record.fill_gbuffer.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Tile Depth Copy");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       light_raster_record.tile_depth_copy.pass_handle, true);

            record_depth_copy(cmdBuffer, resources.tiled_raster_resources,
                              get_frame_graph_texture(resources.framegraph_resources, framegraph,
                                                      light_raster_record.tile_depth_copy.depth_min),
                              get_frame_graph_texture(resources.framegraph_resources, framegraph,
                                                      light_raster_record.tile_depth_copy.depth_max));

            const u32        clear_value = 0;
            FrameGraphBuffer light_lists = get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                                                  light_raster_record.tile_depth_copy.light_list_clear);

            vkCmdFillBuffer(cmdBuffer.handle, light_lists.handle, light_lists.default_view.offset_bytes,
                            light_lists.default_view.size_bytes, clear_value);

            FrameGraphBuffer counters =
                get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                       light_raster_record.tile_depth_copy.classification_counters_clear);

            vkCmdFillBuffer(cmdBuffer.handle, counters.handle, counters.default_view.offset_bytes,
                            counters.default_view.size_bytes, clear_value);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       light_raster_record.tile_depth_copy.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Classify Light Volumes");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       light_raster_record.light_classify.pass_handle, true);

            record_light_classify_command_buffer(cmdBuffer, tiled_lighting_frame, resources.tiled_raster_resources);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       light_raster_record.light_classify.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Rasterize Light Volumes");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       light_raster_record.light_raster.pass_handle, true);

            record_light_raster_command_buffer(
                cmdBuffer, resources.tiled_raster_resources.light_raster,
                get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                       light_raster_record.light_raster.command_counters),
                get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                       light_raster_record.light_raster.draw_commands_inner),
                get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                       light_raster_record.light_raster.draw_commands_outer),
                get_frame_graph_texture(resources.framegraph_resources, framegraph,
                                        light_raster_record.light_raster.tile_depth_min),
                get_frame_graph_texture(resources.framegraph_resources, framegraph,
                                        light_raster_record.light_raster.tile_depth_max));

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       light_raster_record.light_raster.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Tiled Lighting");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       tiled_lighting.pass_handle, true);

            record_tiled_lighting_command_buffer(cmdBuffer, resources.tiled_lighting_resources, render_extent,
                                                 VkExtent2D{light_raster_record.tile_depth_properties.width,
                                                            light_raster_record.tile_depth_properties.height});

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       tiled_lighting.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Tiled Lighting Debug");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       tiled_lighting_debug_record.pass_handle, true);

            record_tiled_lighting_debug_command_buffer(cmdBuffer, resources.tiled_lighting_resources, render_extent,
                                                       VkExtent2D{light_raster_record.tile_depth_properties.width,
                                                                  light_raster_record.tile_depth_properties.height});

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       tiled_lighting_debug_record.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Forward");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       forward.pass_handle, true);

            record_forward_pass_command_buffer(
                cmdBuffer, prepared, resources.forward_pass_resources,
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, forward.meshlet_counters),
                get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                       forward.meshlet_indirect_draw_commands),
                get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                       forward.meshlet_visible_index_buffer),
                get_frame_graph_texture(resources.framegraph_resources, framegraph, forward.scene_hdr),
                get_frame_graph_texture(resources.framegraph_resources, framegraph, forward.depth));

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       forward.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "GUI");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources, gui.pass_handle,
                                       true);

            record_gui_command_buffer(cmdBuffer, resources.gui_pass_resources,
                                      get_frame_graph_texture(resources.framegraph_resources, framegraph, gui.output),
                                      imgui_draw_data);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources, gui.pass_handle,
                                       false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Histogram Clear");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       histogram_clear.pass_handle, true);

            const u32        clear_value = 0;
            FrameGraphBuffer histogram_buffer =
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, histogram_clear.histogram_buffer);

            vkCmdFillBuffer(cmdBuffer.handle, histogram_buffer.handle, histogram_buffer.default_view.offset_bytes,
                            histogram_buffer.default_view.size_bytes, clear_value);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       histogram_clear.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Histogram");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       histogram.pass_handle, true);

            record_histogram_command_buffer(cmdBuffer, resources.histogram_pass_resources, render_extent);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       histogram.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Debug Geometry Build Commands");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       debug_geometry_build_cmds.pass_handle, true);

            record_debug_geometry_build_cmds_command_buffer(cmdBuffer, resources.debug_geometry_resources);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       debug_geometry_build_cmds.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Debug Geometry Draw");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       debug_geometry_draw.pass_handle, true);

            record_debug_geometry_draw_command_buffer(
                cmdBuffer, resources.debug_geometry_resources,
                get_frame_graph_texture(resources.framegraph_resources, framegraph, debug_geometry_draw.scene_hdr),
                get_frame_graph_texture(resources.framegraph_resources, framegraph, debug_geometry_draw.scene_depth),
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, debug_geometry_draw.draw_counter),
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, debug_geometry_draw.draw_commands));

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       debug_geometry_draw.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Swapchain");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       swapchain.pass_handle, true);

            record_swapchain_command_buffer(cmdBuffer, resources.swapchain_pass_resources,
                                            backend.presentInfo.imageViews[current_swapchain_index],
                                            backend.presentInfo.surface_extent);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       swapchain.pass_handle, false);
        }

        {
            const GPUTextureAccess src = swapchain_access_render;
            const GPUTextureAccess dst = swapchain_access_present;

            const GPUTextureSubresource subresource = default_texture_subresource_one_color_mip();

            const VkImageMemoryBarrier2 barrier =
                get_vk_image_barrier(backend.presentInfo.images[current_swapchain_index], subresource, src, dst);

            const VkDependencyInfo dependencies = get_vk_image_barrier_depency_info(std::span(&barrier, 1));

            vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Audio Render");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       audio_pass.render.pass_handle, true);

            record_audio_render_command_buffer(cmdBuffer, prepared, resources.audio_resources);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       audio_pass.render.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Audio Staging Copy");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       audio_pass.staging_copy.pass_handle, true);

            record_audio_copy_command_buffer(cmdBuffer, resources.audio_resources,
                                             get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                                                    audio_pass.staging_copy.audio_buffer));

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       audio_pass.staging_copy.pass_handle, false);
        }
    } // namespace Reaper

#if defined(REAPER_USE_TRACY)
    TracyVkCollect(cmdBuffer.tracy_ctx, cmdBuffer.handle);
#endif

    // Stop recording
    AssertVk(vkEndCommandBuffer(cmdBuffer.handle));

    const VkSemaphoreSubmitInfo wait_semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = backend.semaphore_swapchain_image_available,
        .value = 0, // NOTE: Ignored when not using a timeline semaphore
        .stageMask = swapchain_access_render.stage_mask,
        .deviceIndex = 0, // NOTE: Set to zero when not using device groups
    };

    const VkCommandBufferSubmitInfo command_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmdBuffer.handle,
        .deviceMask = 0, // NOTE: Set to zero when not using device groups
    };

    // NOTE: VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT is used there
    // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
    const VkSemaphoreSubmitInfo signal_semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = backend.semaphore_rendering_finished,
        .value = 0, // NOTE: Ignored when not using a timeline semaphore
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .deviceIndex = 0, // NOTE: Set to zero when not using device groups
    };

    const VkSubmitInfo2 submit_info_2 = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &wait_semaphore_info,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &command_buffer_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signal_semaphore_info,
    };

    log_debug(root, "vulkan: submit drawing commands");
    AssertVk(vkQueueSubmit2(backend.graphics_queue, 1, &submit_info_2, resources.frame_sync_resources.draw_fence));

    log_debug(root, "vulkan: present");

    const VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &backend.semaphore_rendering_finished,
        .swapchainCount = 1,
        .pSwapchains = &backend.presentInfo.swapchain,
        .pImageIndices = &current_swapchain_index,
        .pResults = nullptr,
    };

    Assert(!backend.presentInfo.queue_swapchain_transition);

    VkResult presentResult = vkQueuePresentKHR(backend.present_queue, &presentInfo);

    if (presentResult == VK_SUBOPTIMAL_KHR)
    {
        backend.new_swapchain_extent = backend.presentInfo.surface_extent;
        log_warning(root, "vulkan: present returned 'VK_SUBOPTIMAL_KHR' requesting swapchain re-creation");
    }
    else if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // NOTE: window can change state between event handling and presenting, so it's ok to get OOD events as long as
        // we re-create a correct swapchain later
        log_error(root, "vulkan: present failed with 'VK_ERROR_OUT_OF_DATE_KHR'");
    }
    else
    {
        Assert(presentResult == VK_SUCCESS);
    }

    {
        const VkResult event_status =
            vkGetEventStatus(backend.device, resources.meshlet_culling_resources.countersReadyEvent);
        Assert(event_status == VK_EVENT_SET || event_status == VK_EVENT_RESET);

        MeshletCullingStats                    total = {};
        const std::vector<MeshletCullingStats> culling_stats =
            get_meshlet_culling_gpu_stats(backend, prepared, resources.meshlet_culling_resources);

        log_debug(root, "{}GPU mesh culling stats:", event_status == VK_EVENT_SET ? "" : "[OUT OF DATE] ");
        for (auto stats : culling_stats)
        {
            log_debug(root, "- pass {} surviving meshlets = {}, triangles = {}, draw commands = {}", stats.pass_index,
                      stats.surviving_meshlet_count, stats.surviving_triangle_count, stats.indirect_draw_command_count);

            total.surviving_meshlet_count += stats.surviving_meshlet_count;
            total.surviving_triangle_count += stats.surviving_triangle_count;
            total.indirect_draw_command_count += stats.indirect_draw_command_count;
        }
        log_debug(root, "- total surviving meshlets = {}, triangles = {}, draw commands = {}",
                  total.surviving_meshlet_count, total.surviving_triangle_count, total.indirect_draw_command_count);
    }

    read_gpu_audio_data(backend, resources.audio_resources);
}
} // namespace Reaper
