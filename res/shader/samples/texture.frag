#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 uv;
layout (location = 1) in float scale;

layout (location = 0) out vec4 outputColor;

layout (binding = 1) uniform sampler2D samplerColor;

void main()
{
    float t = abs(scale - 0.5) * 2;
    vec2 uvAnim = uv * vec2(cos(t * 42 * uv.y), sin(t * 42 * uv.x));

    outputColor = texture(samplerColor, uvAnim);
}
