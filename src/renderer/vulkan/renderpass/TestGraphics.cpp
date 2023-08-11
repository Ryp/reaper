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
#include "renderer/vulkan/Memory.h"
#include "renderer/vulkan/MeshCache.h"
#include "renderer/vulkan/Swapchain.h"
#include "renderer/vulkan/api/VulkanStringConversion.h"
#include "renderer/vulkan/renderpass/Constants.h"
#include "renderer/vulkan/renderpass/ForwardPassConstants.h"
#include "renderer/vulkan/renderpass/FrameGraphPass.h"
#include "renderer/vulkan/renderpass/GBufferPassConstants.h"
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
                          GetImageLayoutToString(barrier.src.access.image_layout),
                          GetImageLayoutToString(barrier.dst.access.image_layout));
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
        vkQueueWaitIdle(backend.deviceInfo.presentQueue); // FIXME

        Assert(backend.new_swapchain_extent.height > 0);
        resize_vulkan_wm_swapchain(root, backend, backend.presentInfo, backend.new_swapchain_extent);

        const glm::uvec2 new_swapchain_extent(backend.presentInfo.surface_extent.width,
                                              backend.presentInfo.surface_extent.height);

        backend.new_swapchain_extent.width = 0;
        backend.new_swapchain_extent.height = 0;
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
        ImGui::Checkbox("Freeze culling", &backend.options.freeze_meshlet_culling);
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
    uint32_t current_swapchain_index = 0;

    constexpr u32 MaxAcquireTryCount = 10;

    for (u32 acquireTryCount = 0; acquireTryCount < MaxAcquireTryCount; acquireTryCount++)
    {
        log_debug(root, "vulkan: acquiring frame try #{}", acquireTryCount);
        acquireResult =
            vkAcquireNextImageKHR(backend.device, backend.presentInfo.swapchain, acquireTimeoutUs,
                                  backend.semaphore_image_available, VK_NULL_HANDLE, &current_swapchain_index);

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
        VkFence drawFence = resources.frame_sync_resources.drawFence;

        VkResult waitResult;
        log_debug(root, "vulkan: wait for fence");

        do
        {
            REAPER_PROFILE_SCOPE_COLOR("Wait for fence", Color::Red);

            const u64 waitTimeoutNs = 1 * 1000 * 1000 * 1000;
            waitResult = vkWaitForFences(backend.device, 1, &drawFence, VK_TRUE, waitTimeoutNs);

            if (waitResult != VK_SUCCESS)
            {
                log_debug(root, "- return result {}", GetResultToString(waitResult));
            }
        } while (waitResult != VK_SUCCESS);

#if defined(REAPER_USE_TRACY)
        FrameMark;
#endif

        Assert(vkGetFenceStatus(backend.device, drawFence) == VK_SUCCESS);

        log_debug(root, "vulkan: reset fence");
        Assert(vkResetFences(backend.device, 1, &drawFence) == VK_SUCCESS);
    }

    const VkExtent2D backbufferExtent = backend.presentInfo.surface_extent;

    FrameData frame_data = {};
    frame_data.backbufferExtent = backbufferExtent;

    {
        REAPER_PROFILE_SCOPE("Upload Resources");
        upload_meshlet_culling_resources(backend, prepared, resources.meshlet_culling_resources);
        upload_vis_buffer_pass_frame_resources(backend, prepared, resources.vis_buffer_pass_resources);
        upload_shadow_map_resources(backend, prepared, resources.shadow_map_resources);
        upload_lighting_pass_frame_resources(backend, prepared, resources.lighting_resources);
        upload_tiled_raster_pass_frame_resources(backend, tiled_lighting_frame, resources.tiled_raster_resources);
        upload_tiled_lighting_pass_frame_resources(backend, prepared, resources.tiled_lighting_resources);
        upload_forward_pass_frame_resources(backend, prepared, resources.forward_pass_resources);
        upload_debug_geometry_build_cmds_pass_frame_resources(backend, prepared, resources.debug_geometry_resources);
        upload_audio_frame_resources(backend, prepared, resources.audio_resources);
    }

    FrameGraph::FrameGraph framegraph;

    using namespace FrameGraph;

    Builder builder(framegraph);

    // Debug geometry clear
    struct DebugGeometryClearFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle draw_counter;
        ResourceUsageHandle user_commands_buffer;
    } debug_geometry_clear;

    debug_geometry_clear.pass_handle = builder.create_render_pass("Debug Geometry Clear");

    const GPUBufferProperties debug_geometry_counter_properties = DefaultGPUBufferProperties(
        1, sizeof(u32), GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst);

    debug_geometry_clear.draw_counter = builder.create_buffer(
        debug_geometry_clear.pass_handle, "Debug Indirect draw counter buffer", debug_geometry_counter_properties,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT},
                         .buffer_view = default_buffer_view(debug_geometry_counter_properties)});

    const GPUBufferProperties debug_geometry_user_commands_properties = DefaultGPUBufferProperties(
        DebugGeometryCountMax, sizeof(DebugGeometryUserCommand), GPUBufferUsage::StorageBuffer);

    // Technically we shouldn't create an usage here, the first client of the debug geometry API should call
    // create_buffer() with the right data. But it makes it slightly simpler this way for the user API so I'm taking
    // the trade-off and paying for an extra useless barrier.
    debug_geometry_clear.user_commands_buffer = builder.create_buffer(
        debug_geometry_clear.pass_handle, "Debug geometry user command buffer", debug_geometry_user_commands_properties,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT},
                         .buffer_view = default_buffer_view(debug_geometry_user_commands_properties)});

    // Shadow
    struct ShadowFrameGraphData
    {
        RenderPassHandle                 pass_handle;
        std::vector<ResourceUsageHandle> shadow_maps;
    } shadow;

    shadow.pass_handle = builder.create_render_pass("Shadow");

    std::vector<GPUTextureProperties> shadow_map_properties = fill_shadow_map_properties(prepared);
    for (const GPUTextureProperties& properties : shadow_map_properties)
    {
        shadow.shadow_maps.push_back(
            builder.create_texture(shadow.pass_handle, "Shadow map", properties,
                                   GPUResourceUsage{.access = {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                                               VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                               VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                                                    .texture_view = DefaultGPUTextureView(properties)}));
    }

    // Visibility
    struct VisibilityFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle vis_buffer;
        ResourceUsageHandle depth;
    } visibility;

    visibility.pass_handle = builder.create_render_pass("Visibility");

    const GPUTextureProperties vis_buffer_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, VisibilityBufferFormat,
                                    GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled);

    visibility.vis_buffer = builder.create_texture(
        visibility.pass_handle, "Visibility Buffer", vis_buffer_properties,
        GPUResourceUsage{.access{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(vis_buffer_properties)});

    const GPUTextureProperties scene_depth_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, MainPassDepthFormat,
                                    GPUTextureUsage::DepthStencilAttachment | GPUTextureUsage::Sampled);

    visibility.depth = builder.create_texture(
        visibility.pass_handle, "Main Depth", scene_depth_properties,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                     VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(scene_depth_properties)});

    // Fill GBuffer from visibility
    struct VisibilityGBufferFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle vis_buffer;
        ResourceUsageHandle gbuffer_rt0;
        ResourceUsageHandle gbuffer_rt1;
    } visibility_gbuffer;

    visibility_gbuffer.pass_handle = builder.create_render_pass("Visibility Fill GBuffer");

    visibility_gbuffer.vis_buffer = builder.read_texture(
        visibility_gbuffer.pass_handle, visibility.vis_buffer,
        GPUResourceUsage{.access = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                    VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(vis_buffer_properties)});

    const GPUTextureProperties gbuffer_rt0_properties = DefaultGPUTextureProperties(
        backbufferExtent.width, backbufferExtent.height, GBufferRT0Format,
        GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled | GPUTextureUsage::Storage);

    const GPUTextureProperties gbuffer_rt1_properties = DefaultGPUTextureProperties(
        backbufferExtent.width, backbufferExtent.height, GBufferRT1Format,
        GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled | GPUTextureUsage::Storage);

    visibility_gbuffer.gbuffer_rt0 =
        builder.create_texture(visibility_gbuffer.pass_handle, "GBuffer RT0", gbuffer_rt0_properties,
                               GPUResourceUsage{.access{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                        VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
                                                .texture_view = DefaultGPUTextureView(gbuffer_rt0_properties)});

    visibility_gbuffer.gbuffer_rt1 =
        builder.create_texture(visibility_gbuffer.pass_handle, "GBuffer RT1", gbuffer_rt1_properties,
                               GPUResourceUsage{.access{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                        VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
                                                .texture_view = DefaultGPUTextureView(gbuffer_rt1_properties)});

    // HZB
    struct HZBReduceFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle depth;
        ResourceUsageHandle hzb_texture;
    } hzb_reduce;

    hzb_reduce.pass_handle = builder.create_render_pass("HZB Reduce");

    hzb_reduce.depth =
        builder.read_texture(hzb_reduce.pass_handle, visibility.depth,
                             GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                                          VK_ACCESS_2_SHADER_READ_BIT,
                                                                          VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL},
                                              .texture_view = DefaultGPUTextureView(scene_depth_properties)});

    GPUTextureProperties hzb_properties =
        DefaultGPUTextureProperties(scene_depth_properties.width / 2, scene_depth_properties.height / 2,
                                    PixelFormat::R16G16_UNORM, GPUTextureUsage::Storage | GPUTextureUsage::Sampled);
    hzb_properties.mipCount = 4; // FIXME

    hzb_reduce.hzb_texture = builder.create_texture(
        hzb_reduce.pass_handle, "HZB Texture", hzb_properties,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                     VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
                         .texture_view = DefaultGPUTextureView(hzb_properties)});

    // Depth Downsample
    struct TileDepthFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle scene_depth;
        ResourceUsageHandle depth_min_storage;
        ResourceUsageHandle depth_max_storage;
    } tile_depth;

    tile_depth.pass_handle = builder.create_render_pass("Depth Downsample");

    tile_depth.scene_depth =
        builder.read_texture(tile_depth.pass_handle, visibility.depth,
                             GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                                          VK_ACCESS_2_SHADER_READ_BIT,
                                                                          VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL},
                                              .texture_view = DefaultGPUTextureView(scene_depth_properties)});

    const GPUTextureProperties tile_depth_storage_properties =
        DefaultGPUTextureProperties(tiled_lighting_frame.tile_count_x, tiled_lighting_frame.tile_count_y,
                                    PixelFormat::R16_UNORM, GPUTextureUsage::Storage | GPUTextureUsage::Sampled);

    GPUResourceUsage tile_depth_create_usage = {};
    tile_depth_create_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                       VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL};
    tile_depth_create_usage.texture_view = DefaultGPUTextureView(tile_depth_storage_properties);

    tile_depth.depth_min_storage = builder.create_texture(tile_depth.pass_handle, "Tile Depth Min Storage",
                                                          tile_depth_storage_properties, tile_depth_create_usage);

    tile_depth.depth_max_storage = builder.create_texture(tile_depth.pass_handle, "Tile Depth Max Storage",
                                                          tile_depth_storage_properties, tile_depth_create_usage);

    // Depth copy
    struct TileDepthCopyFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle depth_min_src;
        ResourceUsageHandle depth_max_src;
        ResourceUsageHandle depth_min;
        ResourceUsageHandle depth_max;
        ResourceUsageHandle hzb_texture;
        ResourceUsageHandle light_list_clear_usage_handle;
        ResourceUsageHandle classification_counters_clear;
    } tile_depth_copy;

    tile_depth_copy.pass_handle = builder.create_render_pass("Tile Depth Copy");

    GPUResourceUsage tile_depth_copy_src_usage = {};
    tile_depth_copy_src_usage.access = GPUResourceAccess{
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    tile_depth_copy_src_usage.texture_view = DefaultGPUTextureView(tile_depth_storage_properties);

    GPUTextureProperties tile_depth_copy_properties = tile_depth_storage_properties;
    tile_depth_copy_properties.usage_flags = GPUTextureUsage::DepthStencilAttachment | GPUTextureUsage::Sampled;
    tile_depth_copy_properties.format = MainPassDepthFormat;

    GPUResourceUsage tile_depth_copy_dst_usage = {};
    tile_depth_copy_dst_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                          VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL};
    tile_depth_copy_dst_usage.texture_view = DefaultGPUTextureView(tile_depth_copy_properties);

    tile_depth_copy.depth_min_src =
        builder.read_texture(tile_depth_copy.pass_handle, tile_depth.depth_min_storage, tile_depth_copy_src_usage);
    tile_depth_copy.depth_max_src =
        builder.read_texture(tile_depth_copy.pass_handle, tile_depth.depth_max_storage, tile_depth_copy_src_usage);

    tile_depth_copy.depth_min = builder.create_texture(tile_depth_copy.pass_handle, "Tile Depth Min",
                                                       tile_depth_copy_properties, tile_depth_copy_dst_usage);
    tile_depth_copy.depth_max = builder.create_texture(tile_depth_copy.pass_handle, "Tile Depth Max",
                                                       tile_depth_copy_properties, tile_depth_copy_dst_usage);

    // FIXME
    {
        GPUTextureView hzb_view = DefaultGPUTextureView(hzb_properties);
        hzb_view.mipCount = 1;
        hzb_view.mipOffset = 3;

        tile_depth_copy.hzb_texture = builder.read_texture(
            tile_depth_copy.pass_handle, hzb_reduce.hzb_texture,
            GPUResourceUsage{.access{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                     VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL},
                             .texture_view = hzb_view});
    }

    const GPUBufferProperties light_list_properties = DefaultGPUBufferProperties(
        ElementsPerTile * tile_depth_storage_properties.width * tile_depth_storage_properties.height, sizeof(u32),
        GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst);

    GPUResourceUsage light_list_clear_usage = {};
    light_list_clear_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
    light_list_clear_usage.buffer_view = default_buffer_view(light_list_properties);

    tile_depth_copy.light_list_clear_usage_handle = builder.create_buffer(
        tile_depth_copy.pass_handle, "Light lists", light_list_properties, light_list_clear_usage);

    const GPUBufferProperties classification_counters_properties = DefaultGPUBufferProperties(
        2, sizeof(u32), GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst | GPUBufferUsage::IndirectBuffer);

    GPUResourceUsage classification_counters_clear_usage = {};
    classification_counters_clear_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
    classification_counters_clear_usage.buffer_view = default_buffer_view(classification_counters_properties);

    tile_depth_copy.classification_counters_clear =
        builder.create_buffer(tile_depth_copy.pass_handle, "Classification counters",
                              classification_counters_properties, classification_counters_clear_usage);

    // Light raster classify
    struct LightClassifyFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle classification_counters;
        ResourceUsageHandle draw_commands_inner;
        ResourceUsageHandle draw_commands_outer;
    } light_classify;

    light_classify.pass_handle = builder.create_render_pass("Classify Light Volumes");

    light_classify.classification_counters =
        builder.write_buffer(light_classify.pass_handle, tile_depth_copy.classification_counters_clear,
                             GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                                          VK_ACCESS_2_SHADER_WRITE_BIT},
                                              .buffer_view = default_buffer_view(classification_counters_properties)});

    const GPUBufferProperties draw_command_classify_properties =
        DefaultGPUBufferProperties(TiledRasterMaxIndirectCommandCount, 4 * sizeof(u32), // FIXME
                                   GPUBufferUsage::StorageBuffer | GPUBufferUsage::IndirectBuffer);

    GPUResourceUsage draw_command_classify_usage = {
        .access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT},
        .buffer_view = default_buffer_view(draw_command_classify_properties)};

    light_classify.draw_commands_inner =
        builder.create_buffer(light_classify.pass_handle, "Draw Commands Inner", draw_command_classify_properties,
                              draw_command_classify_usage);
    light_classify.draw_commands_outer =
        builder.create_buffer(light_classify.pass_handle, "Draw Commands Outer", draw_command_classify_properties,
                              draw_command_classify_usage);

    // Light raster
    struct LightRasterFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle command_counters;
        ResourceUsageHandle draw_commands_inner;
        ResourceUsageHandle draw_commands_outer;
        ResourceUsageHandle tile_depth_min;
        ResourceUsageHandle tile_depth_max;
        ResourceUsageHandle light_list;
    } light_raster;

    light_raster.pass_handle = builder.create_render_pass("Rasterize Light Volumes");

    light_raster.command_counters =
        builder.read_buffer(light_raster.pass_handle, light_classify.classification_counters,
                            GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                                                                         VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT},
                                             .buffer_view = default_buffer_view(classification_counters_properties)});

    {
        GPUResourceUsage draw_command_raster_read_usage = {
            .access = GPUResourceAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT},
            .buffer_view = default_buffer_view(draw_command_classify_properties)};

        light_raster.draw_commands_inner = builder.read_buffer(
            light_raster.pass_handle, light_classify.draw_commands_inner, draw_command_raster_read_usage);
        light_raster.draw_commands_outer = builder.read_buffer(
            light_raster.pass_handle, light_classify.draw_commands_outer, draw_command_raster_read_usage);
    }

    {
        GPUResourceUsage tile_depth_usage = {
            .access = GPUResourceAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                            | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                                        VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL},
            .texture_view = DefaultGPUTextureView(tile_depth_copy_properties),
        };

        light_raster.tile_depth_min =
            builder.read_texture(light_raster.pass_handle, tile_depth_copy.depth_min, tile_depth_usage);
        light_raster.tile_depth_max =
            builder.read_texture(light_raster.pass_handle, tile_depth_copy.depth_max, tile_depth_usage);
    }

    {
        GPUResourceUsage light_list_usage = {};
        light_list_usage.access =
            GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        light_list_usage.buffer_view = default_buffer_view(light_list_properties);

        light_raster.light_list = builder.write_buffer(light_raster.pass_handle,
                                                       tile_depth_copy.light_list_clear_usage_handle, light_list_usage);
    }

    // Tiled Lighting
    struct TiledLightingFrameGraphData
    {
        RenderPassHandle                 pass_handle;
        std::vector<ResourceUsageHandle> shadow_maps;
        ResourceUsageHandle              light_list;
        ResourceUsageHandle              depth;
        ResourceUsageHandle              gbuffer_rt0;
        ResourceUsageHandle              gbuffer_rt1;
        ResourceUsageHandle              lighting_usage_handle;
        ResourceUsageHandle              tile_debug_usage_handle;
    } tiled_lighting;

    tiled_lighting.pass_handle = builder.create_render_pass("Tiled Lighting");

    for (u32 shadow_map_index = 0; shadow_map_index < shadow_map_properties.size(); shadow_map_index++)
    {
        GPUResourceUsage shadow_map_usage = {};
        shadow_map_usage.access = {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                   VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
        shadow_map_usage.texture_view = DefaultGPUTextureView(shadow_map_properties[shadow_map_index]);

        const ResourceUsageHandle usage_handle =
            builder.read_texture(tiled_lighting.pass_handle, shadow.shadow_maps[shadow_map_index], shadow_map_usage);
        tiled_lighting.shadow_maps.push_back(usage_handle);
    }

    {
        GPUResourceUsage light_list_usage = {};
        light_list_usage.access =
            GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        light_list_usage.buffer_view = default_buffer_view(light_list_properties);

        tiled_lighting.light_list =
            builder.read_buffer(tiled_lighting.pass_handle, light_raster.light_list, light_list_usage);
    }

    tiled_lighting.gbuffer_rt0 = builder.read_texture(
        tiled_lighting.pass_handle, visibility_gbuffer.gbuffer_rt0,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                     VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(gbuffer_rt0_properties)});

    tiled_lighting.gbuffer_rt1 = builder.read_texture(
        tiled_lighting.pass_handle, visibility_gbuffer.gbuffer_rt1,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                     VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(gbuffer_rt1_properties)});

    {
        GPUResourceUsage depth_usage = {};
        depth_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                               VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL};
        depth_usage.texture_view = DefaultGPUTextureView(scene_depth_properties);

        tiled_lighting.depth = builder.read_texture(tiled_lighting.pass_handle, visibility.depth, depth_usage);
    }

    GPUTextureProperties lighting_properties = DefaultGPUTextureProperties(
        backbufferExtent.width, backbufferExtent.height, PixelFormat::B10G11R11_UFLOAT_PACK32,
        GPUTextureUsage::Storage | GPUTextureUsage::Sampled | GPUTextureUsage::ColorAttachment);

    {
        GPUResourceUsage lighting_usage = {};
        lighting_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                                                  VK_IMAGE_LAYOUT_GENERAL};
        lighting_usage.texture_view = DefaultGPUTextureView(lighting_properties);

        tiled_lighting.lighting_usage_handle =
            builder.create_texture(tiled_lighting.pass_handle, "Lighting", lighting_properties, lighting_usage);
    }

    const GPUBufferProperties tile_debug_properties =
        DefaultGPUBufferProperties(tile_depth_storage_properties.width * tile_depth_storage_properties.height,
                                   sizeof(TileDebug), GPUBufferUsage::StorageBuffer);

    {
        GPUResourceUsage tile_debug_create_usage = {};
        tile_debug_create_usage.access =
            GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        tile_debug_create_usage.buffer_view = default_buffer_view(tile_debug_properties);

        tiled_lighting.tile_debug_usage_handle = builder.create_buffer(tiled_lighting.pass_handle, "Tile debug",
                                                                       tile_debug_properties, tile_debug_create_usage);
    }

    // Tiled Lighting debug
    struct TiledLightingDebugFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle tile_debug;
        ResourceUsageHandle output;
    } tiled_lighting_debug;

    tiled_lighting_debug.pass_handle = builder.create_render_pass("Tiled Lighting Debug");

    tiled_lighting_debug.tile_debug =
        builder.read_buffer(tiled_lighting_debug.pass_handle, tiled_lighting.tile_debug_usage_handle,
                            GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                                         VK_ACCESS_2_SHADER_READ_BIT},
                                             .buffer_view = default_buffer_view(tile_debug_properties)});

    const GPUTextureProperties tiled_debug_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, PixelFormat::R8G8B8A8_UNORM,
                                    GPUTextureUsage::Storage | GPUTextureUsage::Sampled);

    tiled_lighting_debug.output = builder.create_texture(
        tiled_lighting_debug.pass_handle, "Tiled Lighting Debug Texture", tiled_debug_properties,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                     VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL},
                         .texture_view = DefaultGPUTextureView(tiled_debug_properties)});

    // Forward
    struct ForwardFrameGraphData
    {
        RenderPassHandle                 pass_handle;
        ResourceUsageHandle              scene_hdr;
        ResourceUsageHandle              depth;
        std::vector<ResourceUsageHandle> shadow_maps;
    } forward;

    forward.pass_handle = builder.create_render_pass("Forward");

    const GPUTextureProperties scene_hdr_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, ForwardHDRColorFormat,
                                    GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled);

    forward.scene_hdr = builder.create_texture(
        forward.pass_handle, "Scene HDR", scene_hdr_properties,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                     VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(scene_hdr_properties)});

    forward.depth = builder.write_texture(
        forward.pass_handle, visibility.depth,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                     VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(scene_depth_properties)});

    for (u32 shadow_map_index = 0; shadow_map_index < shadow_map_properties.size(); shadow_map_index++)
    {
        forward.shadow_maps.push_back(builder.read_texture(
            forward.pass_handle, shadow.shadow_maps[shadow_map_index],
            GPUResourceUsage{.access = {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                        VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL},
                             .texture_view = DefaultGPUTextureView(shadow_map_properties[shadow_map_index])}));
    }

    // GUI
    struct GUIFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle output;
    } gui;

    gui.pass_handle = builder.create_render_pass("GUI");

    const GPUTextureProperties gui_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, GUIFormat,
                                    GPUTextureUsage::ColorAttachment | GPUTextureUsage::Sampled);

    gui.output = builder.create_texture(
        gui.pass_handle, "GUI SDR", gui_properties,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                     VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(gui_properties)});

    // Histogram Clear
    struct HistogramClearFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle histogram_buffer;
    } histogram_clear;

    histogram_clear.pass_handle = builder.create_render_pass("Histogram Clear");

    const GPUBufferProperties histogram_buffer_properties = DefaultGPUBufferProperties(
        HistogramRes, sizeof(u32), GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst);

    histogram_clear.histogram_buffer = builder.create_buffer(
        histogram_clear.pass_handle, "Histogram Buffer", histogram_buffer_properties,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT},
                         .buffer_view = default_buffer_view(histogram_buffer_properties)});

    // Histogram
    struct HistogramFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle scene_hdr;
        ResourceUsageHandle histogram_buffer;
    } histogram;

    histogram.pass_handle = builder.create_render_pass("Histogram");

    histogram.scene_hdr = builder.read_texture(
        histogram.pass_handle, forward.scene_hdr,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                     VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(scene_hdr_properties)});

    histogram.histogram_buffer =
        builder.write_buffer(histogram.pass_handle, histogram_clear.histogram_buffer,
                             GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                                          VK_ACCESS_2_SHADER_WRITE_BIT},
                                              .buffer_view = default_buffer_view(histogram_buffer_properties)});

    // Debug geometry create command buffer
    struct DebugGeometryComputeFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle draw_counter;
        ResourceUsageHandle user_commands_buffer;
        ResourceUsageHandle draw_commands;
        ResourceUsageHandle instance_buffer;
    } debug_geometry_build_cmds;

    debug_geometry_build_cmds.pass_handle = builder.create_render_pass("Debug Geometry Build Commands");

    debug_geometry_build_cmds.draw_counter =
        builder.read_buffer(debug_geometry_build_cmds.pass_handle, debug_geometry_clear.draw_counter,
                            GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                                         VK_ACCESS_2_SHADER_READ_BIT},
                                             .buffer_view = default_buffer_view(debug_geometry_counter_properties)});

    debug_geometry_build_cmds.user_commands_buffer = builder.read_buffer(
        debug_geometry_build_cmds.pass_handle, debug_geometry_clear.user_commands_buffer,
        GPUResourceUsage{.access =
                             GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT},
                         .buffer_view = default_buffer_view(debug_geometry_user_commands_properties)});

    const GPUBufferProperties debug_geometry_command_properties =
        DefaultGPUBufferProperties(DebugGeometryCountMax, sizeof(VkDrawIndexedIndirectCommand),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer);

    debug_geometry_build_cmds.draw_commands = builder.create_buffer(
        debug_geometry_build_cmds.pass_handle, "Debug Indirect draw command buffer", debug_geometry_command_properties,
        GPUResourceUsage{.access =
                             GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT},
                         .buffer_view = default_buffer_view(debug_geometry_command_properties)});

    const GPUBufferProperties debug_geometry_instance_properties =
        DefaultGPUBufferProperties(DebugGeometryCountMax, sizeof(DebugGeometryInstance), GPUBufferUsage::StorageBuffer);

    debug_geometry_build_cmds.instance_buffer = builder.create_buffer(
        debug_geometry_build_cmds.pass_handle, "Debug geometry instance buffer", debug_geometry_instance_properties,
        GPUResourceUsage{.access =
                             GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT},
                         .buffer_view = default_buffer_view(debug_geometry_instance_properties)});

    // Debug geometry draw
    struct DebugGeometryDrawFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle scene_hdr;
        ResourceUsageHandle scene_depth;
        ResourceUsageHandle draw_counter;
        ResourceUsageHandle draw_commands;
        ResourceUsageHandle instance_buffer;
    } debug_geometry_draw;

    debug_geometry_draw.pass_handle = builder.create_render_pass("Debug Geometry Draw");

    debug_geometry_draw.scene_hdr = builder.write_texture(
        debug_geometry_draw.pass_handle,
        tiled_lighting.lighting_usage_handle,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(scene_hdr_properties)});

    debug_geometry_draw.scene_depth = builder.read_texture(
        debug_geometry_draw.pass_handle, forward.depth,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                                         | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                                                     VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(scene_depth_properties)});

    debug_geometry_draw.draw_counter =
        builder.read_buffer(debug_geometry_draw.pass_handle, debug_geometry_clear.draw_counter,
                            GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                                                                         VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT},
                                             .buffer_view = default_buffer_view(debug_geometry_counter_properties)});

    debug_geometry_draw.draw_commands =
        builder.read_buffer(debug_geometry_draw.pass_handle, debug_geometry_build_cmds.draw_commands,
                            GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                                                                         VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT},
                                             .buffer_view = default_buffer_view(debug_geometry_command_properties)});

    debug_geometry_draw.instance_buffer =
        builder.read_buffer(debug_geometry_draw.pass_handle, debug_geometry_build_cmds.instance_buffer,
                            GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                                                                         VK_ACCESS_2_SHADER_READ_BIT},
                                             .buffer_view = default_buffer_view(debug_geometry_instance_properties)});

    // Swapchain
    struct SwapchainFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle scene_hdr;
        ResourceUsageHandle lighting_result;
        ResourceUsageHandle gui;
        ResourceUsageHandle histogram; // FIXME unused for now
        ResourceUsageHandle tile_debug;
    } swapchain;

    swapchain.pass_handle = builder.create_render_pass("Swapchain", true);

    swapchain.scene_hdr = builder.read_texture(
        swapchain.pass_handle, forward.scene_hdr,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                     VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(scene_hdr_properties)});

    swapchain.lighting_result = builder.read_texture(
        swapchain.pass_handle, debug_geometry_draw.scene_hdr,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                     VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(lighting_properties)});

    swapchain.gui = builder.read_texture(
        swapchain.pass_handle, gui.output,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                     VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(gui_properties)});

    swapchain.histogram =
        builder.read_buffer(swapchain.pass_handle, histogram.histogram_buffer,
                            GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                                         VK_ACCESS_2_SHADER_READ_BIT},
                                             .buffer_view = default_buffer_view(histogram_buffer_properties)});

    swapchain.tile_debug = builder.read_texture(
        swapchain.pass_handle, tiled_lighting_debug.output,
        GPUResourceUsage{.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                     VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL},
                         .texture_view = DefaultGPUTextureView(tiled_debug_properties)});

    builder.build();

    // DumpFrameGraph(framegraph);

    const FrameGraphSchedule schedule = compute_schedule(framegraph);

    log_barriers(root, framegraph, schedule);

    allocate_framegraph_volatile_resources(root, backend, resources.framegraph_resources, framegraph);

    DescriptorWriteHelper descriptor_write_helper(200, 200);

    update_meshlet_culling_pass_descriptor_sets(descriptor_write_helper, prepared, resources.meshlet_culling_resources,
                                                resources.mesh_cache);

    update_shadow_map_pass_descriptor_sets(descriptor_write_helper, prepared, resources.shadow_map_resources,
                                           resources.mesh_cache.vertexBufferPosition);

    std::vector<FrameGraphTexture> forward_shadow_map_views;
    for (auto handle : forward.shadow_maps)
    {
        forward_shadow_map_views.emplace_back(
            get_frame_graph_texture(resources.framegraph_resources, framegraph, handle));
    }

    update_vis_buffer_pass_descriptor_sets(
        descriptor_write_helper,
        resources.vis_buffer_pass_resources,
        prepared,
        resources.samplers_resources,
        resources.material_resources,
        resources.meshlet_culling_resources,
        resources.mesh_cache,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, visibility_gbuffer.vis_buffer),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, visibility_gbuffer.gbuffer_rt0),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, visibility_gbuffer.gbuffer_rt1));

    update_hzb_pass_descriptor_set(
        descriptor_write_helper, resources.hzb_pass_resources, resources.samplers_resources,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, hzb_reduce.depth),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, hzb_reduce.hzb_texture));

    update_forward_pass_descriptor_sets(descriptor_write_helper, resources.forward_pass_resources,
                                        resources.meshlet_culling_resources, resources.samplers_resources,
                                        resources.material_resources, resources.mesh_cache,
                                        resources.lighting_resources, forward_shadow_map_views);

    update_lighting_depth_downsample_descriptor_set(
        descriptor_write_helper, resources.tiled_raster_resources, resources.samplers_resources,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tile_depth.scene_depth),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tile_depth.depth_min_storage),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tile_depth.depth_max_storage));

    update_depth_copy_pass_descriptor_set(
        descriptor_write_helper, resources.tiled_raster_resources,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tile_depth_copy.depth_min_src),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tile_depth_copy.depth_max_src));

    update_classify_descriptor_set(
        descriptor_write_helper, resources.tiled_raster_resources,
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, light_classify.classification_counters),
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, light_classify.draw_commands_inner),
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, light_classify.draw_commands_outer));

    update_light_raster_pass_descriptor_sets(
        descriptor_write_helper, resources.tiled_raster_resources,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, light_raster.tile_depth_min),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, light_raster.tile_depth_max),
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, light_raster.light_list));

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
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tiled_lighting.lighting_usage_handle),
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, tiled_lighting.tile_debug_usage_handle),
        tiled_shadow_maps);

    update_tiled_lighting_debug_pass_descriptor_sets(
        descriptor_write_helper, resources.tiled_lighting_resources,
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, tiled_lighting_debug.tile_debug),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tiled_lighting_debug.output));

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

    update_audio_pass_descriptor_set(descriptor_write_helper, resources.audio_resources);

    descriptor_write_helper.flush_descriptor_write_helper(backend.device);

    log_debug(root, "vulkan: record command buffer");
    Assert(vkResetCommandBuffer(cmdBuffer.handle, 0) == VK_SUCCESS);

    const VkCommandBufferBeginInfo cmdBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .pInheritanceInfo = nullptr,
    };

    Assert(vkBeginCommandBuffer(cmdBuffer.handle, &cmdBufferBeginInfo) == VK_SUCCESS);

    {
        REAPER_GPU_SCOPE(cmdBuffer, "GPU Frame");

        if (backend.presentInfo.queue_swapchain_transition)
        {
            REAPER_GPU_SCOPE(cmdBuffer, "Barrier");

            std::vector<VkImageMemoryBarrier2> imageBarriers;

            for (u32 swapchainImageIndex = 0; swapchainImageIndex < static_cast<u32>(backend.presentInfo.images.size());
                 swapchainImageIndex++)
            {
                const GPUResourceAccess src_undefined = {VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
                                                         VK_IMAGE_LAYOUT_UNDEFINED};
                const GPUResourceAccess dst_access = {VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                                      VK_ACCESS_2_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

                GPUTextureView view = {};
                // view.format;
                view.aspect = ViewAspect::Color;
                view.mipCount = 1;
                view.layerCount = 1;

                imageBarriers.emplace_back(get_vk_image_barrier(backend.presentInfo.images[swapchainImageIndex], view,
                                                                src_undefined, dst_access));
            }

            const VkDependencyInfo dependencies =
                get_vk_image_barrier_depency_info(static_cast<u32>(imageBarriers.size()), imageBarriers.data());

            vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);

            backend.presentInfo.queue_swapchain_transition = false;
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Material Upload");
            record_material_upload_command_buffer(resources.material_resources.staging, cmdBuffer);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Meshlet Culling");

            if (!backend.options.freeze_meshlet_culling)
            {
                record_meshlet_culling_command_buffer(root, cmdBuffer, prepared, resources.meshlet_culling_resources);
            }
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Debug Geometry Clear");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       debug_geometry_clear.pass_handle, true);

            const u32        clear_value = 0;
            FrameGraphBuffer draw_counter =
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, debug_geometry_clear.draw_counter);

            vkCmdFillBuffer(cmdBuffer.handle, draw_counter.handle, draw_counter.view.offset_bytes,
                            draw_counter.view.size_bytes, clear_value);

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

            record_shadow_map_command_buffer(cmdBuffer, prepared, resources.shadow_map_resources, shadow_maps,
                                             resources.meshlet_culling_resources);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       shadow.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Visibility");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       visibility.pass_handle, true);

            record_vis_buffer_pass_command_buffer(
                cmdBuffer, prepared, resources.vis_buffer_pass_resources, resources.meshlet_culling_resources,
                get_frame_graph_texture(resources.framegraph_resources, framegraph, visibility.vis_buffer),
                get_frame_graph_texture(resources.framegraph_resources, framegraph, visibility.depth));

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       visibility.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "HZB Reduce");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       hzb_reduce.pass_handle, true);

            record_hzb_command_buffer(
                cmdBuffer,
                resources.hzb_pass_resources,
                VkExtent2D{.width = scene_depth_properties.width, .height = scene_depth_properties.height});

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       hzb_reduce.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Visibility Fill GBuffer");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       visibility_gbuffer.pass_handle, true);

            record_fill_gbuffer_pass_command_buffer(cmdBuffer, resources.vis_buffer_pass_resources, backbufferExtent);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       visibility_gbuffer.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Tile Depth");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       tile_depth.pass_handle, true);

            record_tile_depth_pass_command_buffer(cmdBuffer, resources.tiled_raster_resources, backbufferExtent);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       tile_depth.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Tile Depth Copy");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       tile_depth_copy.pass_handle, true);

            record_depth_copy(
                cmdBuffer, resources.tiled_raster_resources,
                get_frame_graph_texture(resources.framegraph_resources, framegraph, tile_depth_copy.depth_min),
                get_frame_graph_texture(resources.framegraph_resources, framegraph, tile_depth_copy.depth_max));

            const u32        clear_value = 0;
            FrameGraphBuffer light_lists = get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                                                  tile_depth_copy.light_list_clear_usage_handle);

            vkCmdFillBuffer(cmdBuffer.handle, light_lists.handle, light_lists.view.offset_bytes,
                            light_lists.view.size_bytes, clear_value);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       tile_depth_copy.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Classify Light Volumes");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       light_classify.pass_handle, true);

            const u32        clear_value = 0;
            FrameGraphBuffer counters = get_frame_graph_buffer(resources.framegraph_resources, framegraph,
                                                               light_classify.classification_counters);

            vkCmdFillBuffer(cmdBuffer.handle, counters.handle, counters.view.offset_bytes, counters.view.size_bytes,
                            clear_value);

            record_light_classify_command_buffer(cmdBuffer, tiled_lighting_frame, resources.tiled_raster_resources);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       light_classify.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Rasterize Light Volumes");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       light_raster.pass_handle, true);

            record_light_raster_command_buffer(
                cmdBuffer, resources.tiled_raster_resources,
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, light_raster.command_counters),
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, light_raster.draw_commands_inner),
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, light_raster.draw_commands_outer),
                get_frame_graph_texture(resources.framegraph_resources, framegraph, light_raster.tile_depth_min),
                get_frame_graph_texture(resources.framegraph_resources, framegraph, light_raster.tile_depth_max));

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       light_raster.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Tiled Lighting");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       tiled_lighting.pass_handle, true);

            record_tiled_lighting_command_buffer(
                cmdBuffer, resources.tiled_lighting_resources, backbufferExtent,
                VkExtent2D{tile_depth_copy_properties.width, tile_depth_copy_properties.height});

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       tiled_lighting.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Tiled Lighting Debug");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       tiled_lighting_debug.pass_handle, true);

            record_tiled_lighting_debug_command_buffer(
                cmdBuffer, resources.tiled_lighting_resources, backbufferExtent,
                VkExtent2D{tile_depth_copy_properties.width, tile_depth_copy_properties.height});

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       tiled_lighting_debug.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Forward");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       forward.pass_handle, true);

            record_forward_pass_command_buffer(
                cmdBuffer, prepared, resources.forward_pass_resources, resources.meshlet_culling_resources,
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

            vkCmdFillBuffer(cmdBuffer.handle, histogram_buffer.handle, histogram_buffer.view.offset_bytes,
                            histogram_buffer.view.size_bytes, clear_value);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       histogram_clear.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Histogram");
            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       histogram.pass_handle, true);

            record_histogram_command_buffer(cmdBuffer, frame_data, resources.histogram_pass_resources,
                                            backbufferExtent);

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

            record_swapchain_command_buffer(cmdBuffer, frame_data, resources.swapchain_pass_resources,
                                            backend.presentInfo.imageViews[current_swapchain_index]);

            record_framegraph_barriers(cmdBuffer, schedule, framegraph, resources.framegraph_resources,
                                       swapchain.pass_handle, false);
        }

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Audio");
            record_audio_command_buffer(cmdBuffer, prepared, resources.audio_resources);
        }
    }

#if defined(REAPER_USE_TRACY)
    TracyVkCollect(cmdBuffer.tracy_ctx, cmdBuffer.handle);
#endif

    // Stop recording
    Assert(vkEndCommandBuffer(cmdBuffer.handle) == VK_SUCCESS);

    const VkPipelineStageFlags waitDstMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &backend.semaphore_image_available,
        .pWaitDstStageMask = &waitDstMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdBuffer.handle,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &backend.semaphore_rendering_finished,
    };

    log_debug(root, "vulkan: submit drawing commands");
    Assert(vkQueueSubmit(backend.deviceInfo.graphicsQueue, 1, &submitInfo, resources.frame_sync_resources.drawFence)
           == VK_SUCCESS);

    log_debug(root, "vulkan: present");

    VkPresentInfoKHR presentInfo = {
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

    VkResult presentResult = vkQueuePresentKHR(backend.deviceInfo.presentQueue, &presentInfo);

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
