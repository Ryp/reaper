#include "lib/base.hlsl"

#include "lib/lighting.hlsl"
#include "share/forward.hlsl"
#include "share/lighting.hlsl"

static const uint debug_mode_none = 0;
static const uint debug_mode_normals = 1;
static const uint debug_mode_uv = 2;
static const uint debug_mode_pos_vs = 3;

// https://github.com/microsoft/DirectXShaderCompiler/issues/2957
#if defined(_DXC)
VK_CONSTANT(0) const uint spec_debug_mode = 0;
#else
VK_CONSTANT(0) const uint spec_debug_mode = debug_mode_none;
#endif
VK_CONSTANT(1) const bool spec_debug_enable_shadows = true;
VK_CONSTANT(2) const bool spec_debug_enable_lighting = true;

VK_BINDING(0, 0) ConstantBuffer<ForwardPassParams> pass_params;

VK_BINDING(5, 0) StructuredBuffer<PointLightProperties> point_lights;
VK_BINDING(6, 0) SamplerComparisonState shadow_map_sampler;
VK_BINDING(7, 0) Texture2D<float> t_shadow_map[];

VK_BINDING(0, 1) SamplerState diffuse_map_sampler;
VK_BINDING(1, 1) Texture2D<float3> t_diffuse_map[];

struct PS_INPUT
{
    float4 PositionCS   : SV_Position;
    float3 PositionVS   : TEXCOORD0;
    float3 NormalVS     : TEXCOORD1;
    float2 UV           : TEXCOORD2;
    float3 PositionWS   : TEXCOORD3;
    nointerpolation uint texture_index : TEXCOORD4;
};

struct PS_OUTPUT
{
    float3 color : SV_Target0;
};

void main(in PS_INPUT input, out PS_OUTPUT output)
{
    const float3 view_direction_vs = -normalize(input.PositionVS);
    const float3 normal_vs = normalize(input.NormalVS);

    StandardMaterial material;
    material.albedo = t_diffuse_map[input.texture_index].Sample(diffuse_map_sampler, input.UV);
    material.roughness = 0.5;
    material.f0 = 0.1;

    LightOutput lighting_accum = (LightOutput)0;

    for (uint i = 0; i < pass_params.point_light_count; i++)
    {
        const PointLightProperties point_light = point_lights[i];

        const LightOutput lighting = shade_point_light(point_light, material, input.PositionVS, normal_vs, view_direction_vs);

        float shadow_term = 1.0;
        if (point_light.shadow_map_index != InvalidShadowMapIndex && spec_debug_enable_shadows)
        {
            // NOTE: shadow_map_index MUST be uniform
            shadow_term = sample_shadow_map(t_shadow_map[point_light.shadow_map_index], shadow_map_sampler, point_light.light_ws_to_cs, input.PositionWS);
        }

        lighting_accum.diffuse += lighting.diffuse * shadow_term;
        lighting_accum.specular += lighting.specular * shadow_term;
    }

    float3 shaded_color = material.albedo;

    if (spec_debug_enable_lighting)
    {
        shaded_color *= (lighting_accum.diffuse + lighting_accum.specular);
    }

    if (spec_debug_mode == debug_mode_none)
        output.color = shaded_color;
    else if (spec_debug_mode == debug_mode_normals)
        output.color = normal_vs * 0.5 + 0.5;
    else if (spec_debug_mode == debug_mode_uv)
        output.color = float3(input.UV, 0.0);
    else if (spec_debug_mode == debug_mode_pos_vs)
        output.color = input.PositionVS;
    else
        output.color = float3(0.42, 0.42, 0.42);
}
