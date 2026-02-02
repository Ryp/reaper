#include "lib/base.hlsl"

#include "lib/color_space.hlsl"
#include "lib/morton.hlsl"

#include "reduce_exposure.share.hlsl"

// NOTE: untested
// This needs support for atomic adds and that's not there on my Intel iGPU
// See also: shaderSharedFloat32AtomicAdd.
#define ENABLE_SHARED_FLOAT_ATOMICS 0

VK_PUSH_CONSTANT_HELPER(ReduceExposurePassParams) Consts;

VK_BINDING(0, 0) SamplerState LinearClampSampler;
VK_BINDING(0, 1) Texture2D<float3> SceneHDR;
VK_BINDING(0, 2) [[spv::format_rg16f]] RWTexture2D<float2> AvgLog2Luminance;

static const uint ThreadCount = ExposureThreadCountX * ExposureThreadCountY;

#if ENABLE_SHARED_FLOAT_ATOMICS
groupshared float lds_sum_luma_log2;
groupshared float lds_sum_weight;
#else
groupshared float lds_luma_log2[ThreadCount / MinWaveLaneCount];
groupshared float lds_weight[ThreadCount / MinWaveLaneCount];
#endif

#if ENABLE_SHARED_FLOAT_ATOMICS
[numthreads(ExposureThreadCountX, ExposureThreadCountY, 1)]
#else
[numthreads(ThreadCount, 1, 1)]
#endif
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
#if ENABLE_SHARED_FLOAT_ATOMICS
    if (gi == 0)
    {
        lds_sum_luma_log2 = 0.f;
        lds_sum_weight = 0.f;
    }

    GroupMemoryBarrierWithGroupSync();
#endif

#if ENABLE_SHARED_FLOAT_ATOMICS
    uint2 position_ts = dtid.xy;
#else
    const uint2 offset_ts = gid.xy * uint2(ExposureThreadCountX, ExposureThreadCountY);
    uint2 position_ts = offset_ts + decode_morton_2d(gi);
#endif

    position_ts = position_ts * 2 + 1;

    const bool is_active = all(position_ts < Consts.extent_ts);

    float sum_luma_log2 = 0.f;
    float sum_weight = 0.f;

    if (is_active)
    {
        float2 position_uv = (float2)position_ts * Consts.extent_ts_inv;

        // We're not taking into account invalid pixels that would be gathered outside of the range when using even resolutions.
        // This introduces a small inaccuracy that I'm ignoring.
        // Hopefully none of those inaccuracies matter when using dynamic resolution.
        float4 quad_r = SceneHDR.GatherRed(LinearClampSampler, position_uv);
        float4 quad_g = SceneHDR.GatherGreen(LinearClampSampler, position_uv);
        float4 quad_b = SceneHDR.GatherBlue(LinearClampSampler, position_uv);

        float3 quad01_scene_color_srgb_linear = float3(quad_r.x, quad_g.x, quad_b.x);
        float3 quad11_scene_color_srgb_linear = float3(quad_r.y, quad_g.y, quad_b.y);
        float3 quad10_scene_color_srgb_linear = float3(quad_r.z, quad_g.z, quad_b.z);
        float3 quad00_scene_color_srgb_linear = float3(quad_r.w, quad_g.w, quad_b.w);

        float4 luma_log2 = log2(max(0.00001f, float4(
            luma_srgb(quad01_scene_color_srgb_linear),
            luma_srgb(quad11_scene_color_srgb_linear),
            luma_srgb(quad10_scene_color_srgb_linear),
            luma_srgb(quad00_scene_color_srgb_linear))));

        sum_luma_log2 = dot(luma_log2, 1.f / 4.f);
        sum_weight = 1.f;
    }

    sum_luma_log2 = WaveActiveSum(sum_luma_log2);
    sum_weight = WaveActiveSum(sum_weight);

    uint wave_lane_count = WaveGetLaneCount();

    if (WaveIsFirstLane())
    {
#if ENABLE_SHARED_FLOAT_ATOMICS
        InterlockedAdd(lds_sum_luma_log2, sum_luma_log2);
        InterlockedAdd(lds_sum_weight, sum_weight);
#else
        uint wave_index = gi / wave_lane_count;
        lds_luma_log2[wave_index] = sum_luma_log2;
        lds_weight[wave_index] = sum_weight;
#endif
    }

#if ENABLE_SHARED_FLOAT_ATOMICS
    GroupMemoryBarrierWithGroupSync();
#else
    // Manual LDS reduce pass
    uint active_threads = ThreadCount / wave_lane_count;

    for (uint threads = active_threads / 2; threads > 0; threads /= 2)
    {
        GroupMemoryBarrierWithGroupSync();

        if (gi < threads)
        {
            sum_luma_log2 += lds_luma_log2[gi + threads];
            sum_weight += lds_weight[gi + threads];

            lds_luma_log2[gi] = sum_luma_log2;
            lds_weight[gi] = sum_weight;
        }
    }
#endif

    if (gi == 0)
    {
        uint2 group_size = uint2(ExposureThreadCountX, ExposureThreadCountY);
        uint2 output_position_ts = position_ts / (group_size * 2);

#if ENABLE_SHARED_FLOAT_ATOMICS
        sum_luma_log2 = lds_sum_luma_log2;
        sum_weight = lds_sum_weight;
#else
        // NOTE: data is already in the register from LDS
#endif

        AvgLog2Luminance[output_position_ts] = float2(sum_luma_log2 / sum_weight, sum_weight / float(ThreadCount));
    }
}
