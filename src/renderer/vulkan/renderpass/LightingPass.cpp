////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "LightingPass.h"

#include "renderer/PrepareBuckets.h"
#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Buffer.h"

#include "profiling/Scope.h"

namespace Reaper
{
LightingPassResources create_lighting_pass_resources(VulkanBackend& backend)
{
    static_cast<void>(backend);

    LightingPassResources resources = {};

    return resources;
}

void destroy_lighting_pass_resources(VulkanBackend& backend, LightingPassResources& resources)
{
    static_cast<void>(backend);
    static_cast<void>(resources);
}

void upload_lighting_pass_frame_resources(StorageBufferAllocator& frame_storage_allocator, const PreparedData& prepared,
                                          LightingPassResources& resources)
{
    REAPER_PROFILE_SCOPE_FUNC();

    if (prepared.point_lights.empty())
        return;

    resources.point_light_buffer_alloc =
        allocate_storage(frame_storage_allocator, prepared.point_lights.size() * sizeof(PointLightProperties));

    upload_storage_buffer(frame_storage_allocator, resources.point_light_buffer_alloc, prepared.point_lights.data());
}
} // namespace Reaper
