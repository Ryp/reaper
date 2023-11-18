////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TestGraphics.h"

#include "renderer/graph/FrameGraphBuilder.h"
#include "renderer/graph/GraphDebug.h"
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
#include "renderer/vulkan/renderpass/FrameGraphPass.h"
#include "renderer/vulkan/renderpass/HZBPass.h"
#include "renderer/vulkan/renderpass/TiledLightingCommon.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include "core/BitTricks.h"
#include "core/memory/Allocator.h"
#include "profiling/Scope.h"

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
    FrameGraph::Builder    builder(framegraph);

    const CullMeshletsFrameGraphRecord meshlet_pass = create_cull_meshlet_frame_graph_record(builder);

    const DebugGeometryClearFrameGraphRecord debug_geometry_clear = create_debug_geometry_clear_pass_record(builder);

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

    const ForwardFrameGraphRecord forward =
        create_forward_pass_record(builder, meshlet_pass, shadow, vis_buffer_record.render.depth, render_extent);

    const GUIFrameGraphRecord gui = create_gui_pass_record(builder, backend.presentInfo.surface_extent);

    const HistogramClearFrameGraphRecord histogram_clear = create_histogram_clear_pass_record(builder);

    const HistogramFrameGraphRecord histogram =
        create_histogram_pass_record(builder, histogram_clear, forward.scene_hdr);

    const DebugGeometryComputeFrameGraphRecord debug_geometry_build_cmds =
        create_debug_geometry_compute_pass_record(builder, debug_geometry_clear);

    const DebugGeometryDrawFrameGraphRecord debug_geometry_draw = create_debug_geometry_draw_pass_record(
        builder, debug_geometry_clear, debug_geometry_build_cmds, tiled_lighting.lighting, forward.depth);

    const SwapchainFrameGraphRecord swapchain = create_swapchain_pass_record(builder,
                                                                             forward.scene_hdr,
                                                                             debug_geometry_draw.scene_hdr,
                                                                             gui.output,
                                                                             histogram.histogram_buffer,
                                                                             tiled_lighting_debug_record.output);

    const AudioFrameGraphRecord audio_pass = create_audio_frame_graph_record(builder);

    builder.build();
    // DumpFrameGraph(framegraph);

    allocate_framegraph_volatile_resources(backend, resources.framegraph_resources, framegraph);

    DescriptorWriteHelper descriptor_write_helper(200, 200);

    upload_lighting_pass_frame_resources(resources.frame_storage_allocator, prepared, resources.lighting_resources);

    update_shadow_map_resources(descriptor_write_helper, resources.frame_storage_allocator, prepared,
                                resources.shadow_map_resources, resources.mesh_cache.vertexBufferPosition);

    update_vis_buffer_pass_resources(framegraph, resources.framegraph_resources, vis_buffer_record,
                                     descriptor_write_helper, resources.frame_storage_allocator,
                                     resources.vis_buffer_pass_resources, prepared, resources.samplers_resources,
                                     resources.material_resources, resources.mesh_cache);

    update_tiled_lighting_raster_pass_resources(framegraph, resources.framegraph_resources, light_raster_record,
                                                descriptor_write_helper, resources.frame_storage_allocator,
                                                resources.tiled_raster_resources, tiled_lighting_frame);

    update_meshlet_culling_passes_resources(framegraph, resources.framegraph_resources, meshlet_pass,
                                            descriptor_write_helper, resources.frame_storage_allocator, prepared,
                                            resources.meshlet_culling_resources, resources.mesh_cache);

    update_hzb_pass_descriptor_set(framegraph, resources.framegraph_resources, hzb_reduce, descriptor_write_helper,
                                   resources.hzb_pass_resources, resources.samplers_resources);

    update_forward_pass_descriptor_sets(backend, framegraph, resources.framegraph_resources, forward,
                                        descriptor_write_helper, prepared, resources.forward_pass_resources,
                                        resources.samplers_resources, resources.material_resources,
                                        resources.mesh_cache, resources.lighting_resources);

    update_tiled_lighting_pass_resources(backend, framegraph, resources.framegraph_resources, tiled_lighting,
                                         descriptor_write_helper, prepared, resources.lighting_resources,
                                         resources.tiled_lighting_resources, resources.samplers_resources);

    update_tiled_lighting_debug_pass_resources(framegraph, resources.framegraph_resources, tiled_lighting_debug_record,
                                               descriptor_write_helper, resources.tiled_lighting_resources);

    update_histogram_pass_descriptor_set(framegraph, resources.framegraph_resources, histogram, descriptor_write_helper,
                                         resources.histogram_pass_resources, resources.samplers_resources);

    update_debug_geometry_build_cmds_pass_resources(backend, framegraph, resources.framegraph_resources,
                                                    debug_geometry_build_cmds, descriptor_write_helper, prepared,
                                                    resources.debug_geometry_resources);

    update_debug_geometry_draw_pass_descriptor_sets(framegraph, resources.framegraph_resources, debug_geometry_draw,
                                                    descriptor_write_helper, resources.debug_geometry_resources);

    update_swapchain_pass_descriptor_set(framegraph, resources.framegraph_resources, swapchain, descriptor_write_helper,
                                         resources.swapchain_pass_resources, resources.samplers_resources);

    update_audio_render_resources(backend, framegraph, resources.framegraph_resources, audio_pass,
                                  descriptor_write_helper, prepared, resources.audio_resources);

    storage_allocator_commit_to_gpu(backend, resources.frame_storage_allocator);

    descriptor_write_helper.flush_descriptor_write_helper(backend.device);

    const FrameGraph::FrameGraphSchedule schedule = compute_schedule(framegraph);

    log_barriers(root, framegraph, schedule);

    const FrameGraphHelper frame_graph_helper = {
        .frame_graph = framegraph,
        .schedule = schedule,
        .resources = resources.framegraph_resources,
    };

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

            const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper,
                                                                  meshlet_pass.clear.pass_handle);

            FrameGraphBuffer meshlet_counters =
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, meshlet_pass.clear.meshlet_counters);

            const u32 clear_value = 0;
            vkCmdFillBuffer(cmdBuffer.handle, meshlet_counters.handle, 0, VK_WHOLE_SIZE, clear_value);
        }

        record_meshlet_culling_command_buffer(root, frame_graph_helper, meshlet_pass.cull_meshlets, cmdBuffer, prepared,
                                              resources.meshlet_culling_resources);

        record_triangle_culling_prepare_command_buffer(frame_graph_helper, meshlet_pass.cull_triangles_prepare,
                                                       cmdBuffer, prepared, resources.meshlet_culling_resources);

        record_triangle_culling_command_buffer(frame_graph_helper, meshlet_pass.cull_triangles, cmdBuffer, prepared,
                                               resources.meshlet_culling_resources);

        record_meshlet_culling_debug_command_buffer(frame_graph_helper, meshlet_pass.debug, cmdBuffer,
                                                    resources.meshlet_culling_resources);

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Debug Geometry Clear");

            const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper,
                                                                  debug_geometry_clear.pass_handle);

            const u32        clear_value = 0;
            FrameGraphBuffer draw_counter =
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, debug_geometry_clear.draw_counter);

            vkCmdFillBuffer(cmdBuffer.handle, draw_counter.handle, draw_counter.default_view.offset_bytes,
                            draw_counter.default_view.size_bytes, clear_value);
        }

        record_shadow_map_command_buffer(frame_graph_helper, shadow, cmdBuffer, prepared,
                                         resources.shadow_map_resources);

        record_vis_buffer_pass_command_buffer(frame_graph_helper, vis_buffer_record.render, cmdBuffer, prepared,
                                              resources.vis_buffer_pass_resources);

        record_hzb_command_buffer(
            frame_graph_helper, hzb_reduce, cmdBuffer, resources.hzb_pass_resources,
            VkExtent2D{.width = vis_buffer_record.scene_depth_properties.width,
                       .height = vis_buffer_record.scene_depth_properties.height},
            VkExtent2D{.width = hzb_reduce.hzb_properties.width, .height = hzb_reduce.hzb_properties.height});

        record_fill_gbuffer_pass_command_buffer(frame_graph_helper, vis_buffer_record.fill_gbuffer, cmdBuffer,
                                                resources.vis_buffer_pass_resources, render_extent);

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Tile Depth Copy");

            const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper,
                                                                  light_raster_record.tile_depth_copy.pass_handle);

            record_depth_copy(frame_graph_helper, light_raster_record.tile_depth_copy, cmdBuffer,
                              resources.tiled_raster_resources);

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
        }

        record_light_classify_command_buffer(frame_graph_helper, light_raster_record.light_classify, cmdBuffer,
                                             tiled_lighting_frame, resources.tiled_raster_resources);

        record_light_raster_command_buffer(frame_graph_helper, light_raster_record.light_raster, cmdBuffer,
                                           resources.tiled_raster_resources.light_raster);

        record_tiled_lighting_command_buffer(frame_graph_helper, tiled_lighting, cmdBuffer,
                                             resources.tiled_lighting_resources, render_extent,
                                             VkExtent2D{light_raster_record.tile_depth_properties.width,
                                                        light_raster_record.tile_depth_properties.height});

        record_tiled_lighting_debug_command_buffer(frame_graph_helper, tiled_lighting_debug_record, cmdBuffer,
                                                   resources.tiled_lighting_resources, render_extent,
                                                   VkExtent2D{light_raster_record.tile_depth_properties.width,
                                                              light_raster_record.tile_depth_properties.height});

        record_forward_pass_command_buffer(frame_graph_helper, forward, cmdBuffer, prepared,
                                           resources.forward_pass_resources);

        record_gui_command_buffer(frame_graph_helper, gui, cmdBuffer, resources.gui_pass_resources, imgui_draw_data);

        {
            REAPER_GPU_SCOPE(cmdBuffer, "Histogram Clear");

            const FrameGraphBarrierScope framegraph_barrier_scope(cmdBuffer, frame_graph_helper,
                                                                  histogram_clear.pass_handle);

            const u32        clear_value = 0;
            FrameGraphBuffer histogram_buffer =
                get_frame_graph_buffer(resources.framegraph_resources, framegraph, histogram_clear.histogram_buffer);

            vkCmdFillBuffer(cmdBuffer.handle, histogram_buffer.handle, histogram_buffer.default_view.offset_bytes,
                            histogram_buffer.default_view.size_bytes, clear_value);
        }

        record_histogram_command_buffer(frame_graph_helper, histogram, cmdBuffer, resources.histogram_pass_resources,
                                        render_extent);

        record_debug_geometry_build_cmds_command_buffer(frame_graph_helper, debug_geometry_build_cmds, cmdBuffer,
                                                        resources.debug_geometry_resources);

        record_debug_geometry_draw_command_buffer(frame_graph_helper, debug_geometry_draw, cmdBuffer,
                                                  resources.debug_geometry_resources);

        record_swapchain_command_buffer(frame_graph_helper, swapchain, cmdBuffer, resources.swapchain_pass_resources,
                                        backend.presentInfo.imageViews[current_swapchain_index],
                                        backend.presentInfo.surface_extent);

        {
            const GPUTextureAccess src = swapchain_access_render;
            const GPUTextureAccess dst = swapchain_access_present;

            const GPUTextureSubresource subresource = default_texture_subresource_one_color_mip();

            const VkImageMemoryBarrier2 barrier =
                get_vk_image_barrier(backend.presentInfo.images[current_swapchain_index], subresource, src, dst);

            const VkDependencyInfo dependencies = get_vk_image_barrier_depency_info(std::span(&barrier, 1));

            vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencies);
        }

        record_audio_render_command_buffer(frame_graph_helper, audio_pass.render, cmdBuffer, prepared,
                                           resources.audio_resources);

        record_audio_copy_command_buffer(frame_graph_helper, audio_pass.staging_copy, cmdBuffer,
                                         resources.audio_resources);
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
