////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vulkan_loader/Vulkan.h>

namespace tracy
{
class VkCtx;
}

namespace Reaper
{
struct CommandBuffer
{
    VkCommandBuffer handle;

#if defined(REAPER_USE_TRACY)
    tracy::VkCtx* tracy_ctx;
#endif
};
} // namespace Reaper
