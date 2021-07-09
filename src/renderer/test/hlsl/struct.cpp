////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "renderer/hlsl/Types.h"

constexpr u32 vector4_size = 16;

TEST_CASE("HLSL structs")
{
    SUBCASE("simple")
    {
        struct test1
        {
            hlsl_float a;
            hlsl_float b;
            hlsl_float c;
            hlsl_float d;
        };

        static_assert(sizeof(test1) == vector4_size);

        struct test2
        {
            hlsl_int2   a;
            hlsl_float2 b;
        };

        static_assert(sizeof(test2) == vector4_size);

        struct test3
        {
            hlsl_uint4  a;
            hlsl_float4 b;
        };

        static_assert(sizeof(test3) == 2 * vector4_size);
    }

    SUBCASE("advanced")
    {
        struct test1
        {
            hlsl_float  b;
            hlsl_float  c;
            hlsl_float2 d;
        };

        static_assert(sizeof(test1) == vector4_size);

        struct test2
        {
            hlsl_float4x4 b;
            hlsl_float    c;
        };

        static_assert(sizeof(test2) == 5 * vector4_size);

        struct test3
        {
            hlsl_float2   c;
            hlsl_float    d;
            hlsl_float    f;
            hlsl_float3x3 b;
            hlsl_float3   g;
            hlsl_float    h;
            hlsl_float4x4 a;
        };

        static_assert(sizeof(test3) == 9 * vector4_size);
    }
}
