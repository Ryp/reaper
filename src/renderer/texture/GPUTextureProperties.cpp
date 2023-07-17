////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GPUTextureProperties.h"

namespace Reaper
{
GPUTextureProperties DefaultGPUTextureProperties(u32 width, u32 height, PixelFormat format, u32 usage_flags)
{
    return {
        width,       height,
        1, // depth
        format,
        1,           // mipCount
        1,           // layerCount
        1,           // sampleCount
        usage_flags, // usage_flags
        0,           // misc_flags
    };
}
} // namespace Reaper
