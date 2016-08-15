////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_TEXTURE_INCLUDED
#define REAPER_TEXTURE_INCLUDED

#include <string>
#include <vector>

#include "renderer/format/PixelFormat.h"

using TextureId = std::string;

struct Texture
{
    u32 width;
    u32 height;
    u32 mipLevels;
    u32 layerCount;
    PixelFormat format;
    void* rawData;
    std::size_t size;
    u32 bytesPerPixel;
};

#endif // REAPER_TEXTURE_INCLUDED
