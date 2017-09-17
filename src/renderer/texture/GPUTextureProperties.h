////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/RendererExport.h"
#include "renderer/format/PixelFormat.h"

namespace Reaper
{
    struct GPUTextureProperties
    {
        u32         width;
        u32         height;
        u32         depth;
        PixelFormat format;
        u32         mipCount;
        u32         layerCount;
        u32         sampleCount;
        bool        isCubemap;
    };

    REAPER_RENDERER_API
    GPUTextureProperties DefaultGPUTextureProperties(u32 width, u32 height, PixelFormat format);

    struct GPUTextureUsage
    {
        bool hey;
    };
}
