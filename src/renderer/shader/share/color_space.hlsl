////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_COLOR_SPACE_INCLUDED
#define SHARE_COLOR_SPACE_INCLUDED

#include "types.hlsl"

static const hlsl_uint COLOR_SPACE_SRGB = 0;
static const hlsl_uint COLOR_SPACE_REC709 = 1;
static const hlsl_uint COLOR_SPACE_DISPLAY_P3 = 2;
static const hlsl_uint COLOR_SPACE_REC2020 = 3;

static const hlsl_uint TRANSFER_FUNC_LINEAR = 0;
static const hlsl_uint TRANSFER_FUNC_SRGB = 1;
static const hlsl_uint TRANSFER_FUNC_REC709 = 2;
static const hlsl_uint TRANSFER_FUNC_PQ = 3;

#endif
