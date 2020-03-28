////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_TYPES_INCLUDED
#define SHARE_TYPES_INCLUDED

// This header provides interops types to share code between HLSL and C++.
//
// They allow us to easily send data from CPU to GPU with the right memory layout.
// Care still has to be applied when aligning structures, and manual padding is
// still needed.
#ifdef REAPER_SHADER_CODE
    #define hlsl_bool   bool

    #define hlsl_int    int
    #define hlsl_int2   int2
    #define hlsl_int3   int3
    #define hlsl_int4   int4
    #define hlsl_int3x3 int3x3
    #define hlsl_int4x4 int4x4

    #define hlsl_uint   uint
    #define hlsl_uint2  uint2
    #define hlsl_uint3  uint3
    #define hlsl_uint4  uint4
    #define hlsl_uint3x3 uint3x3
    #define hlsl_uint4x4 uint4x4

    #define hlsl_float  float
    #define hlsl_float2 float2
    #define hlsl_float3 float3
    #define hlsl_float4 float4
    #define hlsl_float3x3 float3x3
    #define hlsl_float4x4 float4x4
#else
    #include "renderer/hlsl/Types.inl"

    using hlsl_int = i32;
    using hlsl_int2 = hlsl_vector2<i32>;
    using hlsl_int3 = hlsl_vector3<i32>;
    using hlsl_int4 = hlsl_vector4<i32>;
    using hlsl_int3x3 = hlsl_matrix3x3_row_major<i32>;
    using hlsl_int4x4 = hlsl_matrix4x4_row_major<i32>;

    using hlsl_uint = u32;
    using hlsl_uint2 = hlsl_vector2<u32>;
    using hlsl_uint3 = hlsl_vector3<u32>;
    using hlsl_uint4 = hlsl_vector4<u32>;
    using hlsl_uint3x3 = hlsl_matrix3x3_row_major<u32>;
    using hlsl_uint4x4 = hlsl_matrix4x4_row_major<u32>;

    using hlsl_float = f32;
    using hlsl_float2 = hlsl_vector2<f32>;
    using hlsl_float3 = hlsl_vector3<f32>;
    using hlsl_float4 = hlsl_vector4<f32>;
    using hlsl_float3x3 = hlsl_matrix3x3_row_major<f32>;
    using hlsl_float4x4 = hlsl_matrix4x4_row_major<f32>;
#endif

#endif
