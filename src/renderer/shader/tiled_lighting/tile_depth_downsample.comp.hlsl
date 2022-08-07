#include "lib/base.hlsl"

#include "share/tile_depth.hlsl"

// Input
VK_PUSH_CONSTANT_HELPER(TileDepthConstants) consts;

VK_BINDING(0, 0) SamplerState Sampler;
VK_BINDING(1, 0) Texture2D<float> SceneDepth;
VK_BINDING(2, 0) RWTexture2D<float2> TileDepthMin;
VK_BINDING(3, 0) RWTexture2D<float2> TileDepthMax;

static const uint ThreadCount = TileDepthThreadCountX * TileDepthThreadCountY;

groupshared float2 lds_depth_min_max[ThreadCount];

[numthreads(TileDepthThreadCountX, TileDepthThreadCountY, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    float depth_min_cs = 1.0;
    float depth_max_cs = 0.0;

    const uint2 position_ts = dtid.xy * 2 + 1;
    const float2 position_uv = (float2)position_ts * consts.extent_ts_inv;

    const float4 quad_depth = SceneDepth.GatherRed(Sampler, position_uv);

    depth_min_cs = min(min(quad_depth.x, quad_depth.y), min(quad_depth.z, quad_depth.w));
    depth_max_cs = max(max(quad_depth.x, quad_depth.y), max(quad_depth.z, quad_depth.w));

    lds_depth_min_max[gi] = float2(depth_min_cs, depth_max_cs);

    for (uint threads = ThreadCount / 2; threads > 0; threads /= 2)
    {
        GroupMemoryBarrierWithGroupSync();

        if (gi < threads)
        {
            const float2 depth_min_max_neighbor = lds_depth_min_max[gi + threads];

            depth_min_cs = min(depth_min_cs, depth_min_max_neighbor.x);
            depth_max_cs = max(depth_max_cs, depth_min_max_neighbor.y);

            lds_depth_min_max[gi] = float2(depth_min_cs, depth_max_cs);
        }
    }

    if (gi == 0)
    {
        TileDepthMin[gid.xy] = depth_min_cs;
        TileDepthMax[gid.xy] = depth_max_cs;
    }
}
