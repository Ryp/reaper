////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_MATERIAL_INCLUDED
#define REAPER_MATERIAL_INCLUDED

#include "resource/Resource.h"

struct Material {
    TextureId albedo;
    TextureId fresnel;
    TextureId roughness;
    TextureId normal;
};

#endif // REAPER_MATERIAL_INCLUDED
