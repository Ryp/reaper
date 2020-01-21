// FIXME padding
struct PassParams
{
    float4x4 viewProj;
    float timeMs;
};

[[vk::binding(0, 0)]]
ConstantBuffer<PassParams> pass_params;

struct InstanceParams
{
    float4x4 model;
    float3x3 modelViewInvT;
};

[[vk::binding(1, 0)]]
StructuredBuffer<InstanceParams> instance_params;

struct VS_INPUT
{
    float3 PositionMS;
    float3 NormalMS;
    float2 UV;
};

struct VS_OUTPUT
{
    float4 positionCS : SV_Position;
    float3 NormalVS;
    float2 UV;
};

VS_OUTPUT main(VS_INPUT input, uint instance_id : SV_InstanceID)
{
    const float3 positionMS = input.PositionMS * (1.0 + 0.3 * sin(pass_params.timeMs * 0.2));
    const float4 positionWS = float4(positionMS, 1.0) * instance_params[instance_id].model;
    const float4 positionCS = positionWS * pass_params.viewProj;

    VS_OUTPUT output;

    output.positionCS = positionCS;

    output.NormalVS = input.NormalMS * instance_params[instance_id].modelViewInvT;
    output.UV = input.UV;

    return output;
}