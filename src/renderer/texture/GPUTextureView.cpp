////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GPUTextureView.h"

namespace Reaper
{
namespace
{
    u32 get_default_view_aspect(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::D16_UNORM:
        case PixelFormat::D32_SFLOAT:
            return ViewAspect::Depth;
        case PixelFormat::X8_D24_UNORM_PACK32:
        case PixelFormat::D16_UNORM_S8_UINT:
        case PixelFormat::D24_UNORM_S8_UINT:
        case PixelFormat::D32_SFLOAT_S8_UINT:
            return ViewAspect::Depth | ViewAspect::Stencil;
        case PixelFormat::S8_UINT:
            return ViewAspect::Stencil;
        default:
            return ViewAspect::Color;
        }
    }
} // namespace

GPUTextureSubresource default_texture_subresource(const GPUTextureProperties& properties)
{
    return GPUTextureSubresource{
        .aspect = get_default_view_aspect(properties.format),
        .mip_offset = 0,
        .mip_count = properties.mip_count,
        .layer_offset = 0,
        .layer_count = properties.layer_count,
    };
}

GPUTextureView default_texture_view(const GPUTextureProperties& properties)
{
    return GPUTextureView{
        .format = properties.format,
        .subresource = default_texture_subresource(properties),
    };
}
} // namespace Reaper
