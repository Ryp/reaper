#version 420 core

layout(location = 0) in vec3 VertexPosition_modelspace;
layout(location = 1) in vec2 VertexUV_modelspace;
layout(location = 2) in vec3 VertexNormal_modelspace;

uniform mat4 MVP;
uniform mat4 MV;
uniform mat4 V;

uniform vec3 LighPosition_worldspace;

out VS_GS_VERTEX
{
  vec2 UV;
  vec3 VextexNormal_cameraspace;
  vec3 EyeDirection_cameraspace;
  vec3 LightDirection_cameraspace;
} vertexOut;

void main()
{
  vec3 VextexPosition_cameraspace = (MV * vec4(VertexPosition_modelspace, 1)).xyz;

  vec3 LightPosition_cameraspace = (V * vec4(LighPosition_worldspace, 1)).xyz;
  vertexOut.LightDirection_cameraspace = LightPosition_cameraspace - VextexPosition_cameraspace;
  vertexOut.EyeDirection_cameraspace = - VextexPosition_cameraspace;

  gl_Position = MVP * vec4(VertexPosition_modelspace, 1);
  vertexOut.UV = VertexUV_modelspace;
  vertexOut.VextexNormal_cameraspace = (MV * vec4(normalize(VertexNormal_modelspace), 0)).xyz;
}
