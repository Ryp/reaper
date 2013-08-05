#version 420 core

in vec2 v_coord;

out vec2 f_texcoord;

void main()
{
  gl_Position = vec4(v_coord, 0.0, 1.0);
  f_texcoord = (v_coord + 1.0) / 2.0;
}
