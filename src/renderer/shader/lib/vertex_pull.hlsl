////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_VERTEX_PULL_INCLUDED
#define LIB_VERTEX_PULL_INCLUDED

float3 pull_position_ms(ByteAddressBuffer buffer_position_ms, uint vertex_id)
{
    return asfloat(buffer_position_ms.Load3(vertex_id * 3 * 4));
}

float3 pull_normal_ms(ByteAddressBuffer buffer_normal_ms, uint vertex_id)
{
    return asfloat(buffer_normal_ms.Load3(vertex_id * 3 * 4));
}

float2 pull_uv(ByteAddressBuffer buffer_uv, uint vertex_id)
{
    return asfloat(buffer_uv.Load2(vertex_id * 2 * 4));
}

#endif
