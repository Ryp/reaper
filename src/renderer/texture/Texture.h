////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_TEXTURE_INCLUDED
#define REAPER_TEXTURE_INCLUDED

#include <string>

#include "renderer/format/PixelFormat.h"

using TextureId = std::string;

struct Texture
{
    u32 width;
    u32 height;
    u32 mipLevels;
    u32 layerCount;
    PixelFormat format;
    void* data;
};

#endif // REAPER_TEXTURE_INCLUDED
