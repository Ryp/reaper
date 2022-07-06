#include "lib/base.hlsl"

VK_BINDING(0, 0) Texture2D<float> Depth;

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
    output.depth = Depth.Load(int3(input.PositionCS.xy, 0));
}
