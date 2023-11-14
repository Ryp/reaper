////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/vulkan/Buffer.h"
#include "renderer/vulkan/StorageBufferAllocator.h"

namespace Reaper
{
struct LightingPassResources
{
    StorageBufferAlloc point_light_buffer_alloc;
};

struct VulkanBackend;

LightingPassResources create_lighting_pass_resources(VulkanBackend& backend);
void                  destroy_lighting_pass_resources(VulkanBackend& backend, LightingPassResources& resources);

struct PreparedData;

void upload_lighting_pass_frame_resources(StorageBufferAllocator& frame_storage_allocator, const PreparedData& prepared,
                                          LightingPassResources& resources);
} // namespace Reaper
