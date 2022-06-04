////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/RendererExport.h"
#include "renderer/format/PixelFormat.h"

#include "core/BitTricks.h"
#include <core/Types.h>

namespace Reaper
{
namespace GPUTextureUsage
{
    enum Type : u32
    {
        None = 0,
        TransferSrc = bit(0),
        TransferDst = bit(1),
        Sampled = bit(2),
        Storage = bit(3),
        ColorAttachment = bit(4),
        DepthStencilAttachment = bit(5),
        TransientAttachment = bit(6),
        InputAttachment = bit(7),
    };
}

namespace GPUMiscFlags
{
    enum
    {
        None = 0,
        Cubemap = bit(0),
    };
}

struct GPUTextureProperties
{
    u32         width;
    u32         height;
    u32         depth;
    PixelFormat format;
    u32         mipCount;
    u32         layerCount;
    u32         sampleCount;
    u32         usage_flags;
    u32         miscFlags;
};

REAPER_RENDERER_API
GPUTextureProperties DefaultGPUTextureProperties(u32 width, u32 height, PixelFormat format);
} // namespace Reaper
