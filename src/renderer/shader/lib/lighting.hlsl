////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_LIGHTING_INCLUDED
#define LIB_LIGHTING_INCLUDED

#include "lib/brdf.hlsl"
#include "lighting.share.hlsl"

float sample_shadow_map(Texture2D<float> shadow_map, SamplerComparisonState cmp_sampler, float4x4 light_transform_ws_to_cs, float3 object_position_ws)
{
    const float4 position_shadow_map_cs = mul(light_transform_ws_to_cs, float4(object_position_ws, 1.0));
    const float3 position_shadow_map_ndc = position_shadow_map_cs.xyz / position_shadow_map_cs.w;
    const float2 position_shadow_map_uv = ndc_to_uv(position_shadow_map_ndc.xy);

    const float shadow_depth_bias = 0.001;
    const float shadow_pcf = shadow_map.SampleCmpLevelZero(cmp_sampler, position_shadow_map_uv, position_shadow_map_ndc.z + shadow_depth_bias);

    return shadow_pcf;
}

struct LightOutput
{
    float3 diffuse;
    float3 specular;
};

float distance_falloff_point(float light_distance_sq)
{
    return rcp(1.0 + light_distance_sq);
}

LightOutput shade_point_light(
    PointLightProperties point_light, StandardMaterial material,
    float3 object_position_vs, float3 object_normal_vs, float3 view_direction_vs)
{
    const float3 object_to_light_vs = point_light.position_vs - object_position_vs;
    const float light_distance_sq = dot(object_to_light_vs, object_to_light_vs);
    const float light_distance_inv = rsqrt(max(light_distance_sq, 0.0001));
    const float light_distance_falloff = distance_falloff_point(light_distance_sq);
    const float3 light_direction_vs = object_to_light_vs * light_distance_inv;

    const float dot_nl_sat = saturate(dot(object_normal_vs, light_direction_vs));

    float attenuation = point_light.intensity * light_distance_falloff;
    if (light_distance_sq > point_light.radius_sq)
    {
        attenuation = 0.0;
    }

    LightOutput output;
    output.diffuse = point_light.color * attenuation * diffuse_brdf(dot_nl_sat);
    output.specular = point_light.color * attenuation * specular_brdf(material, object_normal_vs, view_direction_vs, light_direction_vs);

    return output;
}

#endif
