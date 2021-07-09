////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/format/PixelFormat.h"

struct Texture
{
    u32         width;
    u32         height;
    u32         mipLevels;
    u32         layerCount;
    u32         format;
    void*       rawData;
    std::size_t size;
    u32         bytesPerPixel;
};
