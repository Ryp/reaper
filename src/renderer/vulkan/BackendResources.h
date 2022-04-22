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

#include "renderpass/Audio.h"
#include "renderpass/Culling.h"
#include "renderpass/CullingConstants.h"
#include "renderpass/Frame.h"
#include "renderpass/GuiPass.h"
#include "renderpass/HistogramPass.h"
#include "renderpass/MainPass.h"
#include "renderpass/ShadowMap.h"
#include "renderpass/SwapchainPass.h"

#include "api/Vulkan.h"

namespace Reaper
{
struct BackendResources
{
    // TODO remove *_resources suffix
    FrameGraphResources    framegraph_resources;
    MeshCache              mesh_cache;
    MaterialResources      material_resources;
    CullResources          cull_resources;
    ShadowMapResources     shadow_map_resources;
    MainPassResources      main_pass_resources;
    HistogramPassResources histogram_pass_resources;
    GuiPassResources       gui_pass_resources;
    SwapchainPassResources swapchain_pass_resources;
    FrameSyncResources     frame_sync_resources;
    AudioResources         audio_resources;

    // FIXME wrap this
    VkCommandPool gfxCommandPool;
    CommandBuffer gfxCmdBuffer;

    // NOTE: you need to have as many events as concurrent synchronization barriers.
    // For the first implem we can just have as many events as barrier calls.
    VkEvent event;
};

void create_backend_resources(ReaperRoot& root, VulkanBackend& backend);
void destroy_backend_resources(VulkanBackend& backend);
} // namespace Reaper
