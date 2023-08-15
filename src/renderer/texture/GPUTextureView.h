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
    u32         mip_offset;
    u32         mip_count;
    u32         layer_offset;
    u32         layer_count;
};

REAPER_RENDERER_API
GPUTextureView default_texture_view(const GPUTextureProperties& properties);
} // namespace Reaper
