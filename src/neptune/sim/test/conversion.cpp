////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include <doctest/doctest.h>

#include <glm/glm.hpp>

#include "neptune/sim/BulletConversion.inl"

TEST_CASE("Conversion")
{
    using namespace Neptune;

    SUBCASE("vec3")
    {
        glm::fvec3      vec_glm(1.f, 2.f, 4.f);
        const btVector3 vec_bt = toBt(vec_glm);

        CHECK_EQ(vec_bt[0], vec_glm[0]);
        CHECK_EQ(vec_bt[1], vec_glm[1]);
        CHECK_EQ(vec_bt[2], vec_glm[2]);
    }

    SUBCASE("mat3x3")
    {
        glm::fmat3x3 mat_glm = glm::identity<glm::fmat3x3>();
        mat_glm[2][1] = 4.f;
        mat_glm[0][2] = 8.f;

        const btMatrix3x3 mat_bt = toBt(mat_glm);
        CHECK_EQ(mat_bt[1][2], glm::row(mat_glm, 1)[2]);
        CHECK_EQ(mat_bt[2][0], glm::row(mat_glm, 2)[0]);
        CHECK_EQ(mat_bt[0][0], glm::row(mat_glm, 0)[0]);

        const glm::fmat3x3 mat_glm2 = toGlm(mat_bt);
        CHECK_EQ(mat_bt[1][2], glm::row(mat_glm2, 1)[2]);
        CHECK_EQ(mat_bt[2][0], glm::row(mat_glm2, 2)[0]);
        CHECK_EQ(mat_bt[0][0], glm::row(mat_glm2, 0)[0]);
    }

    SUBCASE("mat4x3")
    {
        glm::fmat4x3 mat_glm = glm::identity<glm::fmat4x3>();
        mat_glm[2][1] = 4.f;
        mat_glm[0][2] = 8.f;
        mat_glm[3][1] = 16.f;

        const btTransform mat_bt = toBt(mat_glm);
        CHECK_EQ(mat_bt.getBasis()[1][2], glm::row(mat_glm, 1)[2]);
        CHECK_EQ(mat_bt.getBasis()[2][0], glm::row(mat_glm, 2)[0]);
        CHECK_EQ(mat_bt.getOrigin()[1], glm::column(mat_glm, 3)[1]);

        const glm::fmat4x3 mat_glm2 = toGlm(mat_bt);
        CHECK_EQ(mat_bt.getBasis()[1][2], glm::row(mat_glm2, 1)[2]);
        CHECK_EQ(mat_bt.getBasis()[2][0], glm::row(mat_glm2, 2)[0]);
        CHECK_EQ(mat_bt.getOrigin()[1], glm::column(mat_glm2, 3)[1]);
    }
}
