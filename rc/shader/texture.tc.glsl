#version 420 core

layout (vertices = 3) out;

uniform int TessLevelInner;
uniform int TessLevelOuter;

void main()
{
  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

  gl_TessLevelOuter[0] = TessLevelOuter;
  gl_TessLevelOuter[1] = TessLevelOuter;
  gl_TessLevelOuter[2] = TessLevelOuter;
  gl_TessLevelInner[0] = TessLevelInner;
}
