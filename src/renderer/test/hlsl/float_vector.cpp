////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "renderer/hlsl/Types.h"

TEST_CASE("HLSL float vector types")
{
    SUBCASE("hlsl_float")
    {
        static_assert(sizeof(hlsl_float) == 1 * sizeof(float));
        static_assert(alignof(hlsl_float) == 1 * sizeof(float));

        hlsl_float zeroed = {};

        CHECK_EQ(zeroed, 0.f);
    }

    SUBCASE("hlsl_float2")
    {
        static_assert(sizeof(hlsl_float2) == 2 * sizeof(float));
        static_assert(alignof(hlsl_float2) == 2 * sizeof(float));

        hlsl_float2 zeroed = {};

        CHECK_EQ(zeroed.x, 0.f);
        CHECK_EQ(zeroed.y, 0.f);

        hlsl_float2 initialized = glm::fvec2(1.f, 2.f);

        CHECK_EQ(initialized.x, 1.f);
        CHECK_EQ(initialized.y, 2.f);
    }

    SUBCASE("hlsl_float3")
    {
        static_assert(sizeof(hlsl_float3) == 3 * sizeof(float));
        // static_assert(alignof(hlsl_float3) == 4 * sizeof(float));

        hlsl_float3 zeroed = {};

        CHECK_EQ(zeroed.x, 0.f);
        CHECK_EQ(zeroed.y, 0.f);
        CHECK_EQ(zeroed.z, 0.f);

        hlsl_float3 initialized = glm::fvec3(1.f, 2.f, 3.f);

        CHECK_EQ(initialized.x, 1.f);
        CHECK_EQ(initialized.y, 2.f);
        CHECK_EQ(initialized.z, 3.f);
    }

    SUBCASE("hlsl_float4")
    {
        static_assert(sizeof(hlsl_float4) == 4 * sizeof(float));
        static_assert(alignof(hlsl_float4) == 4 * sizeof(float));

        hlsl_float4 zeroed = {};

        CHECK_EQ(zeroed.x, 0.f);
        CHECK_EQ(zeroed.y, 0.f);
        CHECK_EQ(zeroed.z, 0.f);
        CHECK_EQ(zeroed.w, 0.f);

        hlsl_float4 initialized = glm::fvec4(1.f, 2.f, 3.f, 4.f);

        CHECK_EQ(initialized.x, 1.f);
        CHECK_EQ(initialized.y, 2.f);
        CHECK_EQ(initialized.z, 3.f);
        CHECK_EQ(initialized.w, 4.f);
    }
}
