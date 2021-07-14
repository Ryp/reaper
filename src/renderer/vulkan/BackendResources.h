////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CommandBuffer.h"
#include "FrameSync.h"
#include "MaterialResources.h"
#include "MeshCache.h"

#include "renderpass/Culling.h"
#include "renderpass/CullingConstants.h"
#include "renderpass/Frame.h"
#include "renderpass/HistogramPass.h"
#include "renderpass/MainPass.h"
#include "renderpass/ShadowMap.h"
#include "renderpass/SwapchainPass.h"

#include "api/Vulkan.h"

namespace Reaper
{
struct BackendResources
{
    MeshCache              mesh_cache;
    MaterialResources      material_resources;
    CullResources          cull_resources;
    ShadowMapResources     shadow_map_resources;
    MainPassResources      main_pass_resources;
    HistogramPassResources histogram_pass_resources;
    SwapchainPassResources swapchain_pass_resources;
    FrameSyncResources     frame_sync_resources;

    // FIXME wrap this
    VkCommandPool graphicsCommandPool;
    CommandBuffer gfxCmdBuffer;
};

REAPER_RENDERER_API void create_backend_resources(ReaperRoot& root, VulkanBackend& backend);
REAPER_RENDERER_API void destroy_backend_resources(VulkanBackend& backend);
} // namespace Reaper
