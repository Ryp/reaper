#include "lib/base.hlsl"

#include "debug_gradient.share.hlsl"

// NOTE: this shader is not part of REAPER_SHADER_SRCS and is not built by the
// CMake side. It exists so the Zig port has a frame graph smoke test that needs
// no scene: one compute pass writing a known pattern, which the graph then
// moves around. Delete it once real passes cover the same ground.

VK_PUSH_CONSTANT_HELPER(DebugGradientPushConstants) Consts;

VK_BINDING(0, 0) [[spv::format_rgba8]] RWTexture2D<float4> t_output;

[numthreads(DebugGradientThreadCountX, DebugGradientThreadCountY, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    const uint2 position_ts = dtid.xy;

    if (any(position_ts >= Consts.extent_ts))
        return;

    const float2 position_uv = (float2(position_ts) + 0.5) * Consts.extent_ts_inv;

    t_output[position_ts] = float4(position_uv.x, position_uv.y, 0.0, 1.0);
}
