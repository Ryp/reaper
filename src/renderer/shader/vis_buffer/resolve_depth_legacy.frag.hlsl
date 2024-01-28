#include "lib/base.hlsl"

VK_BINDING(0, 0) Texture2DMS<float> DepthMS;

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
    uint2 position_ts = input.PositionCS.xy;
    uint sample_location = (position_ts.x & 1) + (position_ts.y & 1) * 2;

    const float depth = DepthMS.Load(uint3(position_ts / 2, 0), sample_location);

    output.depth = depth;
}
