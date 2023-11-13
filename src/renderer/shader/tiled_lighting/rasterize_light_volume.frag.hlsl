#include "lib/base.hlsl"

#include "rasterize_light_volume.hlsl"
#include "tiled_lighting.hlsl"

// FIXME use spec constants
static const bool spec_tile_lighting_enable_ps_culling = false;
static const bool spec_is_inner_pass = false;

// Fast ray/aabb test
// Returns only forward hits - and does NOT care about ordering.
bool slabs(float3 p0, float3 p1, float3 ray_origin, float3 ray_direction_inv)
{
    const float3 t0 = (p0 - ray_origin) * ray_direction_inv;
    const float3 t1 = (p1 - ray_origin) * ray_direction_inv;
    const float3 tmin = min(t0, t1), tmax = max(t0, t1);
    const float intersect_a = max3(tmin);
    const float intersect_b = min3(tmax);

    return (intersect_a < intersect_b) && (intersect_b > 0.f);
}

[earlydepthstencil]
void main(in VS_OUTPUT input)
{
    const uint2 position_ts = input.position.xy;
    const LightVolumeInstance instance_data = LightVolumeInstances[input.instance_id];

    const float ray_depth_ndc = TileDepth.Load(uint3(position_ts, 0));
    const float ray_sign = spec_is_inner_pass ? -1.0 : 1.0;

    bool is_visible = true;

    if (spec_tile_lighting_enable_ps_culling)
    {
        // Per-tile culling with a ray/aabb test. Simply comparing depth is not sufficient.
        // FIXME enabling conservative raster is not sufficient in this case, the collision volume also needs to be expanded for the ray cast to be correct
        const float3 ray_position_ndc = float3(input.position_cs.xy / input.position_cs.w, ray_depth_ndc);
        const float4 ray_position_ms_h = mul(instance_data.cs_to_ms, float4(ray_position_ndc, 1.0));
        const float3 ray_position_ms = ray_position_ms_h.xyz / ray_position_ms_h.w;

        const float3 ray_direction_cs = ray_position_ndc;
        const float4 ray_direction_vs_h = mul(instance_data.cs_to_vs, float4(ray_direction_cs, 1.0));
        const float3 ray_direction_vs = (ray_direction_vs_h.xyz / ray_direction_vs_h.w) * ray_sign;
        const float3 ray_direction_ms = mul(instance_data.vs_to_ms, float4(ray_direction_vs, 0.0));
        const float3 ray_direction_ms_inv = 1.f / ray_direction_ms;

        const float3 aabb_min_ms = -1.0;
        const float3 aabb_max_ms = 1.0;
        is_visible = is_visible && slabs(aabb_min_ms, aabb_max_ms, ray_position_ms, ray_direction_ms_inv);
    }

    if (is_visible)
    {
        const uint tile_index_flat = get_tile_index_flat(position_ts, consts.tile_count_x);
        const uint tile_start_offset = get_light_list_offset(tile_index_flat);

        // Add the light to the visible lights array for this tile
        uint visible_light_array_index;
        TileVisibleLightIndices.InterlockedAdd(tile_start_offset * 4, 1u, visible_light_array_index);

        // Discard if we filled the buffer
        if (visible_light_array_index < LightsPerTileMax)
        {
            const uint next_light_offset = tile_start_offset + 1 + visible_light_array_index;
            TileVisibleLightIndices.Store(next_light_offset * 4, instance_data.light_index);
        }
    }
}
