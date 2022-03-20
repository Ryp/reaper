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
    output.color = float4(0.5, 0.2, 0.9, 1.0);
}
