#include "lib/base.hlsl"

#include "lib/color_space.hlsl"
#include "lib/morton.hlsl"

#include "reduce_exposure.share.hlsl"

VK_PUSH_CONSTANT_HELPER(ReduceExposureTailPassParams) Consts;

VK_BINDING(0, 0) SamplerState LinearClampSampler;
VK_BINDING(0, 1) Texture2D<float2> AvgLog2LuminanceInput;
VK_BINDING(0, 2) RWByteAddressBuffer AvgLog2LuminanceBuffer;
VK_BINDING(0, 3) globallycoherent RWByteAddressBuffer TailCounter;
VK_BINDING(0, 4) globallycoherent RWTexture2D<float2> AvgLog2LuminanceTail;

static const uint ThreadCount = ExposureThreadCountX * ExposureThreadCountY;

groupshared float lds_luma_log2[ThreadCount / MinWaveLaneCount];
groupshared float lds_weight[ThreadCount / MinWaveLaneCount];
groupshared uint lds_thread_group_index;

[numthreads(ThreadCount, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    const uint2 offset_ts = gid.xy * uint2(ExposureThreadCountX, ExposureThreadCountY);
    uint2 position_ts = offset_ts + decode_morton_2d(gi);

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
        // FIXME This might matter for the tail!
        float2 luma_weight = AvgLog2LuminanceInput.Sample(LinearClampSampler, position_uv);

        sum_luma_log2 = luma_weight.x;
        sum_weight = luma_weight.y;
    }

    sum_luma_log2 = WaveActiveSum(sum_luma_log2);
    sum_weight = WaveActiveSum(sum_weight);

    uint wave_lane_count = WaveGetLaneCount();

    if (WaveIsFirstLane())
    {
#if ENABLE_SHARED_FLOAT_ATOMICS
        // FIXME this needs support for atomic adds and that's not supported on my Intel iGPU
        InterlockedAdd(lds_sum_luma_log2, sum_luma_log2);
        InterlockedAdd(lds_sum_weight, sum_weight);
#else
        uint wave_index = gi / wave_lane_count;
        lds_luma_log2[wave_index] = sum_luma_log2;
        lds_weight[wave_index] = sum_weight;
#endif
    }

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

    if (gi == 0)
    {
        uint2 group_size = uint2(ExposureThreadCountX, ExposureThreadCountY);
        uint2 output_position_ts = position_ts / (group_size * 2);

        // NOTE: data is already in the register from LDS
        AvgLog2LuminanceTail[output_position_ts] = float2(sum_luma_log2 / sum_weight, sum_weight / float(ThreadCount));

        // Like AMD's SPD code, we're keeping the last thread group alive to make another go at the rest of the tail.
        TailCounter.InterlockedAdd(0, uint(1), lds_thread_group_index);
    }

    GroupMemoryBarrierWithGroupSync();

    // Exit if we're not the last thread group running
    if (lds_thread_group_index != Consts.last_thread_group_index)
        return;

    // =========================================================================
    // FIXME reduce the rest of the mip chain

    position_ts = decode_morton_2d(gi); // FIXME

    const bool is_active2 = all(position_ts < Consts.tail_extent_ts); // FIXME

    sum_luma_log2 = 0.f; // FIXME
    sum_weight = 0.f; // FIXME

    if (is_active2)
    {
        float2 luma_weight = AvgLog2LuminanceTail[position_ts];

        sum_luma_log2 = luma_weight.x;
        sum_weight = luma_weight.y;
    }

    sum_luma_log2 = WaveActiveSum(sum_luma_log2);
    sum_weight = WaveActiveSum(sum_weight);

    // uint wave_lane_count = WaveGetLaneCount();

    if (WaveIsFirstLane())
    {
#if ENABLE_SHARED_FLOAT_ATOMICS
        // FIXME this needs support for atomic adds and that's not supported on my Intel iGPU
        InterlockedAdd(lds_sum_luma_log2, sum_luma_log2);
        InterlockedAdd(lds_sum_weight, sum_weight);
#else
        uint wave_index = gi / wave_lane_count;
        lds_luma_log2[wave_index] = sum_luma_log2;
        lds_weight[wave_index] = sum_weight;
#endif
    }

    // Manual LDS reduce pass
    //uint active_threads = ThreadCount / wave_lane_count;

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

    // =========================================================================

    if (gi == 0)
    {
        // NOTE: data is already in the register from LDS
        float average_luma_log = sum_luma_log2 / sum_weight;

        AvgLog2LuminanceBuffer.Store(0, asuint(average_luma_log));

        // Clear counter for next passes
        TailCounter.Store(0, 0);
    }
}
