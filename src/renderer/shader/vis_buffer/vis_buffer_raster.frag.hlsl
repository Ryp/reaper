#include "lib/base.hlsl"

#include "vis_buffer.hlsl"

struct PS_INPUT
{
    float4 position_cs : SV_Position;
    nointerpolation uint visible_meshlet_index : TEXCOORD0;

    // NOTE: using SV_PrimitiveID might hurt us
    // See https://twitter.com/SebAaltonen/status/1480831952118759430
    // And https://twitter.com/SebAaltonen/status/1483028277052821504
    uint triangle_id : SV_PrimitiveID;
};

struct PS_OUTPUT
{
    VisBufferRawType vis_buffer_raw : SV_Target0;
};

void main(in PS_INPUT input, out PS_OUTPUT output)
{
    VisibilityBuffer vis_buffer;
    vis_buffer.visible_meshlet_index = input.visible_meshlet_index;
    vis_buffer.meshlet_triangle_id = input.triangle_id;

    output.vis_buffer_raw = encode_vis_buffer(vis_buffer);
}
