#include "lib/base.hlsl"

#include "lib/color_space.hlsl"

#include "reduce_histogram.share.hlsl"

// Input
VK_PUSH_CONSTANT_HELPER(ReduceHDRPassParams) consts;

VK_BINDING(0, 0) SamplerState Sampler;
VK_BINDING(1, 0) Texture2D<float3> t_hdr_color;

// Output
VK_BINDING(2, 0) globallycoherent RWByteAddressBuffer HistogramOut;

uint compute_histogram_bucket_index(float luma)
{
    return saturate((log2(luma) + HistogramEVOffset) / HistogramEVCount) * (float)HistogramRes;
}

// FIXME Verify this
float compute_histogram_bucket_luma(uint bucket_index)
{
    return exp2(((float)bucket_index / (float)HistogramRes) * HistogramEVCount - HistogramEVOffset);
}

groupshared uint lds_histogram[HistogramRes];

static const uint ThreadCountTotal = HistogramThreadCountX * HistogramThreadCountY;

[numthreads(HistogramThreadCountX, HistogramThreadCountY, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    const uint2 position_ts = dtid.xy * 2 + 1;

    const uint histogram_slots_per_thread = HistogramRes / ThreadCountTotal;
    const uint histogram_thread_start = gi * histogram_slots_per_thread;
    const uint histogram_thread_stop = histogram_thread_start + histogram_slots_per_thread;

    {
        for (uint i = histogram_thread_start; i < histogram_thread_stop; i++)
        {
            lds_histogram[i] = 0;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    const bool is_active = all(position_ts < consts.extent_ts);

    if (is_active)
    {
        const float2 position_uv = (float2)position_ts * consts.extent_ts_inv;

        const float4 quad_r = t_hdr_color.GatherRed(Sampler, position_uv);
        const float4 quad_g = t_hdr_color.GatherGreen(Sampler, position_uv);
        const float4 quad_b = t_hdr_color.GatherBlue(Sampler, position_uv);

        const float3 quad01_scene_color_srgb_linear = float3(quad_r.x, quad_g.x, quad_b.x);
        const float3 quad11_scene_color_srgb_linear = float3(quad_r.y, quad_g.y, quad_b.y);
        const float3 quad10_scene_color_srgb_linear = float3(quad_r.z, quad_g.z, quad_b.z);
        const float3 quad00_scene_color_srgb_linear = float3(quad_r.w, quad_g.w, quad_b.w);

        const uint bucket_index01 = compute_histogram_bucket_index(luma_srgb(quad01_scene_color_srgb_linear));
        const uint bucket_index11 = compute_histogram_bucket_index(luma_srgb(quad11_scene_color_srgb_linear));
        const uint bucket_index10 = compute_histogram_bucket_index(luma_srgb(quad10_scene_color_srgb_linear));
        const uint bucket_index00 = compute_histogram_bucket_index(luma_srgb(quad00_scene_color_srgb_linear));

        InterlockedAdd(lds_histogram[bucket_index01], 1);
        InterlockedAdd(lds_histogram[bucket_index11], 1);
        InterlockedAdd(lds_histogram[bucket_index10], 1);
        InterlockedAdd(lds_histogram[bucket_index00], 1);
    }

    GroupMemoryBarrierWithGroupSync();

    {
        for (uint i = histogram_thread_start; i < histogram_thread_stop; i++)
        {
            const uint histogram_element_size_in_bytes = 4;
            HistogramOut.InterlockedAdd(i * histogram_element_size_in_bytes, lds_histogram[i]);
        }
    }
}
