////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef FORWARD_SHARE_INCLUDED
#define FORWARD_SHARE_INCLUDED

#define Slot_fw_pass_params         0
#define Slot_fw_instance_params     1
#define Slot_fw_visible_meshlets    2
#define Slot_fw_buffer_position_ms  3
#define Slot_fw_buffer_normal_ms    4
#define Slot_fw_buffer_tangent_ms   5
#define Slot_fw_buffer_uv           6
#define Slot_fw_point_lights        7
#define Slot_fw_shadow_map_sampler  8
#define Slot_fw_shadow_maps         9

#define Slot_fw_diffuse_map_sampler 0
#define Slot_fw_material_maps       1

#include "shared_types.hlsl"

struct ForwardPassParams
{
    hlsl_float3x4 ws_to_vs_matrix;
    hlsl_float4x4 ws_to_cs_matrix;
    hlsl_uint3    _pad;
    hlsl_uint     point_light_count;
};

#endif
