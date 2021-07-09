////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource/Resource.h"

struct Material
{
    TextureId albedo;
    TextureId fresnel;
    TextureId roughness;
    TextureId normal;
};
