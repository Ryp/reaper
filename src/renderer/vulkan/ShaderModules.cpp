////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ShaderModules.h"

#include "Backend.h"
#include "Debug.h"

#include <common/ReaperRoot.h>

#include <core/Assert.h>
#include <core/fs/FileLoading.h>

namespace Reaper
{
namespace
{
    VkShaderModule create_shader_module(VkDevice device, const std::string& fileName)
    {
        std::string       filename_with_root = "build/shader/" + fileName;
        std::vector<char> fileContents = readWholeFile(filename_with_root);

        VkShaderModuleCreateInfo shader_module_create_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
                                                              fileContents.size(),
                                                              reinterpret_cast<const uint32_t*>(&fileContents[0])};

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        Assert(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shaderModule) == VK_SUCCESS);

        VulkanSetDebugName(device, shaderModule, filename_with_root.c_str());
        return shaderModule;
    }
} // namespace

ShaderModules create_shader_modules(ReaperRoot& /*root*/, VulkanBackend& backend)
{
    ShaderModules modules = {};

    modules.copy_to_depth_fs = create_shader_module(backend.device, "copy_to_depth.frag.spv");
    modules.cull_meshlet_cs = create_shader_module(backend.device, "meshlet/cull_meshlet.comp.spv");
    modules.cull_triangle_batch_cs = create_shader_module(backend.device, "meshlet/cull_triangle_batch.comp.spv");
    modules.forward_fs = create_shader_module(backend.device, "forward.frag.spv");
    modules.forward_vs = create_shader_module(backend.device, "forward.vert.spv");
    modules.fullscreen_triangle_vs = create_shader_module(backend.device, "fullscreen_triangle.vert.spv");
    modules.gui_write_fs = create_shader_module(backend.device, "gui_write.frag.spv");
    modules.histogram_cs = create_shader_module(backend.device, "histogram.comp.spv");
    modules.oscillator_cs = create_shader_module(backend.device, "sound/oscillator.comp.spv");
    modules.prepare_fine_culling_indirect_cs =
        create_shader_module(backend.device, "meshlet/prepare_fine_culling_indirect.comp.spv");
    modules.rasterize_light_volume_fs =
        create_shader_module(backend.device, "tiled_lighting/rasterize_light_volume.frag.spv");
    modules.rasterize_light_volume_vs =
        create_shader_module(backend.device, "tiled_lighting/rasterize_light_volume.vert.spv");
    modules.render_shadow_vs = create_shader_module(backend.device, "render_shadow.vert.spv");
    modules.swapchain_write_fs = create_shader_module(backend.device, "swapchain_write.frag.spv");
    modules.tile_depth_downsample_cs =
        create_shader_module(backend.device, "tiled_lighting/tile_depth_downsample.comp.spv");
    modules.tiled_lighting_cs = create_shader_module(backend.device, "tiled_lighting/tiled_lighting.comp.spv");

    return modules;
}

void destroy_shader_modules(VulkanBackend& backend, ShaderModules& shader_modules)
{
    vkDestroyShaderModule(backend.device, shader_modules.copy_to_depth_fs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.cull_meshlet_cs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.cull_triangle_batch_cs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.forward_fs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.forward_vs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.fullscreen_triangle_vs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.gui_write_fs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.histogram_cs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.oscillator_cs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.prepare_fine_culling_indirect_cs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.rasterize_light_volume_fs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.rasterize_light_volume_vs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.render_shadow_vs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.swapchain_write_fs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.tile_depth_downsample_cs, nullptr);
    vkDestroyShaderModule(backend.device, shader_modules.tiled_lighting_cs, nullptr);
}
} // namespace Reaper
