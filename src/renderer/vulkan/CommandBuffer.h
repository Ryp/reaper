////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

struct MicroProfileThreadLogGpu;

namespace Reaper
{
struct CommandBuffer
{
    VkCommandBuffer           handle;
    MicroProfileThreadLogGpu* mlog;
};
} // namespace Reaper
