////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Frustum
{
    glm::vec4 plane[6];
};

struct AABB
{
    glm::vec3 min;
    glm::vec3 max;
};

namespace
{
inline void transform_point(glm::vec3* p, const glm::mat3x4* mat)
{
    glm::vec3 op = *p;
    *p = (*mat) * op;
}

inline float dot(const glm::vec3* v, const glm::vec4* p)
{
    return v->x * p->x + v->y * p->y + v->z * p->z + p->w;
}

bool is_visible(glm::mat3x4* transform, AABB* aabb, Frustum* frustum)
{
    // get aabb points
    glm::vec3 points[] = {{aabb->min.x, aabb->min.y, aabb->min.z}, {aabb->max.x, aabb->min.y, aabb->min.z},
                          {aabb->max.x, aabb->max.y, aabb->min.z}, {aabb->min.x, aabb->max.y, aabb->min.z},

                          {aabb->min.x, aabb->min.y, aabb->max.z}, {aabb->max.x, aabb->min.y, aabb->max.z},
                          {aabb->max.x, aabb->max.y, aabb->max.z}, {aabb->min.x, aabb->max.y, aabb->max.z}};

    // transform points to world space
    for (int i = 0; i < 8; ++i)
    {
        transform_point(points + i, transform);
    }

    // for each plane…
    for (int i = 0; i < 6; ++i)
    {
        bool inside = false;

        for (int j = 0; j < 8; ++j)
        {
            if (dot(points + j, frustum->plane + i) > 0)
            {
                inside = true;
                break;
            }
        }

        if (!inside)
        {
            return false;
        }
    }

    return true;
}
} // namespace

TEST_CASE("Frustum culling")
{
    float     angle = 30.0f;
    glm::vec3 translation(125.0f, 5.0f, 10.0f);
    glm::vec3 rotationDirection(3.0f, 1.0f, 0.0f);
    glm::mat4 transform = glm::translate(glm::rotate(glm::mat4(1.0f), angle, rotationDirection), translation);

    AABB box = {{-1, -2, -3}, {1, 2, 3}};

    // TODO compute frustum plane equations
    Frustum frustum = {{{1, 0, 0, 10}, {-1, 0, 0, 10}, {0, 1, 0, 10}, {0, -1, 0, 10}, {0, 0, 1, 10}, {0, 0, -1, 10}}};

    glm::mat3x4 hmatrix = {{0.123f, 0.456f, 0.789f, 1}, {0.456f, 0.123f, 0.789f, -1}, {0.789f, 0.123f, 0.456f, 1}};

    // glm::mat3x4 hmatrix = transform;

    SUBCASE("visible") { CHECK(is_visible(&hmatrix, &box, &frustum)); }
}
