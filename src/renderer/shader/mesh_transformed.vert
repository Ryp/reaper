#version 450 core

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 viewProj;
} uniformBuffer;

layout(location = 0) in vec3 in_PositionMS;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    const vec4 positionWS = uniformBuffer.model * vec4(in_PositionMS, 1.0);
    const vec4 positionCS = uniformBuffer.viewProj * positionWS;

    gl_Position = positionCS;
}
