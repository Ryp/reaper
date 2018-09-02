#version 450 core

in vec2 v_coord;

out vec2 UV;

void main()
{
  gl_Position = vec4(v_coord, 0.0, 1.0);
  UV = (v_coord + vec2(1.0, 1.0)) / 2.0;
}
