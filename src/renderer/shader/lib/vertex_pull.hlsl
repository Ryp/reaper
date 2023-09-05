////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_VERTEX_PULL_INCLUDED
#define LIB_VERTEX_PULL_INCLUDED

// https://github.com/KhronosGroup/glslang/issues/3297
float3 pull_position(ByteAddressBuffer buffer_position, uint vertex_id)
{
    return asfloat(buffer_position.Load3(vertex_id * 3 * 4));
}

float3 pull_normal(ByteAddressBuffer buffer_normal, uint vertex_id)
{
    return asfloat(buffer_normal.Load3(vertex_id * 3 * 4));
}

float4 pull_tangent(ByteAddressBuffer buffer_tangent, uint vertex_id)
{
    return asfloat(buffer_tangent.Load4(vertex_id * 4 * 4));
}

float2 pull_uv(ByteAddressBuffer buffer_uv, uint vertex_id)
{
    return asfloat(buffer_uv.Load2(vertex_id * 2 * 4));
}

#endif
