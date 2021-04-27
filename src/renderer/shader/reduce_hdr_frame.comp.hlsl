#include "lib/base.hlsl"

#include "lib/color_space.hlsl"

#include "share/hdr.hlsl"

//------------------------------------------------------------------------------
// Input

VK_BINDING(0, 0) ConstantBuffer<ReduceHDRPassParams> pass_params;

VK_BINDING(1, 0) Texture2D<float3> t_hdr_color;

//------------------------------------------------------------------------------
// Output

VK_BINDING(2, 0) RWByteAddressBuffer HistogramOut;

//------------------------------------------------------------------------------

uint compute_histogram_bucket_index(float luma)
{
    return saturate((log2(luma) + HISTOGRAM_EV_OFFSET) / HISTOGRAM_EV_COUNT) * (float)HISTOGRAM_RES;
}

// FIXME Verify this
float compute_histogram_bucket_luma(uint bucket_index)
{
    return exp2(((float)bucket_index / (float)HISTOGRAM_RES) * HISTOGRAM_EV_COUNT - HISTOGRAM_EV_OFFSET);
}

groupshared uint lds_histogram[HISTOGRAM_RES];

[numthreads(ReduceHDRGroupSizeX, ReduceHDRGroupSizeY, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    const uint2 position_ts = dtid.xy;

    if (gi == 0)
    {
        for (uint i = 0; i < HISTOGRAM_RES; i++)
        {
            lds_histogram[i] = 0;
        }
    }

    if (!all(position_ts < pass_params.input_size_ts))
        return;

    GroupMemoryBarrierWithGroupSync();

    const float3 scene_color_rec709_linear = t_hdr_color[position_ts];

    const float scene_luma_rec709_linear = luma_rec709(scene_color_rec709_linear);

    const uint bucket_index = compute_histogram_bucket_index(scene_luma_rec709_linear);

    InterlockedAdd(lds_histogram[bucket_index], 1);

    GroupMemoryBarrierWithGroupSync();

    // Write final result from LDS to main memory
    // NOTE: This is not particularly optimized, we could try to distribute atomics accross the whole invocation
    if (gi == 0)
    {
        for (uint i = 0; i < HISTOGRAM_RES; i++)
        {
            const uint histogram_element_size_in_bytes = 4;
            HistogramOut.InterlockedAdd(i * histogram_element_size_in_bytes, lds_histogram[i]);
        }
    }
}
