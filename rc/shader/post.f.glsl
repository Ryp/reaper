#version 420 core

uniform sampler2D renderedTexture;
uniform float offset;

in vec2 f_texcoord;

out vec3 color;

void main()
{
  color = texture(renderedTexture, f_texcoord).xyz;
}
