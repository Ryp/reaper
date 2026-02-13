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

const uint AttributesSizeBytes = 12 * 4;
const uint AttributesNormalOffsetBytes = 0;
const uint AttributesUVOffsetBytes = 4 * 4;
const uint AttributesTangentOffsetBytes = 8 * 4;

float3 pull_normal(ByteAddressBuffer buffer_attributes, uint vertex_id)
{
    return asfloat(buffer_attributes.Load3(vertex_id * AttributesSizeBytes + AttributesNormalOffsetBytes));
}

float4 pull_tangent(ByteAddressBuffer buffer_attributes, uint vertex_id)
{
    return asfloat(buffer_attributes.Load4(vertex_id * AttributesSizeBytes + AttributesTangentOffsetBytes));
}

float2 pull_uv(ByteAddressBuffer buffer_attributes, uint vertex_id)
{
    return asfloat(buffer_attributes.Load2(vertex_id * AttributesSizeBytes + AttributesUVOffsetBytes));
}

#endif
