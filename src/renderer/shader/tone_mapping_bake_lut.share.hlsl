////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef TONE_MAPPING_BAKE_LUT_SHARE_INCLUDED
#define TONE_MAPPING_BAKE_LUT_SHARE_INCLUDED

#include "shared_types.hlsl"

static const hlsl_uint ToneMappingBakeLUT_Res = 1024;
static const hlsl_uint ToneMappingBakeLUT_ThreadCount = 256;

struct ToneMappingBakeLUT_Consts
{
    hlsl_float min_nits;
    hlsl_float max_nits;
};

#endif
