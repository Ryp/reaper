#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outputColor;

layout (binding = 1) uniform sampler2D samplerColor;

void main()
{
    outputColor = texture(samplerColor, uv);
}

