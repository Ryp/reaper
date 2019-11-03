#version 450 core

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 viewProj;
    mat3 modelViewInvT;
    float timeMs;
} uniformBuffer;

layout(location = 0) in vec3 in_PositionMS;
layout(location = 1) in vec3 in_NormalMS;
layout(location = 2) in vec2 in_UV;

layout(location = 0) out vec3 out_NormalVS;
layout(location = 1) out vec2 out_UV;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    const vec3 positionMS = in_PositionMS * (1.0 + 0.2 * sin(uniformBuffer.timeMs * 0.2));
    const vec4 positionWS = uniformBuffer.model * vec4(positionMS, 1.0);
    const vec4 positionCS = uniformBuffer.viewProj * positionWS;

    gl_Position = positionCS;
    out_NormalVS = uniformBuffer.modelViewInvT * in_NormalMS;
    out_UV = in_UV;
}
