////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CommandBuffer.h"
#include "FrameGraphResources.h"
#include "FrameSync.h"
#include "MaterialResources.h"
#include "MeshCache.h"
#include "PipelineFactory.h"
#include "SamplerResources.h"
#include "ShaderModules.h"
#include "StorageBufferAllocator.h"

#include "renderpass/Audio.h"
#include "renderpass/DebugGeometryRenderPass.h"
#include "renderpass/ExposurePass.h"
#include "renderpass/ForwardPass.h"
#include "renderpass/GuiPass.h"
#include "renderpass/HZBPass.h"
#include "renderpass/HistogramPass.h"
#include "renderpass/LightingPass.h"
#include "renderpass/MeshletCulling.h"
#include "renderpass/ShadowMap.h"
#include "renderpass/SwapchainPass.h"
#include "renderpass/TiledLightingPass.h"
#include "renderpass/TiledRasterPass.h"
#include "renderpass/VisibilityBufferPass.h"

#include <vulkan_loader/Vulkan.h>

namespace Reaper
{
struct BackendResources
{
    // TODO remove *_resources suffix
    PipelineFactory               pipeline_factory;
    ShaderModules                 shader_modules;
    SamplerResources              samplers_resources;
    StorageBufferAllocator        frame_storage_allocator;
    DebugGeometryPassResources    debug_geometry_resources;
    FrameGraphResources           framegraph_resources;
    MeshCache                     mesh_cache;
    MaterialResources             material_resources;
    MeshletCullingResources       meshlet_culling_resources;
    VisibilityBufferPassResources vis_buffer_pass_resources;
    ShadowMapResources            shadow_map_resources;
    LightingPassResources         lighting_resources;
    TiledRasterResources          tiled_raster_resources;
    TiledLightingPassResources    tiled_lighting_resources;
    ForwardPassResources          forward_pass_resources;
    HistogramPassResources        histogram_pass_resources;
    ExposurePassResources         exposure_pass_resources;
    HZBPassResources              hzb_pass_resources;
    GuiPassResources              gui_pass_resources;
    SwapchainPassResources        swapchain_pass_resources;
    FrameSyncResources            frame_sync_resources;
    AudioResources                audio_resources;

    // FIXME wrap this
    VkCommandPool gfxCommandPool;
    CommandBuffer gfxCmdBuffer;
};

void create_backend_resources(ReaperRoot& root, VulkanBackend& backend);
void destroy_backend_resources(VulkanBackend& backend);
} // namespace Reaper
