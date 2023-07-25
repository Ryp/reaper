////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_BARYCENTRICS_INCLUDED
#define LIB_BARYCENTRICS_INCLUDED

struct BarycentricsWithDerivatives
{
    float3 lambda;
    float3 dd_x;
    float3 dd_y;
};

// James McLaren and Stephen Hill
// http://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/
//
// NOTE: With Vulkan NDC fix
// NOTE: cs_w_inv has the inverse of W for each of the points in clip space.
BarycentricsWithDerivatives compute_barycentrics_with_derivatives(float2 p0_ndc, float2 p1_ndc, float2 p2_ndc, float3 cs_w_inv, float2 pixel_ndc, float2 viewport_extent_inv)
{
    BarycentricsWithDerivatives bary;

    float inv_det = rcp(determinant(float2x2(p2_ndc - p1_ndc, p0_ndc - p1_ndc)));

    bary.dd_x = float3(p1_ndc.y - p2_ndc.y, p2_ndc.y - p0_ndc.y, p0_ndc.y - p1_ndc.y) * inv_det * cs_w_inv;
    bary.dd_y = float3(p2_ndc.x - p1_ndc.x, p0_ndc.x - p2_ndc.x, p1_ndc.x - p0_ndc.x) * inv_det * cs_w_inv;

    float ddx_sum = dot(bary.dd_x, float3(1, 1, 1));
    float ddy_sum = dot(bary.dd_y, float3(1, 1, 1));

    float2 delta_vec = pixel_ndc - p0_ndc;
    float interp_inv_w = cs_w_inv.x + delta_vec.x * ddx_sum + delta_vec.y * ddy_sum;
    float interp_w = rcp(interp_inv_w);

    bary.lambda.x = interp_w * (cs_w_inv.x + delta_vec.x * bary.dd_x.x + delta_vec.y * bary.dd_y.x);
    bary.lambda.y = interp_w * (0.0f + delta_vec.x * bary.dd_x.y + delta_vec.y * bary.dd_y.y);
    bary.lambda.z = interp_w * (0.0f + delta_vec.x * bary.dd_x.z + delta_vec.y * bary.dd_y.z);

    bary.dd_x *= 2.0f * viewport_extent_inv.x;
    bary.dd_y *= 2.0f * viewport_extent_inv.y;

    ddx_sum *= 2.0f * viewport_extent_inv.x;
    ddy_sum *= 2.0f * viewport_extent_inv.y;

    float interp_w_ddx = 1.0f / (interp_inv_w + ddx_sum);
    float interp_w_ddy = 1.0f / (interp_inv_w + ddy_sum);

    bary.dd_x = interp_w_ddx * (bary.lambda * interp_inv_w + bary.dd_x) - bary.lambda;
    bary.dd_y = interp_w_ddy * (bary.lambda * interp_inv_w + bary.dd_y) - bary.lambda;

    return bary;
}

// Single interpolant
float3 interpolate_barycentrics_with_derivatives(BarycentricsWithDerivatives barycentrics, float v0, float v1, float v2)
{
    float3 merged = float3(v0, v1, v2);

    return float3(
        dot(merged, barycentrics.lambda),
        dot(merged, barycentrics.dd_x),
        dot(merged, barycentrics.dd_y)
    );
}

// Float3 interpolant
float3 interpolate_barycentrics_simple_float3(float3 barycentric_lambda, float3 v0, float3 v1, float3 v2)
{
    float3 merged_x = float3(v0.x, v1.x, v2.x);
    float3 merged_y = float3(v0.y, v1.y, v2.y);
    float3 merged_z = float3(v0.z, v1.z, v2.z);

    return float3(
        dot(merged_x, barycentric_lambda),
        dot(merged_y, barycentric_lambda),
        dot(merged_z, barycentric_lambda)
    );
}

#endif
