#version 430

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 inUv;

layout (binding = 0) uniform UBO
{
    float time;
    float scaleFactor;
} ubo;

layout (push_constant) uniform PushConsts
{
    float scale;
} pushConsts;

layout (location = 0) out vec2 outUv;
layout (location = 1) out float scale;

out gl_PerVertex
{
  vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(pos * ubo.scaleFactor, 1);
    outUv = inUv;
    scale = ubo.scaleFactor;
}
