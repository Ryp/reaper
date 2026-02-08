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

struct SwapchainWriteParams
{
    hlsl_float exposure_compensation;
    hlsl_float tonemap_min_nits;
    hlsl_float tonemap_max_nits;
    hlsl_float sdr_ui_max_brightness_nits;
    hlsl_float sdr_peak_brightness_nits;
};

#endif
