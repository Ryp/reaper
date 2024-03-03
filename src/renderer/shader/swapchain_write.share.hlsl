////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SWAPCHAIN_WRITE_SHARE_INCLUDED
#define SWAPCHAIN_WRITE_SHARE_INCLUDED

#include "shared_types.hlsl"

static const hlsl_uint COLOR_SPACE_SRGB = 0;
static const hlsl_uint COLOR_SPACE_REC709 = 1;
static const hlsl_uint COLOR_SPACE_DISPLAY_P3 = 2;
static const hlsl_uint COLOR_SPACE_REC2020 = 3;

static const hlsl_uint TRANSFER_FUNC_LINEAR = 0;
static const hlsl_uint TRANSFER_FUNC_SRGB = 1;
static const hlsl_uint TRANSFER_FUNC_REC709 = 2;
static const hlsl_uint TRANSFER_FUNC_PQ = 3;
static const hlsl_uint TRANSFER_FUNC_WINDOWS_SCRGB = 4;

static const hlsl_uint DYNAMIC_RANGE_SDR = 0;
static const hlsl_uint DYNAMIC_RANGE_HDR = 1;

static const hlsl_uint TONEMAP_FUNC_NONE = 0;
static const hlsl_uint TONEMAP_FUNC_UNCHARTED2 = 1;
static const hlsl_uint TONEMAP_FUNC_ACES_APPROX = 2;

#endif
