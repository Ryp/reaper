#include "lib/base.hlsl"

#include "lib/morton.hlsl"
#include "hzb_reduce.share.hlsl"

VK_PUSH_CONSTANT_HELPER(HZBReducePushConstants) consts;

VK_BINDING(0, Slot_LinearClampSampler) SamplerState LinearClampSampler;
VK_BINDING(0, Slot_SceneDepth) Texture2D<float> SceneDepth;
VK_BINDING(0, Slot_HZB_mips) RWTexture2D<float2> HZB_mips[HZBMaxMipCount];

static const uint ThreadCount = HZBReduceThreadCountX * HZBReduceThreadCountY;

groupshared float2 lds_depth_min_max[ThreadCount / MinWaveLaneCount];

[numthreads(ThreadCount, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    float depth_min = 1.0;
    float depth_max = 0.0;

    const uint2 offset_ts = gid.xy * uint2(HZBReduceThreadCountX, HZBReduceThreadCountY);
    const uint2 position_ts = offset_ts + decode_morton_2d(gi);
    const float2 gather_uv = (float2)(position_ts * 2 + 1) * consts.depth_extent_ts_inv;

    const float4 quad_depth = SceneDepth.GatherRed(LinearClampSampler, gather_uv);

    depth_min = min(min(quad_depth.x, quad_depth.y), min(quad_depth.z, quad_depth.w));
    depth_max = max(max(quad_depth.x, quad_depth.y), max(quad_depth.z, quad_depth.w));

    HZB_mips[0][position_ts] = float2(depth_min, depth_max);

    uint lane_index = WaveGetLaneIndex();

    depth_min = min(depth_min, WaveReadLaneAt(depth_min, lane_index ^ 1));
    depth_max = max(depth_max, WaveReadLaneAt(depth_max, lane_index ^ 1));

    depth_min = min(depth_min, WaveReadLaneAt(depth_min, lane_index ^ 2));
    depth_max = max(depth_max, WaveReadLaneAt(depth_max, lane_index ^ 2));

    if (gi % 4 == 0)
    {
        HZB_mips[1][position_ts >> 1] = float2(depth_min, depth_max);
    }

    depth_min = min(depth_min, WaveReadLaneAt(depth_min, lane_index ^ 4));
    depth_max = max(depth_max, WaveReadLaneAt(depth_max, lane_index ^ 4));

    depth_min = min(depth_min, WaveReadLaneAt(depth_min, lane_index ^ 8));
    depth_max = max(depth_max, WaveReadLaneAt(depth_max, lane_index ^ 8));

    if (gi % 16 == 0)
    {
        HZB_mips[2][position_ts >> 2] = float2(depth_min, depth_max);
    }

#if 0
    depth_min = min(depth_min, WaveReadLaneAt(depth_min, lane_index ^ 16));
    depth_max = max(depth_max, WaveReadLaneAt(depth_max, lane_index ^ 16));

    depth_min = min(depth_min, WaveReadLaneAt(depth_min, lane_index ^ 32));
    depth_max = max(depth_max, WaveReadLaneAt(depth_max, lane_index ^ 32));
#else
    if (gi % 16 == 0)
    {
        lds_depth_min_max[gi / 16] = float2(depth_min, depth_max);
    }

    // Reduce using LDS
    uint active_threads = ThreadCount / 16;

    for (uint threads = active_threads / 2; threads > 0; threads /= 2)
    {
        GroupMemoryBarrierWithGroupSync();

        if (gi < threads)
        {
            const float2 depth_min_max_neighbor = lds_depth_min_max[gi + threads];

            depth_min = min(depth_min, depth_min_max_neighbor.x);
            depth_max = max(depth_max, depth_min_max_neighbor.y);

            lds_depth_min_max[gi] = float2(depth_min, depth_max);
        }
    }
#endif

    if (gi % 64 == 0)
    {
        HZB_mips[3][position_ts >> 3] = float2(depth_min, depth_max);
    }
}
