struct UniformBufferObject
{
    float4x4 model;
    float4x4 viewProj;
    float3x3 modelViewInvT;
    float timeMs;
};

UniformBufferObject uniformBuffer : register(b0);

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

VS_OUTPUT main(VS_INPUT input)
{
    const float3 positionMS = input.PositionMS * (1.0 + 0.2 * sin(uniformBuffer.timeMs * 0.2));
    const float4 positionWS = float4(positionMS, 1.0) * uniformBuffer.model;
    const float4 positionCS = positionWS * uniformBuffer.viewProj;

    VS_OUTPUT output;

    output.positionCS = positionCS;

    output.NormalVS = input.NormalMS * uniformBuffer.modelViewInvT;
    output.UV = input.UV;

    return output;
}
