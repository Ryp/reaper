////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GPUTextureProperties.h"

namespace Reaper
{
namespace ViewAspect
{
    enum type : u32
    {
        Color = bit(0),
        Depth = bit(1),
        Stencil = bit(2),
    };
}

struct GPUTextureView
{
    PixelFormat format;
    u32         aspect;
    u32         mipOffset;
    u32         mipCount;
    u32         layerOffset;
    u32         layerCount;
};

REAPER_RENDERER_API
GPUTextureView DefaultGPUTextureView(const GPUTextureProperties& properties);
} // namespace Reaper
