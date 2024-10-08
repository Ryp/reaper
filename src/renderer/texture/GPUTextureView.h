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

struct GPUTextureSubresource
{
    u32 aspect;
    u32 mip_offset;
    u32 mip_count;
    u32 layer_offset;
    u32 layer_count;
};

REAPER_RENDERER_API
GPUTextureSubresource default_texture_subresource(const GPUTextureProperties& properties);

inline GPUTextureSubresource default_texture_subresource_one_color_mip(u32 mip_index = 0, u32 layer_index = 0)
{
    return GPUTextureSubresource{
        .aspect = ViewAspect::Color,
        .mip_offset = mip_index,
        .mip_count = 1,
        .layer_offset = layer_index,
        .layer_count = 1,
    };
}

enum class GPUTextureViewType
{
    Tex1D,
    Tex2D,
    Tex3D,
    TexCube,
    Tex1DArray,
    Tex2DArray,
    TexCubeArray,
};

struct GPUTextureView
{
    GPUTextureViewType    type;
    PixelFormat           format;
    GPUTextureSubresource subresource;
};

REAPER_RENDERER_API
GPUTextureView default_texture_view(const GPUTextureProperties& properties);
} // namespace Reaper
