#include "lib/base.hlsl"

#include "lib/lighting.hlsl"
#include "lib/gbuffer.hlsl"
#include "tiled_lighting/tiled_lighting.hlsl"

VK_CONSTANT(0) const bool spec_debug_enable_shadows = true;

VK_PUSH_CONSTANT_HELPER(TiledLightingPushConstants) push;

VK_BINDING(0, 0) ConstantBuffer<TiledLightingConstants> pass_params;
VK_BINDING(1, 0) ByteAddressBuffer TileVisibleLightIndices;
VK_BINDING(2, 0) Texture2D<GBuffer0Type> GBuffer0;
VK_BINDING(3, 0) Texture2D<GBuffer1Type> GBuffer1;
VK_BINDING(4, 0) Texture2D<float> DepthNDC;
VK_BINDING(5, 0) StructuredBuffer<PointLightProperties> point_lights;
VK_BINDING(6, 0) SamplerComparisonState shadow_map_sampler;
VK_BINDING(7, 0) RWTexture2D<float4> LightingOutput; // FIXME should be float3 but would trigger validation error
VK_BINDING(8, 0) RWStructuredBuffer<TileDebug> TileDebugOutput;
VK_BINDING(9, 0) Texture2D<float> t_shadow_map[];

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

    // Get tile data
    const uint2 tile_index = gid.xy;
    const uint tile_index_flat = get_tile_index_flat(tile_index, push.tile_count_x);
    const uint tile_start_offset = get_light_list_offset(tile_index_flat);
    const uint tile_light_count = TileVisibleLightIndices.Load(tile_start_offset * 4);

    GBufferRaw gbuffer_raw;
    gbuffer_raw.rt0 = GBuffer0.Load(uint3(position_ts, 0));
    gbuffer_raw.rt1 = GBuffer1.Load(uint3(position_ts, 0));

    const GBuffer gbuffer = decode_gbuffer(gbuffer_raw);

    StandardMaterial material;
    material.albedo = gbuffer.albedo;
    material.roughness = gbuffer.roughness;
    material.f0 = gbuffer.f0;

    LightOutput lighting_accum = (LightOutput)0;

    for (uint slot_index = 0; slot_index < tile_light_count; slot_index++)
    {
        const uint light_index_offset_bytes = (tile_start_offset + 1 + slot_index) * 4;
        const uint light_index = TileVisibleLightIndices.Load(light_index_offset_bytes);

        const PointLightProperties point_light = point_lights[light_index];

        const LightOutput lighting = shade_point_light(point_light, material, position_vs, gbuffer.normal_vs, view_direction_vs);

        float shadow_term = 1.0;
        if (point_light.shadow_map_index != InvalidShadowMapIndex && spec_debug_enable_shadows)
        {
            // NOTE: shadow_map_index MUST be uniform
            shadow_term = sample_shadow_map(t_shadow_map[point_light.shadow_map_index], shadow_map_sampler, point_light.light_ws_to_cs, position_ws);
        }

        lighting_accum.diffuse += lighting.diffuse * shadow_term;
        lighting_accum.specular += lighting.specular * shadow_term;
    }

    float3 lighting_sum = lighting_accum.diffuse + lighting_accum.specular;

    lighting_sum *= gbuffer.albedo;

    if (all(position_ts < push.extent_ts))
    {
        if (depth_ndc == 0.0) // Far plane
        {
            // FIXME match forward clear
            lighting_sum = 0.1;
        }

        LightingOutput[position_ts] = float4(lighting_sum, 0.0);
    }

    if (gi == 0)
    {
        TileDebug debug;

        debug.light_count = tile_light_count;
        TileDebugOutput[tile_index_flat] = debug;
    }
}
