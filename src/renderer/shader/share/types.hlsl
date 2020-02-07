////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_TYPES_INCLUDED
#define SHARE_TYPES_INCLUDED

#ifdef REAPER_SHADER_CODE
    #define sl_uint     uint
    #define sl_uint2    uint2
    #define sl_uint3    uint3
    #define sl_uint4    uint4

    #define sl_float    float
    #define sl_float2   float2
    #define sl_float3   float3
    #define sl_float4   float4

    #define sl_float4x4 float4x4
    #define sl_float3x3 float3x3
#else
    using sl_uint       = u32;
    using sl_uint2      = glm::uvec2;
    using sl_uint3      = glm::uvec3;
    using sl_uint4      = glm::uvec4;

    using sl_float      = float;
    using sl_float2     = glm::fvec2;
    using sl_float3     = glm::fvec3;
    using sl_float4     = glm::fvec4;

    using sl_float4x4   = glm::fmat4;
    using sl_float3x3   = glm::fmat3x4; // FIXME padding
#endif

#endif
