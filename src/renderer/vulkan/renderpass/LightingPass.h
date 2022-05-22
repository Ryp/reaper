////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"

namespace Reaper
{
struct LightingPassResources
{
    BufferInfo pointLightBuffer;
};

struct ReaperRoot;
struct VulkanBackend;

LightingPassResources create_lighting_pass_resources(ReaperRoot& root, VulkanBackend& backend);
void                  destroy_lighting_pass_resources(VulkanBackend& backend, LightingPassResources& resources);

struct PreparedData;

void upload_lighting_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                          LightingPassResources& resources);
} // namespace Reaper
