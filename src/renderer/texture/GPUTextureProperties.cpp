////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GPUTextureProperties.h"

namespace Reaper
{
GPUTextureProperties default_texture_properties(u32 width, u32 height, PixelFormat format, u32 usage_flags)
{
    return GPUTextureProperties{
        .width = width,
        .height = height,
        .depth = 1,
        .format = format,
        .mip_count = 1,
        .layer_count = 1,
        .sample_count = 1,
        .usage_flags = usage_flags,
        .misc_flags = 0,
    };
}
} // namespace Reaper
