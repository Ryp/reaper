////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// This header provides interops types to share code between HLSL and C++.
//
// They allow us to easily send data from CPU to GPU with the right memory layout.
// Care still has to be applied when aligning structures, and manual padding is
// Still needed.
//
// Matrices are column major by default since that's what GLM uses.
//
// FIXME
// Since GLM uses a column major layout, all HLSL code flips the multiplication
// order to cancel out the difference in layout. This should be properly fixed.

#pragma once

#ifdef REAPER_SHADER_CODE
#    error
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>

using hlsl_int = i32;
using hlsl_int2 = glm::ivec2;
using hlsl_int3 = glm::ivec3;
using hlsl_int4 = glm::ivec4;

using hlsl_uint = u32;
using hlsl_uint2 = glm::uvec2;
using hlsl_uint3 = glm::uvec3;
using hlsl_uint4 = glm::uvec4;

using hlsl_float = f32;
using hlsl_float2 = glm::fvec2;
// using hlsl_float3 = glm::fvec3;
// using hlsl_float4 = glm::fvec4;

// using hlsl_float3x3 = glm::fmat3;
// using hlsl_float4x4 = glm::fmat4;

struct hlsl_float3
{
    float x;
    float y;
    float z;

    hlsl_float3() = default;
    hlsl_float3(const glm::fvec3& other)
    {
        this->x = other.x;
        this->y = other.y;
        this->z = other.z;
    }

    operator glm::vec3() const { return glm::vec3(x, y, z); }
};

struct alignas(16) hlsl_float4
{
    float x;
    float y;
    float z;
    float w;

    hlsl_float4() = default;
    hlsl_float4(const glm::fvec4& other)
    {
        this->x = other.x;
        this->y = other.y;
        this->z = other.z;
        this->w = other.w;
    }

    operator glm::vec4() const { return glm::vec4(x, y, z, w); }
};

struct alignas(16) hlsl_float3x3
{
    hlsl_float3 col0;
    hlsl_float  _pad0;
    hlsl_float3 col1;
    hlsl_float  _pad1;
    hlsl_float3 col2;
    hlsl_float  _pad2;

    hlsl_float3x3() = default;
    hlsl_float3x3(const glm::fmat3& other)
    {
        this->col0 = glm::column(other, 0);
        this->col1 = glm::column(other, 1);
        this->col2 = glm::column(other, 2);
        this->_pad0 = 0.f;
        this->_pad1 = 0.f;
        this->_pad2 = 0.f;
    }

    operator glm::mat3() const { return glm::mat3(col0, col1, col2); }
};

struct alignas(16) hlsl_float4x4
{
    hlsl_float4 col0;
    hlsl_float4 col1;
    hlsl_float4 col2;
    hlsl_float4 col3;

    hlsl_float4x4() = default;
    hlsl_float4x4(const glm::fmat4& other)
    {
        this->col0 = glm::column(other, 0);
        this->col1 = glm::column(other, 1);
        this->col2 = glm::column(other, 2);
        this->col3 = glm::column(other, 3);
    }

    operator glm::fmat4() const { return glm::fmat4(col0, col1, col2, col3); }
};
