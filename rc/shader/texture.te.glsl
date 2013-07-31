#version 420 core

layout (triangles) in;

void main()
{
  gl_Position = vec4(gl_in[0].gl_Position.xyz * gl_TessCoord.u +
		     gl_in[1].gl_Position.xyz * gl_TessCoord.v +
		     gl_in[2].gl_Position.xyz * gl_TessCoord.w + 1.0);
}
