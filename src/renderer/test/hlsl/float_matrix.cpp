////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "renderer/hlsl/Types.h"

TEST_CASE("HLSL float matrix types")
{
    SUBCASE("hlsl_float3x3")
    {
        static_assert(alignof(hlsl_float3x3) == 4 * sizeof(float));
        static_assert(sizeof(hlsl_float3x3) == 12 * sizeof(float));

        hlsl_float3x3 identity = glm::fmat3();

        CHECK_EQ(identity.row0.x, 1.f);
        CHECK_EQ(identity.row1.y, 1.f);
        CHECK_EQ(identity.row2.z, 1.f);

        CHECK_EQ(identity.row0.y, 0.f);
        CHECK_EQ(identity.row0.z, 0.f);

        CHECK_EQ(identity.row2.x, 0.f);
        CHECK_EQ(identity.row2.y, 0.f);
    }

    SUBCASE("hlsl_float4x4")
    {
        static_assert(alignof(hlsl_float4x4) == 4 * sizeof(float));
        static_assert(sizeof(hlsl_float4x4) == 16 * sizeof(float));

        hlsl_float4x4 identity = glm::fmat4();

        CHECK_EQ(identity.row0.x, 1.f);
        CHECK_EQ(identity.row1.y, 1.f);
        CHECK_EQ(identity.row2.z, 1.f);
        CHECK_EQ(identity.row3.w, 1.f);

        CHECK_EQ(identity.row0.y, 0.f);
        CHECK_EQ(identity.row0.z, 0.f);

        CHECK_EQ(identity.row2.x, 0.f);
        CHECK_EQ(identity.row2.y, 0.f);

        CHECK_EQ(identity.row3.z, 0.f);
    }
}
