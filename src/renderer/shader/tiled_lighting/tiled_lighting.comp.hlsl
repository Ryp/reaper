#include "lib/base.hlsl"

#include "lib/lighting.hlsl"
#include "tiled_lighting/tiled_lighting.hlsl"

VK_PUSH_CONSTANT_HELPER(TiledLightingPushConstants) push;

VK_BINDING(0, 0) ConstantBuffer<TiledLightingConstants> pass_params;
VK_BINDING(1, 0) ByteAddressBuffer TileVisibleLightIndices;
VK_BINDING(2, 0) Texture2D<float> DepthNDC;
VK_BINDING(3, 0) StructuredBuffer<PointLightProperties> point_lights;
VK_BINDING(4, 0) SamplerComparisonState shadow_map_sampler;
VK_BINDING(5, 0) RWTexture2D<float3> LightingOutput;
VK_BINDING(6, 0) Texture2D<float> t_shadow_map[];

VK_BINDING(0, 1) SamplerState diffuse_map_sampler;
VK_BINDING(1, 1) Texture2D<float3> t_diffuse_map[];

[numthreads(TiledLightingThreadCountX, TiledLightingThreadCountY, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    const uint2 position_ts = dtid.xy;

    // Reconstruct view-space position
    const float depth_ndc = DepthNDC.Load(uint3(position_ts, 0));
    const float2 position_uv = ts_to_uv(position_ts, push.extent_ts_inv);
    const float3 position_ndc = float3(uv_to_ndc(position_uv), depth_ndc);
    const float4 position_vs_h = mul(pass_params.cs_to_vs, float4(position_ndc, 1.0));
    const float3 position_vs = position_vs_h.xyz / position_vs_h.w;

    const float3 view_direction_vs = -normalize(position_vs);

    // Get world-space position for shadows
    const float3 position_ws = mul(pass_params.vs_to_ws, float4(position_vs, 1.0));

    // FIXME Entirely faked, we need normals for tiled lighting
    const float3 normal_vs = float3(0.0, 0.0, -1.0);

    // Get tile data
    const uint2 tile_index = gid.xy;
    const uint tile_index_flat = get_tile_index_flat(tile_index, push.tile_count_x);
    const uint tile_start_offset = get_tile_offset(tile_index_flat);
    const uint tile_light_count = TileVisibleLightIndices.Load(tile_start_offset * 4);

    StandardMaterial material;
    material.albedo = 1.f; // FIXME For now gets multiplied in the swapchain pass
    material.roughness = 0.5;
    material.f0 = 0.1;

    LightOutput lighting_accum = (LightOutput)0;

    for (uint i = 0; i < tile_light_count; i++)
    {
        const PointLightProperties point_light = point_lights[i];

        const LightOutput lighting = shade_point_light(point_light, material, position_vs, normal_vs, view_direction_vs);

        // NOTE: shadow_map_index MUST be uniform
        const float shadow_term = sample_shadow_map(t_shadow_map[point_light.shadow_map_index], shadow_map_sampler, point_light.light_ws_to_cs, position_ws);

        lighting_accum.diffuse += lighting.diffuse * shadow_term;
        lighting_accum.specular += lighting.specular * shadow_term;
    }

    const float3 lighting_sum = lighting_accum.diffuse + lighting_accum.specular;

    if (all(position_ts < push.extent_ts))
    {
        LightingOutput[position_ts] = lighting_sum;
    }
}
