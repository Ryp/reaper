#version 450 core

layout (location = 0) in vec4 position;
layout (location = 1) in vec3 normal;

out vec3 n;

uniform mat4 MVP;

void main(void)
{
    n = normal;
    gl_Position = MVP * position;
}
