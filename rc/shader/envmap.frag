#version 450 core

layout (location = 0) out vec4 color0;
layout (location = 1) out vec4 color1;

in vec2 UV;

uniform sampler2D envmap;

void main(void)
{
    color0 = texture(envmap, UV);
    color1 = vec4(0.0);
}
