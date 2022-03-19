////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GPUTextureView.h"

namespace Reaper
{
GPUTextureView DefaultGPUTextureView(const GPUTextureProperties& properties)
{
    return {
        properties.format,
        0, // mipOffset
        properties.mipCount,
        0, // layerOffset
        properties.layerCount,
    };
}
} // namespace Reaper
