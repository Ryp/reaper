#version 420 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in VS_GS_VERTEX
{
  vec2 UV;
  vec3 VextexNormal_cameraspace;
  vec3 EyeDirection_cameraspace;
  vec3 LightDirection_cameraspace;
} vertexIn[];

out GS_FS_VERTEX
{
  vec2 UV;
  vec3 VextexNormal_cameraspace;
  vec3 EyeDirection_cameraspace;
  vec3 LightDirection_cameraspace;
} vertexOut;

void main()
{
  for (int i = 0; i < gl_in.length(); i++)
  {
    gl_Position = gl_in[i].gl_Position;
    vertexOut.UV = vertexIn[i].UV;
    vertexOut.VextexNormal_cameraspace = vertexIn[i].VextexNormal_cameraspace;
    vertexOut.EyeDirection_cameraspace = vertexIn[i].EyeDirection_cameraspace;
    vertexOut.LightDirection_cameraspace = vertexIn[i].LightDirection_cameraspace;
    EmitVertex();
  }
  EndPrimitive();
}
