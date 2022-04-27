#include "lib/base.hlsl"

struct PS_INPUT
{
    float4 PositionCS : SV_Position;
    float2 PositionUV : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 color : SV_Target0;
};

void main(in PS_INPUT input, out PS_OUTPUT output)
{
    if (all(input.PositionUV > 0.8) && all(input.PositionUV < 0.9))
    {
        output.color = float4(0.5, 0.2, 0.9, input.PositionUV.y * input.PositionUV.x);
    }
    else
    {
        output.color = 0.0;
    }
}
