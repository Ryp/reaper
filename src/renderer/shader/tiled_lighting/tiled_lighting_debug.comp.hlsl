#include "lib/base.hlsl"
#include "lib/debug.hlsl"

#include "tiled_lighting/tiled_lighting.hlsl"

VK_PUSH_CONSTANT_HELPER(TiledLightingDebugPushConstants) push;

VK_BINDING(0, 0) StructuredBuffer<TileDebug> TileDebugBuffer;
VK_BINDING(0, 1) RWTexture2D<float4> DebugTexture;

[numthreads(TiledLightingThreadCountX, TiledLightingThreadCountY, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    const uint2 position_ts = dtid.xy;

    if (all(position_ts < push.extent_ts))
    {
        // Get tile data
        const uint2 tile_index = gid.xy;
        const uint tile_index_flat = get_tile_index_flat(tile_index, push.tile_count_x);
        const TileDebug debug_info = TileDebugBuffer[tile_index_flat];

        static const uint UnsafeLightCount = 3;
        const float heat = float(debug_info.light_count) / float(UnsafeLightCount);

        const float3 debug_color = heatmap_color(heat);

        DebugTexture[position_ts] = float4(debug_color, 1.f);
    }
}
