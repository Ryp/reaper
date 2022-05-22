////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "LightingPass.h"

#include "renderer/PrepareBuckets.h"

#include "renderer/vulkan/Backend.h"

#include "common/ReaperRoot.h"

#include "renderer/shader/share/lighting.hlsl"

namespace Reaper
{
constexpr u32 PointLightMax = 128;

LightingPassResources create_lighting_pass_resources(ReaperRoot& root, VulkanBackend& backend)
{
    LightingPassResources resources = {};

    resources.pointLightBuffer = create_buffer(
        root, backend.device, "Point light buffer",
        DefaultGPUBufferProperties(PointLightMax, sizeof(PointLightProperties), GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    return resources;
}

void destroy_lighting_pass_resources(VulkanBackend& backend, LightingPassResources& resources)
{
    vmaDestroyBuffer(backend.vma_instance, resources.pointLightBuffer.handle, resources.pointLightBuffer.allocation);
}

void upload_lighting_pass_frame_resources(VulkanBackend& backend, const PreparedData& prepared,
                                          LightingPassResources& resources)
{
    if (prepared.point_lights.empty())
        return;

    upload_buffer_data(backend.device, backend.vma_instance, resources.pointLightBuffer, prepared.point_lights.data(),
                       prepared.point_lights.size() * sizeof(PointLightProperties));
}
} // namespace Reaper
