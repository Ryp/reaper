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
enum class GPUTextureType
{
    Tex1D,
    Tex2D,
    Tex3D,
};

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

namespace GPUTextureMisc
{
    enum Type : u32
    {
        None = 0,
        LinearTiling = bit(0),
        SampleLocationCompatible = bit(1),
    };
}

struct GPUTextureProperties
{
    GPUTextureType type;
    u32            width;
    u32            height;
    u32            depth;
    PixelFormat    format;
    u32            mip_count;
    u32            layer_count;
    u32            sample_count;
    u32            usage_flags;
    u32            misc_flags;
};

REAPER_RENDERER_API
GPUTextureProperties default_texture_properties(u32 width, u32 height, PixelFormat format, u32 usage_flags);
} // namespace Reaper
