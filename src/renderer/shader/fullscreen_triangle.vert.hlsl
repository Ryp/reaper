#include "lib/base.hlsl"

struct VS_INPUT
{
    uint vertex_id : SV_VertexID;
};

struct VS_OUTPUT
{
    float4 PositionCS : SV_Position;
    float2 PositionUV : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    float2 vertex_uv;

    if (input.vertex_id == 0)
        vertex_uv = float2(0.0, 0.0);
    else if (input.vertex_id == 1)
        vertex_uv = float2(0.0, 2.0);
    else
        vertex_uv = float2(2.0, 0.0);

    VS_OUTPUT output;

    output.PositionCS = float4(uv_to_ndc(vertex_uv), 0.0, 1.0);
    output.PositionUV = vertex_uv;

    return output;
}
