#version 450 core

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 viewProj;
    float timeMs;
} uniformBuffer;

layout(location = 0) in vec3 in_PositionMS;

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
}
