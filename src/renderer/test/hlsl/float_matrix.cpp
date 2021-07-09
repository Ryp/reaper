////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "renderer/hlsl/Types.h"

TEST_CASE("HLSL float matrix types")
{
    SUBCASE("hlsl_float3x3")
    {
        static_assert(alignof(hlsl_matrix3x3<RowMajor, f32>) == 4 * sizeof(float));
        static_assert(sizeof(hlsl_matrix3x3<RowMajor, f32>) == 12 * sizeof(float));

        static_assert(alignof(hlsl_matrix3x3<ColumnMajor, f32>) == 4 * sizeof(float));
        static_assert(sizeof(hlsl_matrix3x3<ColumnMajor, f32>) == 12 * sizeof(float));

        hlsl_float3x3 identity = glm::fmat3();

        CHECK_EQ(identity.element[0].x, 1.f);
        CHECK_EQ(identity.element[1].y, 1.f);
        CHECK_EQ(identity.element[2].z, 1.f);

        CHECK_EQ(identity.element[0].y, 0.f);
        CHECK_EQ(identity.element[0].z, 0.f);

        CHECK_EQ(identity.element[2].x, 0.f);
        CHECK_EQ(identity.element[2].y, 0.f);
    }

    SUBCASE("hlsl_float3x4")
    {
        static_assert(alignof(hlsl_matrix3x4<RowMajor, f32>) == 4 * sizeof(float));
        static_assert(sizeof(hlsl_matrix3x4<RowMajor, f32>) == 12 * sizeof(float));

        static_assert(alignof(hlsl_matrix3x4<ColumnMajor, f32>) == 4 * sizeof(float));
        static_assert(sizeof(hlsl_matrix3x4<ColumnMajor, f32>) == 16 * sizeof(float));

        hlsl_float3x4 identity = glm::fmat4x3();

        CHECK_EQ(identity.element[0].x, 1.f);
        CHECK_EQ(identity.element[1].y, 1.f);
        CHECK_EQ(identity.element[2].z, 1.f);

        CHECK_EQ(identity.element[0].y, 0.f);
        CHECK_EQ(identity.element[0].z, 0.f);

        CHECK_EQ(identity.element[2].x, 0.f);
        CHECK_EQ(identity.element[2].y, 0.f);

        CHECK_EQ(identity.element[0].z, 0.f);
    }

    SUBCASE("hlsl_float4x4")
    {
        static_assert(alignof(hlsl_matrix4x4<RowMajor, f32>) == 4 * sizeof(float));
        static_assert(sizeof(hlsl_matrix4x4<RowMajor, f32>) == 16 * sizeof(float));

        static_assert(alignof(hlsl_matrix4x4<ColumnMajor, f32>) == 4 * sizeof(float));
        static_assert(sizeof(hlsl_matrix4x4<ColumnMajor, f32>) == 16 * sizeof(float));

        hlsl_float4x4 identity = glm::fmat4();

        CHECK_EQ(identity.element[0].x, 1.f);
        CHECK_EQ(identity.element[1].y, 1.f);
        CHECK_EQ(identity.element[2].z, 1.f);
        CHECK_EQ(identity.element[3].w, 1.f);

        CHECK_EQ(identity.element[0].y, 0.f);
        CHECK_EQ(identity.element[0].z, 0.f);

        CHECK_EQ(identity.element[2].x, 0.f);
        CHECK_EQ(identity.element[2].y, 0.f);

        CHECK_EQ(identity.element[3].z, 0.f);
    }
}
