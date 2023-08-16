#include "lib/base.hlsl"

#include "copy_to_depth_from_hzb.share.hlsl"

VK_PUSH_CONSTANT_HELPER(CopyDepthFromHZBPushConstants) consts;

VK_BINDING(0, 0) Texture2D<float2> HZBMip;

struct PS_INPUT
{
    float4 PositionCS : SV_Position;
    float2 PositionUV : TEXCOORD0;
};

struct PS_OUTPUT
{
    float depth : SV_Depth;
};

void main(in PS_INPUT input, out PS_OUTPUT output)
{
    const float2 depth_min_max_cs = HZBMip.Load(int3(input.PositionCS.xy, 0));

    if (consts.copy_min)
    {
        output.depth = depth_min_max_cs.x;
    }
    else
    {
        output.depth = depth_min_max_cs.y;
    }
}
