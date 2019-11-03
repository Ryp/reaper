#version 450 core

layout(location = 0) in vec3 in_NormalVS;

layout(location = 0) out vec4 out_Color;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

void main()
{
    const vec3 lightDirectionVS = normalize(vec3(1.0, -1.0, 1.0));
    const vec3 normalVS = normalize(in_NormalVS);

    const float NdotL = saturate(dot(normalVS, lightDirectionVS));

    out_Color = vec4(1.0, 0.4, 1.0, 1.0) * NdotL;
}
