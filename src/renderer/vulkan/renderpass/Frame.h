////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
struct FrameData2
{
    u32   index;
    float timeMs;
};

struct FrameData
{
    VkExtent2D backbufferExtent;
};

} // namespace Reaper
