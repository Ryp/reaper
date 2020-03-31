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
//
// NOTE:
// GLM matrices are COLUMN-major by default. It does not look like this can be changed.
// HLSL matrices are ROW-major by default. This can be changed globally or locally.
// Our interop layer forces ROW-major all the time, independently of the HLSL setting.
// We could support column major but it is not useful at the time.
//
// Be careful when manipulating non-square matrices between GLM and the interop layer,
// since a 4x3 matrix in GLM will be a 3x4 matrix for the interop layer.
#define REAPER_ROW_MAJOR 0
#define REAPER_COL_MAJOR 1
#define REAPER_HLSL_INTEROP_MATRIX_ORDER REAPER_ROW_MAJOR

#ifdef REAPER_SHADER_CODE
    #define hlsl_int    int
    #define hlsl_int2   int2
    #define hlsl_int3   int3
    #define hlsl_int4   int4

    #define hlsl_uint   uint
    #define hlsl_uint2  uint2
    #define hlsl_uint3  uint3
    #define hlsl_uint4  uint4

    #define hlsl_float  float
    #define hlsl_float2 float2
    #define hlsl_float3 float3
    #define hlsl_float4 float4

    #if REAPER_HLSL_INTEROP_MATRIX_ORDER == REAPER_ROW_MAJOR
    #define hlsl_int3x3 row_major int3x3
    #define hlsl_int3x4 row_major int3x4
    #define hlsl_int4x4 row_major int4x4

    #define hlsl_uint3x3 row_major uint3x3
    #define hlsl_uint3x4 row_major uint3x4
    #define hlsl_uint4x4 row_major uint4x4

    #define hlsl_float3x3 row_major float3x3
    #define hlsl_float3x4 row_major float3x4
    #define hlsl_float4x4 row_major float4x4
    #elif REAPER_HLSL_INTEROP_MATRIX_ORDER == REAPER_COL_MAJOR
    #define hlsl_int3x3 column_major int3x3
    #define hlsl_int3x4 column_major int3x4
    #define hlsl_int4x4 column_major int4x4

    #define hlsl_uint3x3 column_major uint3x3
    #define hlsl_uint3x4 column_major uint3x4
    #define hlsl_uint4x4 column_major uint4x4

    #define hlsl_float3x3 column_major float3x3
    #define hlsl_float3x4 column_major float3x4
    #define hlsl_float4x4 column_major float4x4
    #endif
#else
    #include "renderer/hlsl/Types.inl"

    using hlsl_int = i32;
    using hlsl_int2 = hlsl_vector2<i32>;
    using hlsl_int3 = hlsl_vector3<i32>;
    using hlsl_int4 = hlsl_vector4<i32>;

    using hlsl_uint = u32;
    using hlsl_uint2 = hlsl_vector2<u32>;
    using hlsl_uint3 = hlsl_vector3<u32>;
    using hlsl_uint4 = hlsl_vector4<u32>;

    using hlsl_float = f32;
    using hlsl_float2 = hlsl_vector2<f32>;
    using hlsl_float3 = hlsl_vector3<f32>;
    using hlsl_float4 = hlsl_vector4<f32>;

    #if REAPER_HLSL_INTEROP_MATRIX_ORDER == REAPER_ROW_MAJOR
    using hlsl_float3x3 = hlsl_matrix3x3_row_major<f32>;
    using hlsl_float3x4 = hlsl_matrix3x4_row_major<f32>;
    using hlsl_float4x4 = hlsl_matrix4x4_row_major<f32>;

    using hlsl_uint3x3 = hlsl_matrix3x3_row_major<u32>;
    using hlsl_uint3x4 = hlsl_matrix3x4_row_major<u32>;
    using hlsl_uint4x4 = hlsl_matrix4x4_row_major<u32>;

    using hlsl_int3x3 = hlsl_matrix3x3_row_major<i32>;
    using hlsl_int3x4 = hlsl_matrix3x4_row_major<i32>;
    using hlsl_int4x4 = hlsl_matrix4x4_row_major<i32>;
    #elif REAPER_HLSL_INTEROP_MATRIX_ORDER == REAPER_COL_MAJOR
    using hlsl_float3x3 = hlsl_matrix3x3_col_major<f32>;
    using hlsl_float3x4 = hlsl_matrix3x4_col_major<f32>;
    using hlsl_float4x4 = hlsl_matrix4x4_col_major<f32>;

    using hlsl_uint3x3 = hlsl_matrix3x3_col_major<u32>;
    using hlsl_uint3x4 = hlsl_matrix3x4_col_major<u32>;
    using hlsl_uint4x4 = hlsl_matrix4x4_col_major<u32>;

    using hlsl_int3x3 = hlsl_matrix3x3_col_major<i32>;
    using hlsl_int3x4 = hlsl_matrix3x4_col_major<i32>;
    using hlsl_int4x4 = hlsl_matrix4x4_col_major<i32>;
    #endif
#endif

#endif
