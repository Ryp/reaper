////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vulkan_loader/Vulkan.h>

namespace Reaper
{
struct ShaderModules
{
    VkShaderModule copy_to_depth_fs;
    VkShaderModule copy_to_depth_from_hzb_fs;
    VkShaderModule cull_meshlet_cs;
    VkShaderModule cull_triangle_batch_cs;
    VkShaderModule debug_geometry_build_cmds_cs;
    VkShaderModule debug_geometry_draw_fs;
    VkShaderModule debug_geometry_draw_vs;
    VkShaderModule forward_fs;
    VkShaderModule forward_vs;
    VkShaderModule fullscreen_triangle_vs;
    VkShaderModule gbuffer_write_opaque_fs;
    VkShaderModule gbuffer_write_opaque_vs;
    VkShaderModule gui_write_fs;
    VkShaderModule histogram_cs;
    VkShaderModule hzb_reduce_cs;
    VkShaderModule oscillator_cs;
    VkShaderModule prepare_fine_culling_indirect_cs;
    VkShaderModule rasterize_light_volume_fs;
    VkShaderModule rasterize_light_volume_vs;
    VkShaderModule reduce_exposure_cs;
    VkShaderModule reduce_exposure_tail_cs;
    VkShaderModule render_shadow_vs;
    VkShaderModule swapchain_write_fs;
    VkShaderModule classify_volume_cs;
    VkShaderModule tile_depth_downsample_cs;
    VkShaderModule tiled_lighting_cs;
    VkShaderModule tiled_lighting_debug_cs;
    VkShaderModule vis_buffer_raster_fs;
    VkShaderModule vis_buffer_raster_vs;
    VkShaderModule vis_fill_gbuffer_cs;
};

struct VulkanBackend;

ShaderModules create_shader_modules(VulkanBackend& backend);
void          destroy_shader_modules(VulkanBackend& backend, ShaderModules& shader_modules);
} // namespace Reaper
