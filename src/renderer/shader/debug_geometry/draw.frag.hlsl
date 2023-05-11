#include "lib/base.hlsl"

#include "draw.hlsl"

struct PS_OUTPUT
{
    float3 color : SV_Target0;
};

void main(in VS_OUTPUT input, out PS_OUTPUT output)
{
    output.color = input.color;
}
