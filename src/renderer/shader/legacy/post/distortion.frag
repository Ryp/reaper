#version 450 core

in vec2 UV;

out vec3 color;

uniform sampler2D renderedTexture;
uniform float time;

void main()
{
  vec2  texcoord = UV;
  float offset = sin(texcoord.y * 4 * 2 * 3.14159 + time) / 100;

  texcoord.x += offset;
  color = texture(renderedTexture, texcoord).xyz;
}
