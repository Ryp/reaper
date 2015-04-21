#version 450 core

layout (location = 0) in vec4 position;
layout (location = 2) in vec2 vertexUV;

out vec2 UV;

uniform mat4 MVP;

void main(void)
{
    UV = vertexUV;
    gl_Position = MVP * position;
}
