#include "lib/base.hlsl"
#include "lib/vertex_pull.hlsl"

#include "rasterize_light_volume.hlsl"

float3 unit_box_strip(uint vertex_id)
{
    uint b = 1u << vertex_id;

    return float3(
        (0x287A & b) != 0 ? 1.0 : -1.0,
        (0x02AF & b) != 0 ? 1.0 : -1.0,
        (0x31E3 & b) != 0 ? 1.0 : -1.0
    );
}

struct VS_INPUT
{
    uint vertex_id : SV_VertexID;
    uint api_instance_id : SV_InstanceID;
};

void main(VS_INPUT input, out VS_OUTPUT output)
{
    // Patch instance_id
    // https://twitter.com/iquilezles/status/736353148969746432
    const uint instance_id = consts.instance_id_offset + input.api_instance_id;
    const LightVolumeInstance volume_instance = LightVolumeInstances[instance_id];

#if 1
    const float3 vpos_ms = pull_position(VertexPositionsMS, input.vertex_id);
    const float3 position_ms = vpos_ms * volume_instance.radius;
#else
    const float3 position_ms = unit_box_strip(input.vertex_id) * volume_instance.radius;
#endif

    const float4 position_cs = mul(volume_instance.ms_to_cs, float4(position_ms, 1.0));

    output.position = position_cs;
    output.position_cs = position_cs;
    output.instance_id = instance_id;
}
