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
    if (backend.new_swapchain_extent.width != 0)
    {
        vkQueueWaitIdle(backend.deviceInfo.presentQueue); // FIXME

        Assert(backend.new_swapchain_extent.height > 0);
        resize_vulkan_wm_swapchain(root, backend, backend.presentInfo, backend.new_swapchain_extent);

        const glm::uvec2 new_swapchain_extent(backend.presentInfo.surfaceExtent.width,
                                              backend.presentInfo.surfaceExtent.height);

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
        acquireResult = vkAcquireNextImageKHR(backend.device, backend.presentInfo.swapchain, acquireTimeoutUs,
                                              backend.presentInfo.imageAvailableSemaphore, VK_NULL_HANDLE,
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
        VkFence drawFence = resources.frame_sync_resources.drawFence;

        VkResult waitResult;
        log_debug(root, "vulkan: wait for fence");

        do
        {
            REAPER_PROFILE_SCOPE_COLOR("Wait for fence", MP_RED);

            const u64 waitTimeoutNs = 1 * 1000 * 1000 * 1000;
            waitResult = vkWaitForFences(backend.device, 1, &drawFence, VK_TRUE, waitTimeoutNs);

            if (waitResult != VK_SUCCESS)
            {
                log_debug(root, "- return result {}", GetResultToString(waitResult));
            }
        } while (waitResult != VK_SUCCESS);

        Assert(vkGetFenceStatus(backend.device, drawFence) == VK_SUCCESS);

        log_debug(root, "vulkan: reset fence");
        Assert(vkResetFences(backend.device, 1, &drawFence) == VK_SUCCESS);
    }

    const VkExtent2D backbufferExtent = backend.presentInfo.surfaceExtent;

    FrameData frame_data = {};
    frame_data.backbufferExtent = backbufferExtent;

    // FIXME Recreate the swapchain pipeline every frame
    reload_swapchain_pipeline(backend, resources.shader_modules, resources.swapchain_pass_resources);

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

#if 0
    VkHdrMetadataEXT hdrMetaData = {};
    hdrMetaData.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT, hdrMetaData.pNext = nullptr,
    hdrMetaData.displayPrimaryRed.x = 0.708f;
    hdrMetaData.displayPrimaryRed.y = 0.292f;
    hdrMetaData.displayPrimaryGreen.x = 0.170f;
    hdrMetaData.displayPrimaryGreen.y = 0.797f;
    hdrMetaData.displayPrimaryBlue.x = 0.131f;
    hdrMetaData.displayPrimaryBlue.y = 0.046f;
    hdrMetaData.minLuminance = 0.0f;
    hdrMetaData.maxLuminance =
        10000.0f; // This will cause tonemapping to happen on display end as long as it's greater than display's actual
                  // queried max luminance. The look will change and it will be display dependent!
    hdrMetaData.maxContentLightLevel = 10000.0f;
    hdrMetaData.maxFrameAverageLightLevel =
        400.0f; // max and average content light level data will be used to do tonemapping on display

    vkSetHdrMetadataEXT(backend.device, 1, &backend.presentInfo.swapchain, &hdrMetaData);
#endif

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

    {
        const GPUResourceUsage draw_counter_usage = {
            .access = GPUResourceAccess{VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT},
            .buffer_view = default_buffer_view(debug_geometry_counter_properties)};

        debug_geometry_clear.draw_counter =
            builder.create_buffer(debug_geometry_clear.pass_handle, "Debug Indirect draw counter buffer",
                                  debug_geometry_counter_properties, draw_counter_usage);
    }

    const GPUBufferProperties debug_geometry_user_commands_properties = DefaultGPUBufferProperties(
        DebugGeometryCountMax, sizeof(DebugGeometryUserCommand), GPUBufferUsage::StorageBuffer);

    {
        // Technically we shouldn't create an usage here, the first client of the debug geometry API should call
        // create_buffer() with the right data. But it makes it slightly simpler this way for the user API so I'm taking
        // the trade-off and paying for an extra useless barrier.
        const GPUResourceUsage user_commands_usage = {
            .access = GPUResourceAccess{VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT},
            .buffer_view = default_buffer_view(debug_geometry_user_commands_properties)};

        debug_geometry_clear.user_commands_buffer =
            builder.create_buffer(debug_geometry_clear.pass_handle, "Debug geometry user command buffer",
                                  debug_geometry_user_commands_properties, user_commands_usage);
    }

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
        const GPUResourceUsage shadow_map_usage = {.access = {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                                              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                              VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
                                                   .texture_view = DefaultGPUTextureView(properties)};

        const ResourceUsageHandle usage_handle =
            builder.create_texture(shadow.pass_handle, "Shadow map", properties, shadow_map_usage);

        shadow.shadow_maps.push_back(usage_handle);
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

    // Depth Downsample
    struct TileDepthFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle depth_read_usage_handle;
        ResourceUsageHandle depth_min_storage_usage_handle;
        ResourceUsageHandle depth_max_storage_usage_handle;
    } tile_depth;

    tile_depth.pass_handle = builder.create_render_pass("Depth Downsample");

    {
        const GPUResourceUsage depth_read_usage = {.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                                               VK_ACCESS_2_SHADER_READ_BIT,
                                                                               VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL},
                                                   .texture_view = DefaultGPUTextureView(scene_depth_properties)};

        tile_depth.depth_read_usage_handle =
            builder.read_texture(tile_depth.pass_handle, visibility.depth, depth_read_usage);
    }

    const GPUTextureProperties tile_depth_storage_properties =
        DefaultGPUTextureProperties(tiled_lighting_frame.tile_count_x, tiled_lighting_frame.tile_count_y,
                                    PixelFormat::R16_UNORM, GPUTextureUsage::Storage | GPUTextureUsage::Sampled);

    GPUResourceUsage tile_depth_create_usage = {};
    tile_depth_create_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                       VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL};
    tile_depth_create_usage.texture_view = DefaultGPUTextureView(tile_depth_storage_properties);

    tile_depth.depth_min_storage_usage_handle = builder.create_texture(
        tile_depth.pass_handle, "Tile Depth Min Storage", tile_depth_storage_properties, tile_depth_create_usage);

    tile_depth.depth_max_storage_usage_handle = builder.create_texture(
        tile_depth.pass_handle, "Tile Depth Max Storage", tile_depth_storage_properties, tile_depth_create_usage);

    // Depth copy
    struct TileDepthCopyFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle depth_min_src_usage_handle;
        ResourceUsageHandle depth_max_src_usage_handle;
        ResourceUsageHandle depth_min_usage_handle;
        ResourceUsageHandle depth_max_usage_handle;
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

    tile_depth_copy.depth_min_src_usage_handle = builder.read_texture(
        tile_depth_copy.pass_handle, tile_depth.depth_min_storage_usage_handle, tile_depth_copy_src_usage);
    tile_depth_copy.depth_max_src_usage_handle = builder.read_texture(
        tile_depth_copy.pass_handle, tile_depth.depth_max_storage_usage_handle, tile_depth_copy_src_usage);

    tile_depth_copy.depth_min_usage_handle = builder.create_texture(
        tile_depth_copy.pass_handle, "Tile Depth Min", tile_depth_copy_properties, tile_depth_copy_dst_usage);
    tile_depth_copy.depth_max_usage_handle = builder.create_texture(
        tile_depth_copy.pass_handle, "Tile Depth Max", tile_depth_copy_properties, tile_depth_copy_dst_usage);

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

    GPUResourceUsage classification_counters_write_usage = {};
    classification_counters_write_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
    classification_counters_write_usage.buffer_view = default_buffer_view(classification_counters_properties);

    light_classify.classification_counters = builder.write_buffer(
        light_classify.pass_handle, tile_depth_copy.classification_counters_clear, classification_counters_write_usage);

    const GPUBufferProperties draw_command_classify_properties =
        DefaultGPUBufferProperties(TiledRasterMaxIndirectCommandCount, 4 * sizeof(u32), // FIXME
                                   GPUBufferUsage::StorageBuffer | GPUBufferUsage::IndirectBuffer);

    GPUResourceUsage draw_command_classify_usage = {};
    draw_command_classify_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
    draw_command_classify_usage.buffer_view = default_buffer_view(draw_command_classify_properties);

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
        ResourceUsageHandle tile_depth_min_usage_handle;
        ResourceUsageHandle tile_depth_max_usage_handle;
        ResourceUsageHandle light_list_usage_handle;
    } light_raster;

    light_raster.pass_handle = builder.create_render_pass("Rasterize Light Volumes");

    GPUResourceUsage classification_counters_read_usage = {};
    classification_counters_read_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT};
    classification_counters_read_usage.buffer_view = default_buffer_view(classification_counters_properties);

    light_raster.command_counters = builder.read_buffer(
        light_raster.pass_handle, light_classify.classification_counters, classification_counters_read_usage);

    GPUResourceUsage draw_command_raster_read_usage = {};
    draw_command_raster_read_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT};
    draw_command_raster_read_usage.buffer_view = default_buffer_view(draw_command_classify_properties);

    light_raster.draw_commands_inner = builder.read_buffer(light_raster.pass_handle, light_classify.draw_commands_inner,
                                                           draw_command_raster_read_usage);
    light_raster.draw_commands_outer = builder.read_buffer(light_raster.pass_handle, light_classify.draw_commands_outer,
                                                           draw_command_raster_read_usage);

    {
        GPUResourceUsage tile_depth_usage = {};
        tile_depth_usage.access = GPUResourceAccess{
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL};
        tile_depth_usage.texture_view = DefaultGPUTextureView(tile_depth_copy_properties);

        light_raster.tile_depth_min_usage_handle =
            builder.read_texture(light_raster.pass_handle, tile_depth_copy.depth_min_usage_handle, tile_depth_usage);
        light_raster.tile_depth_max_usage_handle =
            builder.read_texture(light_raster.pass_handle, tile_depth_copy.depth_max_usage_handle, tile_depth_usage);
    }

    {
        GPUResourceUsage light_list_usage = {};
        light_list_usage.access =
            GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        light_list_usage.buffer_view = default_buffer_view(light_list_properties);

        light_raster.light_list_usage_handle = builder.write_buffer(
            light_raster.pass_handle, tile_depth_copy.light_list_clear_usage_handle, light_list_usage);
    }

    // Tiled Lighting
    struct TiledLightingFrameGraphData
    {
        RenderPassHandle                 pass_handle;
        std::vector<ResourceUsageHandle> shadow_maps;
        ResourceUsageHandle              light_list_usage_handle;
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

        tiled_lighting.light_list_usage_handle =
            builder.read_buffer(tiled_lighting.pass_handle, light_raster.light_list_usage_handle, light_list_usage);
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

    GPUResourceUsage tile_debug_read_usage = {};
    tile_debug_read_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
    tile_debug_read_usage.buffer_view = default_buffer_view(tile_debug_properties);

    tiled_lighting_debug.tile_debug = builder.read_buffer(
        tiled_lighting_debug.pass_handle, tiled_lighting.tile_debug_usage_handle, tile_debug_read_usage);

    const GPUTextureProperties tiled_debug_properties =
        DefaultGPUTextureProperties(backbufferExtent.width, backbufferExtent.height, PixelFormat::R8G8B8A8_UNORM,
                                    GPUTextureUsage::Storage | GPUTextureUsage::Sampled);

    {
        GPUResourceUsage tile_debug_usage = {};
        tile_debug_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                    VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL};
        tile_debug_usage.texture_view = DefaultGPUTextureView(tiled_debug_properties);

        tiled_lighting_debug.output = builder.create_texture(
            tiled_lighting_debug.pass_handle, "Tiled Lighting Debug Texture", tiled_debug_properties, tile_debug_usage);
    }

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

    const GPUResourceAccess scene_hdr_access_forward_pass = {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                             VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                             VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL};

    const GPUResourceUsage scene_hdr_texture_usage = {.access = scene_hdr_access_forward_pass,
                                                      .texture_view = DefaultGPUTextureView(scene_hdr_properties)};

    forward.scene_hdr =
        builder.create_texture(forward.pass_handle, "Scene HDR", scene_hdr_properties, scene_hdr_texture_usage);

    const GPUResourceUsage forward_depth_usage = {
        .access = GPUResourceAccess{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL},
        .texture_view = DefaultGPUTextureView(scene_depth_properties)};

    forward.depth = builder.write_texture(forward.pass_handle, visibility.depth, forward_depth_usage);

    for (u32 shadow_map_index = 0; shadow_map_index < shadow_map_properties.size(); shadow_map_index++)
    {
        GPUResourceUsage shadow_map_usage = {};
        shadow_map_usage.access = {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                   VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
        shadow_map_usage.texture_view = DefaultGPUTextureView(shadow_map_properties[shadow_map_index]);

        const ResourceUsageHandle usage_handle =
            builder.read_texture(forward.pass_handle, shadow.shadow_maps[shadow_map_index], shadow_map_usage);
        forward.shadow_maps.push_back(usage_handle);
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

    GPUResourceUsage gui_texture_usage = {};
    gui_texture_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL};
    gui_texture_usage.texture_view = DefaultGPUTextureView(gui_properties);

    gui.output = builder.create_texture(gui.pass_handle, "GUI SDR", gui_properties, gui_texture_usage);

    // Histogram Clear
    struct HistogramClearFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle histogram_buffer;
    } histogram_clear;

    histogram_clear.pass_handle = builder.create_render_pass("Histogram Clear");

    const GPUBufferProperties histogram_buffer_properties = DefaultGPUBufferProperties(
        HistogramRes, sizeof(u32), GPUBufferUsage::StorageBuffer | GPUBufferUsage::TransferDst);

    GPUResourceUsage histogram_clear_usage = {};
    histogram_clear_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
    histogram_clear_usage.buffer_view = default_buffer_view(histogram_buffer_properties);

    histogram_clear.histogram_buffer = builder.create_buffer(histogram_clear.pass_handle, "Histogram Buffer",
                                                             histogram_buffer_properties, histogram_clear_usage);

    // Histogram
    struct HistogramFrameGraphData
    {
        RenderPassHandle    pass_handle;
        ResourceUsageHandle scene_hdr;
        ResourceUsageHandle histogram_buffer;
    } histogram;

    histogram.pass_handle = builder.create_render_pass("Histogram");

    GPUResourceUsage histogram_hdr_usage = {};
    histogram_hdr_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                   VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    histogram_hdr_usage.texture_view = DefaultGPUTextureView(scene_hdr_properties);

    histogram.scene_hdr = builder.read_texture(histogram.pass_handle, forward.scene_hdr, histogram_hdr_usage);

    GPUResourceUsage histogram_write_usage = {};
    histogram_write_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
    histogram_write_usage.buffer_view = default_buffer_view(histogram_buffer_properties);

    histogram.histogram_buffer =
        builder.write_buffer(histogram.pass_handle, histogram_clear.histogram_buffer, histogram_write_usage);

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

    {
        GPUResourceUsage draw_counter_usage = {};
        draw_counter_usage.access =
            GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        draw_counter_usage.buffer_view = default_buffer_view(debug_geometry_counter_properties);

        debug_geometry_build_cmds.draw_counter = builder.read_buffer(
            debug_geometry_build_cmds.pass_handle, debug_geometry_clear.draw_counter, draw_counter_usage);
    }

    {
        GPUResourceUsage user_commands_usage = {};
        user_commands_usage.access =
            GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        user_commands_usage.buffer_view = default_buffer_view(debug_geometry_user_commands_properties);

        debug_geometry_build_cmds.user_commands_buffer = builder.read_buffer(
            debug_geometry_build_cmds.pass_handle, debug_geometry_clear.user_commands_buffer, user_commands_usage);
    }

    const GPUBufferProperties debug_geometry_command_properties =
        DefaultGPUBufferProperties(DebugGeometryCountMax, sizeof(VkDrawIndexedIndirectCommand),
                                   GPUBufferUsage::IndirectBuffer | GPUBufferUsage::StorageBuffer);

    {
        GPUResourceUsage draw_commands_usage = {};
        draw_commands_usage.access =
            GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        draw_commands_usage.buffer_view = default_buffer_view(debug_geometry_command_properties);

        debug_geometry_build_cmds.draw_commands =
            builder.create_buffer(debug_geometry_build_cmds.pass_handle, "Debug Indirect draw command buffer",
                                  debug_geometry_command_properties, draw_commands_usage);
    }

    const GPUBufferProperties debug_geometry_instance_properties =
        DefaultGPUBufferProperties(DebugGeometryCountMax, sizeof(DebugGeometryInstance), GPUBufferUsage::StorageBuffer);

    {
        GPUResourceUsage instance_buffer_usage = {};
        instance_buffer_usage.access =
            GPUResourceAccess{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT};
        instance_buffer_usage.buffer_view = default_buffer_view(debug_geometry_instance_properties);

        debug_geometry_build_cmds.instance_buffer =
            builder.create_buffer(debug_geometry_build_cmds.pass_handle, "Debug geometry instance buffer",
                                  debug_geometry_instance_properties, instance_buffer_usage);
    }

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

    {
        GPUResourceUsage scene_hdr_usage = {};
        scene_hdr_usage.access =
            GPUResourceAccess{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        scene_hdr_usage.texture_view = DefaultGPUTextureView(scene_hdr_properties);

        debug_geometry_draw.scene_hdr = builder.write_texture(debug_geometry_draw.pass_handle,
                                                              tiled_lighting.lighting_usage_handle, scene_hdr_usage);
    }

    {
        GPUResourceUsage depth_usage = {};
        depth_usage.access = GPUResourceAccess{
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL};
        depth_usage.texture_view = DefaultGPUTextureView(scene_depth_properties);

        debug_geometry_draw.scene_depth =
            builder.read_texture(debug_geometry_draw.pass_handle, forward.depth, depth_usage);
    }

    {
        GPUResourceUsage draw_counter_usage = {};
        draw_counter_usage.access =
            GPUResourceAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT};
        draw_counter_usage.buffer_view = default_buffer_view(debug_geometry_counter_properties);

        debug_geometry_draw.draw_counter =
            builder.read_buffer(debug_geometry_draw.pass_handle, debug_geometry_clear.draw_counter, draw_counter_usage);
    }

    {
        GPUResourceUsage draw_commands_usage = {};
        draw_commands_usage.access =
            GPUResourceAccess{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT};
        draw_commands_usage.buffer_view = default_buffer_view(debug_geometry_command_properties);

        debug_geometry_draw.draw_commands = builder.read_buffer(
            debug_geometry_draw.pass_handle, debug_geometry_build_cmds.draw_commands, draw_commands_usage);
    }

    {
        GPUResourceUsage instance_buffer_usage = {};
        instance_buffer_usage.access =
            GPUResourceAccess{VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        instance_buffer_usage.buffer_view = default_buffer_view(debug_geometry_instance_properties);

        debug_geometry_draw.instance_buffer = builder.read_buffer(
            debug_geometry_draw.pass_handle, debug_geometry_build_cmds.instance_buffer, instance_buffer_usage);
    }

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

    GPUResourceUsage swapchain_hdr_usage = {};
    swapchain_hdr_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                   VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    swapchain_hdr_usage.texture_view = DefaultGPUTextureView(scene_hdr_properties);

    swapchain.scene_hdr = builder.read_texture(swapchain.pass_handle, forward.scene_hdr, swapchain_hdr_usage);

    GPUResourceUsage swapchain_l_usage = {};
    swapchain_l_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                 VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    swapchain_l_usage.texture_view = DefaultGPUTextureView(lighting_properties);

    swapchain.lighting_result =
        builder.read_texture(swapchain.pass_handle, debug_geometry_draw.scene_hdr, swapchain_l_usage);

    GPUResourceUsage swapchain_gui_usage = {};
    swapchain_gui_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                                   VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    swapchain_gui_usage.texture_view = DefaultGPUTextureView(gui_properties);

    swapchain.gui = builder.read_texture(swapchain.pass_handle, gui.output, swapchain_gui_usage);

    GPUResourceUsage swapchain_histogram_buffer_usage = {};
    swapchain_histogram_buffer_usage.access =
        GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
    swapchain_histogram_buffer_usage.buffer_view = default_buffer_view(histogram_buffer_properties);

    swapchain.histogram =
        builder.read_buffer(swapchain.pass_handle, histogram.histogram_buffer, swapchain_histogram_buffer_usage);

    {
        GPUResourceUsage tile_debug_usage = {};
        tile_debug_usage.access = GPUResourceAccess{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                    VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
        tile_debug_usage.texture_view = DefaultGPUTextureView(tiled_debug_properties);

        swapchain.tile_debug =
            builder.read_texture(swapchain.pass_handle, tiled_lighting_debug.output, tile_debug_usage);
    }

    builder.build();

    // DumpFrameGraph(framegraph);

    const FrameGraphSchedule schedule = compute_schedule(framegraph);

    log_barriers(root, framegraph, schedule);

    allocate_framegraph_volatile_resources(root, backend, resources.framegraph_resources, framegraph);

    DescriptorWriteHelper descriptor_write_helper = create_descriptor_write_helper(200, 200);

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
        resources.samplers_resources,
        resources.material_resources,
        resources.meshlet_culling_resources,
        resources.mesh_cache,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, visibility_gbuffer.vis_buffer),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, visibility_gbuffer.gbuffer_rt0),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, visibility_gbuffer.gbuffer_rt1));

    update_forward_pass_descriptor_sets(descriptor_write_helper, resources.forward_pass_resources,
                                        resources.samplers_resources, resources.material_resources,
                                        resources.mesh_cache, resources.lighting_resources, forward_shadow_map_views);

    update_lighting_depth_downsample_descriptor_set(
        descriptor_write_helper, resources.tiled_raster_resources, resources.samplers_resources,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tile_depth.depth_read_usage_handle),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tile_depth.depth_min_storage_usage_handle),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tile_depth.depth_max_storage_usage_handle));

    update_depth_copy_pass_descriptor_set(
        descriptor_write_helper, resources.tiled_raster_resources,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, tile_depth_copy.depth_min_src_usage_handle),
        get_frame_graph_texture(resources.framegraph_resources, framegraph,
                                tile_depth_copy.depth_max_src_usage_handle));

    update_classify_descriptor_set(
        descriptor_write_helper, resources.tiled_raster_resources,
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, light_classify.classification_counters),
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, light_classify.draw_commands_inner),
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, light_classify.draw_commands_outer));

    update_light_raster_pass_descriptor_sets(
        descriptor_write_helper, resources.tiled_raster_resources,
        get_frame_graph_texture(resources.framegraph_resources, framegraph, light_raster.tile_depth_min_usage_handle),
        get_frame_graph_texture(resources.framegraph_resources, framegraph, light_raster.tile_depth_max_usage_handle),
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, light_raster.light_list_usage_handle));

    std::vector<FrameGraphTexture> tiled_shadow_maps;
    for (auto handle : tiled_lighting.shadow_maps)
    {
        tiled_shadow_maps.emplace_back(get_frame_graph_texture(resources.framegraph_resources, framegraph, handle));
    }

    update_tiled_lighting_pass_descriptor_sets(
        descriptor_write_helper, resources.lighting_resources, resources.tiled_lighting_resources,
        resources.samplers_resources,
        get_frame_graph_buffer(resources.framegraph_resources, framegraph, tiled_lighting.light_list_usage_handle),
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

    flush_descriptor_write_helper(descriptor_write_helper, backend.device);

    log_debug(root, "vulkan: record command buffer");
    Assert(vkResetCommandBuffer(cmdBuffer.handle, 0) == VK_SUCCESS);

    VkCommandBufferBeginInfo cmdBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
        0,      // Not caring yet
        nullptr // No inheritance yet
    };

    Assert(vkBeginCommandBuffer(cmdBuffer.handle, &cmdBufferBeginInfo) == VK_SUCCESS);

#if defined(REAPER_USE_MICROPROFILE)
    MicroProfileFlip(cmdBuffer.handle);

    cmdBuffer.mlog = MicroProfileThreadLogGpuAlloc();
    MICROPROFILE_GPU_BEGIN(cmdBuffer.handle, cmdBuffer.mlog);
#endif

    {
        REAPER_GPU_SCOPE(cmdBuffer, "GPU Frame");

        if (backend.mustTransitionSwapchain)
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

            backend.mustTransitionSwapchain = false;
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

            record_depth_copy(cmdBuffer, resources.tiled_raster_resources,
                              get_frame_graph_texture(resources.framegraph_resources, framegraph,
                                                      tile_depth_copy.depth_min_usage_handle),
                              get_frame_graph_texture(resources.framegraph_resources, framegraph,
                                                      tile_depth_copy.depth_max_usage_handle));

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
                get_frame_graph_texture(resources.framegraph_resources, framegraph,
                                        light_raster.tile_depth_min_usage_handle),
                get_frame_graph_texture(resources.framegraph_resources, framegraph,
                                        light_raster.tile_depth_max_usage_handle));

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

#if defined(REAPER_USE_MICROPROFILE)
    const u64 microprofile_data = MicroProfileGpuEnd(cmdBuffer.mlog);
    MicroProfileThreadLogGpuFree(cmdBuffer.mlog);

    MICROPROFILE_GPU_SUBMIT(MicroProfileGetGlobalGpuQueue(), microprofile_data);
#endif

    // Stop recording
    Assert(vkEndCommandBuffer(cmdBuffer.handle) == VK_SUCCESS);

    VkPipelineStageFlags waitDstMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    std::array<VkSemaphore, 1> semaphores_to_signal = {backend.presentInfo.renderingFinishedSemaphore};

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               nullptr,
                               1,
                               &backend.presentInfo.imageAvailableSemaphore,
                               &waitDstMask,
                               1,
                               &cmdBuffer.handle,
                               semaphores_to_signal.size(),
                               semaphores_to_signal.data()};

    log_debug(root, "vulkan: submit drawing commands");
    Assert(vkQueueSubmit(backend.deviceInfo.graphicsQueue, 1, &submitInfo, resources.frame_sync_resources.drawFence)
           == VK_SUCCESS);

    log_debug(root, "vulkan: present");

    VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                    nullptr,
                                    1,
                                    &backend.presentInfo.renderingFinishedSemaphore,
                                    1,
                                    &backend.presentInfo.swapchain,
                                    &current_swapchain_index,
                                    nullptr};

    Assert(!backend.mustTransitionSwapchain);

    VkResult presentResult = vkQueuePresentKHR(backend.deviceInfo.presentQueue, &presentInfo);

    if (presentResult == VK_SUBOPTIMAL_KHR)
    {
        backend.new_swapchain_extent = backend.presentInfo.surfaceExtent;
        backend.mustTransitionSwapchain = true;
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

    // VkResult acquireFullscreenResult = vkAcquireFullScreenExclusiveModeEXT(backend.device,
    // backend.presentInfo.swapchain); Assert(acquireFullscreenResult == VK_SUCCESS);

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
