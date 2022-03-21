////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Resource.h"

struct Material
{
    TextureId albedo;
    TextureId fresnel;
    TextureId roughness;
    TextureId normal;
};
