////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_PIXELFORMAT_INCLUDED
#define REAPER_PIXELFORMAT_INCLUDED

enum class PixelFormat
{
    Unknown = 0,
    R16G16B16A16_UNORM,
    R16G16B16A16_SFLOAT,
    R8G8B8A8_UNORM,
    R8G8B8A8_SRGB,
    B8G8R8A8_UNORM,
    BC2_UNORM_BLOCK
};

#endif // REAPER_PIXELFORMAT_INCLUDED
