#include "lib/base.hlsl"

#include "lib/tonemapping.hlsl"

#include "tone_mapping_bake_lut.share.hlsl"

VK_PUSH_CONSTANT_HELPER(ToneMappingBakeLUT_Consts) Consts;

VK_BINDING(0, 0) RWTexture1D<float> ToneMappingLUT;

[numthreads(ToneMappingBakeLUT_ThreadCount, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    uint position_ts = dtid.x;

    // Avoid using the half-texel range at the texture boundaries
    float position_uv = (float)position_ts / (float)(ToneMappingBakeLUT_Res - 1);

    position_uv = remap_lut_input_range_inverse(position_uv);

    ToneMappingLUT[position_ts] = tonemapping_gran_turismo_2025(position_uv, Consts.min_nits, Consts.max_nits);
}
