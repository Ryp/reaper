////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GPUTextureProperties.h"

namespace Reaper
{
struct GPUTextureView
{
    PixelFormat format;
    u32         mipOffset;
    u32         mipCount;
    u32         layerOffset;
    u32         layerCount;
};

REAPER_RENDERER_API
GPUTextureView DefaultGPUTextureView(const GPUTextureProperties& properties);
} // namespace Reaper
