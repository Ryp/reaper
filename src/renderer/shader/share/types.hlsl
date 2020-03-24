////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_TYPES_INCLUDED
#define SHARE_TYPES_INCLUDED

// NOTE: Not all HLSL types are supported.
// It should be trivial to add it for missing one if they are ever used.
#ifdef REAPER_SHADER_CODE
    #define hlsl_bool   bool

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

    #define hlsl_float3x3 float3x3
    #define hlsl_float4x4 float4x4
#else
    #include "renderer/hlsl/Types.h"
#endif

#endif
