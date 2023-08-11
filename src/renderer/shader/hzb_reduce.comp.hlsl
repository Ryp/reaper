#include "lib/base.hlsl"

#include "lib/morton.hlsl"
#include "hzb_reduce.share.hlsl"

#define USE_QUAD_INTRINSICS 0

VK_PUSH_CONSTANT_HELPER(HZBReducePushConstants) consts;

VK_BINDING(0, Slot_LinearClampSampler) SamplerState LinearClampSampler;
VK_BINDING(0, Slot_SceneDepth) Texture2D<float> SceneDepth;
VK_BINDING(0, Slot_HZB_mips) RWTexture2D<float2> HZB_mips[HZBMaxMipCount];

static const uint ThreadCount = HZBReduceThreadCountX * HZBReduceThreadCountY;

groupshared float2 lds_depth_min_max[ThreadCount];

[numthreads(ThreadCount, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    float depth_min = 1.0;
    float depth_max = 0.0;

    const uint2 local_position_ts = gid.xy * uint2(HZBReduceThreadCountX, HZBReduceThreadCountY) * 2;
    const uint2 position_ts = local_position_ts + decode_morton_2d(gi) * 2 + 1;
    const float2 position_uv = (float2)position_ts * consts.extent_ts_inv;

    if (any(position_ts >= consts.extent_ts))
    {
        return;
    }

    const float4 quad_depth = SceneDepth.GatherRed(LinearClampSampler, position_uv);

    depth_min = min(min(quad_depth.x, quad_depth.y), min(quad_depth.z, quad_depth.w));
    depth_max = max(max(quad_depth.x, quad_depth.y), max(quad_depth.z, quad_depth.w));

    const uint2 position_hzb_mip0_ts = position_ts >> 1;

    if (true)
    {
        HZB_mips[0][position_hzb_mip0_ts] = float2(depth_min, depth_max);
    }

#if USE_QUAD_INTRINSICS
    depth_min = min(depth_min, QuadReadAcrossX(depth_min));
    depth_max = max(depth_max, QuadReadAcrossX(depth_max));

    depth_min = min(depth_min, QuadReadAcrossY(depth_min));
    depth_max = max(depth_max, QuadReadAcrossY(depth_max));
#else
    depth_min = min(depth_min, WaveReadLaneAt(depth_min, gi ^ 1));
    depth_max = max(depth_max, WaveReadLaneAt(depth_max, gi ^ 1));

    depth_min = min(depth_min, WaveReadLaneAt(depth_min, gi ^ 2));
    depth_max = max(depth_max, WaveReadLaneAt(depth_max, gi ^ 2));
#endif

    if (gi % 4 == 0)
    {
        HZB_mips[1][position_hzb_mip0_ts >> 1] = float2(depth_min, depth_max);
    }

    depth_min = min(depth_min, WaveReadLaneAt(depth_min, gi ^ 4));
    depth_max = max(depth_max, WaveReadLaneAt(depth_max, gi ^ 4));

    depth_min = min(depth_min, WaveReadLaneAt(depth_min, gi ^ 8));
    depth_max = max(depth_max, WaveReadLaneAt(depth_max, gi ^ 8));

    if (gi % 16 == 0)
    {
        HZB_mips[2][position_hzb_mip0_ts >> 2] = float2(depth_min, depth_max);
    }

    depth_min = min(depth_min, WaveReadLaneAt(depth_min, gi ^ 16));
    depth_max = max(depth_max, WaveReadLaneAt(depth_max, gi ^ 16));

    depth_min = min(depth_min, WaveReadLaneAt(depth_min, gi ^ 32));
    depth_max = max(depth_max, WaveReadLaneAt(depth_max, gi ^ 32));

    if (gi % 64 == 0)
    {
        HZB_mips[3][position_hzb_mip0_ts >> 3] = float2(depth_min, depth_max);
    }
}
