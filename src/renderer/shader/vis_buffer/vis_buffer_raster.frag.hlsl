#include "lib/base.hlsl"

#include "vis_buffer.hlsl"

struct PS_INPUT
{
    float4 position_cs : SV_Position;
    nointerpolation uint triangle_id : TEXCOORD0;
    nointerpolation uint instance_id : TEXCOORD1;
};

struct PS_OUTPUT
{
    VisBufferRawType vis_buffer : SV_Target0;
};

void main(in PS_INPUT input, out PS_OUTPUT output)
{
    VisibilityBuffer vis_buffer;
    vis_buffer.triangle_id = input.triangle_id;
    vis_buffer.instance_id = input.instance_id;

    output.vis_buffer = encode_vis_buffer(vis_buffer);
}
