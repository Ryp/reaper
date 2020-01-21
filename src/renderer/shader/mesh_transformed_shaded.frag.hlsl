struct PS_INPUT
{
    float4 positionCS : SV_Position;
    float3 NormalVS;
    float2 UV;
};

struct PS_OUTPUT
{
    float4 color : SV_Target0;
};

PS_OUTPUT main(PS_INPUT input)
{
    const float3 lightDirectionVS = float3(0.0, 1.0, 0.0);
    const float3 normalVS = normalize(input.NormalVS);

    const float NdotL = saturate(dot(normalVS, lightDirectionVS));

    const float3 uvChecker = float3(frac(input.UV * 10.0).xy, 1.0);
    const float3 color = uvChecker * max(0.3, NdotL);

    PS_OUTPUT output;

    output.color = float4(color, 1.0);

    return output;
}
