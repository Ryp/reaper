////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef VIS_BUFFER_FILL_GBUFFER_SHARE_INCLUDED
#define VIS_BUFFER_FILL_GBUFFER_SHARE_INCLUDED

#include "shared_types.hlsl"

#define Slot_VisBuffer              0
#define Slot_VisBufferDepthMS       1
#define Slot_ResolvedDepth          2
#define Slot_GBuffer0               3
#define Slot_GBuffer1               4
#define Slot_instance_params        5
#define Slot_visible_index_buffer   6
#define Slot_buffer_position_ms     7
#define Slot_buffer_attributes      8
#define Slot_visible_meshlets       9
#define Slot_diffuse_map_sampler    10
#define Slot_material_maps          11

static const hlsl_uint GBufferFillThreadCountX = 16;
static const hlsl_uint GBufferFillThreadCountY = 16;

struct FillGBufferPushConstants
{
    hlsl_uint2 extent_ts;
    hlsl_float2 extent_ts_inv;
};

#endif
