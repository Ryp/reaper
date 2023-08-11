#include "lib/base.hlsl"

#include "lib/morton.hlsl"
#include "tile_depth_downsample.share.hlsl"

#define INTERLOCKED 0
#define MORTON 1

// NOTE:
// https://gitlab.freedesktop.org/mesa/mesa/-/issues/9039
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineShaderStageCreateFlagBits.html
// See: VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT

VK_PUSH_CONSTANT_HELPER(TileDepthConstants) consts;

VK_BINDING(0, 0) SamplerState Sampler;
VK_BINDING(0, 1) Texture2D<float> SceneDepth;
VK_BINDING(0, 2) RWTexture2D<float> TileDepthMin;
VK_BINDING(0, 3) RWTexture2D<float> TileDepthMax;

static const uint ThreadCount = TileDepthThreadCountX * TileDepthThreadCountY;

#if INTERLOCKED
groupshared uint lds_depth_min;
groupshared uint lds_depth_max;
#else
groupshared float2 lds_depth_min_max[ThreadCount / MinWaveLaneCount];
#endif

#if MORTON
[numthreads(ThreadCount, 1, 1)]
#else
[numthreads(TileDepthThreadCountX, TileDepthThreadCountY, 1)]
#endif
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    float depth_min_cs = 1.0;
    float depth_max_cs = 0.0;

#if INTERLOCKED
    if (gi == 0)
    {
        lds_depth_min = asuint(depth_min_cs);
        lds_depth_max = asuint(depth_max_cs);
    }

    GroupMemoryBarrierWithGroupSync();
#endif

#if MORTON
    const uint2 local_position_ts = gid.xy * uint2(TileDepthThreadCountX, TileDepthThreadCountY) * 2;
    const uint2 position_ts = local_position_ts + decode_morton_2d(gi) * 2 + 1;
#else
    const uint2 position_ts = dtid.xy * 2 + 1;
#endif
    const float2 position_uv = (float2)position_ts * consts.extent_ts_inv;

    // Use a single thread to process 4 texels first
    const float4 quad_depth = SceneDepth.GatherRed(Sampler, position_uv);

    depth_min_cs = min(min(quad_depth.x, quad_depth.y), min(quad_depth.z, quad_depth.w));
    depth_max_cs = max(max(quad_depth.x, quad_depth.y), max(quad_depth.z, quad_depth.w));

    // Reduce as much as we can with wave intrinsics
    depth_min_cs = WaveActiveMin(depth_min_cs);
    depth_max_cs = WaveActiveMax(depth_max_cs);

#if INTERLOCKED
    if (WaveIsFirstLane())
    {
        InterlockedMin(lds_depth_min, asuint(depth_min_cs));
        InterlockedMax(lds_depth_max, asuint(depth_max_cs));
    }

    GroupMemoryBarrierWithGroupSync();
#else
    // Write wave-wide min/max results to LDS
    uint wave_lane_count = WaveGetLaneCount();

    if (WaveIsFirstLane())
    {
        // NOTE: Only valid with VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT
        uint wave_index = gi / wave_lane_count;
        lds_depth_min_max[wave_index] = float2(depth_min_cs, depth_max_cs);
    }

    // Reduce using LDS
    uint active_threads = ThreadCount / wave_lane_count;

    for (uint threads = active_threads / 2; threads > 0; threads /= 2)
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
#endif

    if (gi == 0)
    {
#if INTERLOCKED
        TileDepthMin[gid.xy] = asfloat(lds_depth_min);
        TileDepthMax[gid.xy] = asfloat(lds_depth_max);
#else
        TileDepthMin[gid.xy] = depth_min_cs;
        TileDepthMax[gid.xy] = depth_max_cs;
#endif
    }
}
