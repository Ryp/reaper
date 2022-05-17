////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_AABB_INCLUDED
#define LIB_AABB_INCLUDED

struct AABB
{
    float3 center;
    float3 half_extent;
};

AABB build_aabb(float3 position_min, float3 position_max)
{
    const float3 aabb_center = (position_min + position_max) / 2.0;
    const float3 aabb_half_extent = position_max - aabb_center;

    AABB aabb;
    aabb.center = aabb_center;
    aabb.half_extent = aabb_half_extent;
    return aabb;
}

// Return true if they overlap
bool aabb_vs_aabb_test(AABB lhs, AABB rhs)
{
    const float3 position_delta = abs(lhs.center - rhs.center);
    const float3 half_extent_sum = lhs.half_extent + rhs.half_extent;

    return all(position_delta <= half_extent_sum);
}

#endif
