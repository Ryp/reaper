#version 450 core

layout(location = 0) in vec3 in_NormalVS;
layout(location = 1) in vec2 in_UV;

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

    const vec3 uvChecker = vec3(fract(in_UV * 10.0).xy, 0.5);
    const vec3 color = uvChecker * max(0.3, NdotL);

    out_Color = vec4(color, 1.0);
}
