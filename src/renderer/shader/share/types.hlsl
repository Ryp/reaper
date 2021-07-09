////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
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
// GLM matrices have COLUMN-major ordering and storage. It does not look like
// this can be changed.
// HLSL matrices have ROW-major ordering (float2x3 = 2 rows 3 cols) but have
// COLUMN-major storage by default. Storage can be changed globally or locally.
//
// Our interop layer forces it's own storage layout all the time,
// independently of the global setting.
// For now we use COLUMN-major as the default storage because there's a bug in
// glslang that prevents us from
// using the 'row_major" qualifier.
// https://github.com/KhronosGroup/glslang/issues/1734
//
// NOTE:
// Be careful when manipulating non-square matrices between GLM and the interop layer,
// since a 4x3 matrix in GLM will be equivalent to a 3x4 matrix for HLSL.
//
// NOTE:
// When inspecting SPIR-V code, don't be surprised if the codegen starts showing
// ColMajor when you actually specified row_major (and vice-versa).
// Here's a relevant source:
// https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/SPIR-V.rst#packing
#define REAPER_ROW_MAJOR 0
#define REAPER_COL_MAJOR 1
#define REAPER_HLSL_INTEROP_MATRIX_STORAGE REAPER_COL_MAJOR

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

    #if REAPER_HLSL_INTEROP_MATRIX_STORAGE == REAPER_ROW_MAJOR
    #define hlsl_int3x3 row_major int3x3
    #define hlsl_int3x4 row_major int3x4
    #define hlsl_int4x4 row_major int4x4

    #define hlsl_uint3x3 row_major uint3x3
    #define hlsl_uint3x4 row_major uint3x4
    #define hlsl_uint4x4 row_major uint4x4

    #define hlsl_float3x3 row_major float3x3
    #define hlsl_float3x4 row_major float3x4
    #define hlsl_float4x4 row_major float4x4
    #elif REAPER_HLSL_INTEROP_MATRIX_STORAGE == REAPER_COL_MAJOR
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

    #if REAPER_HLSL_INTEROP_MATRIX_STORAGE == REAPER_ROW_MAJOR
    using hlsl_float3x3 = hlsl_matrix3x3<RowMajor, f32>;
    using hlsl_float3x4 = hlsl_matrix3x4<RowMajor, f32>;
    using hlsl_float4x4 = hlsl_matrix4x4<RowMajor, f32>;

    using hlsl_uint3x3 = hlsl_matrix3x3<RowMajor, u32>;
    using hlsl_uint3x4 = hlsl_matrix3x4<RowMajor, u32>;
    using hlsl_uint4x4 = hlsl_matrix4x4<RowMajor, u32>;

    using hlsl_int3x3 = hlsl_matrix3x3<RowMajor, i32>;
    using hlsl_int3x4 = hlsl_matrix3x4<RowMajor, i32>;
    using hlsl_int4x4 = hlsl_matrix4x4<RowMajor, i32>;
    #elif REAPER_HLSL_INTEROP_MATRIX_STORAGE == REAPER_COL_MAJOR
    using hlsl_float3x3 = hlsl_matrix3x3<ColumnMajor, f32>;
    using hlsl_float3x4 = hlsl_matrix3x4<ColumnMajor, f32>;
    using hlsl_float4x4 = hlsl_matrix4x4<ColumnMajor, f32>;

    using hlsl_uint3x3 = hlsl_matrix3x3<ColumnMajor, u32>;
    using hlsl_uint3x4 = hlsl_matrix3x4<ColumnMajor, u32>;
    using hlsl_uint4x4 = hlsl_matrix4x4<ColumnMajor, u32>;

    using hlsl_int3x3 = hlsl_matrix3x3<ColumnMajor, i32>;
    using hlsl_int3x4 = hlsl_matrix3x4<ColumnMajor, i32>;
    using hlsl_int4x4 = hlsl_matrix4x4<ColumnMajor, i32>;
    #endif
#endif

#endif
