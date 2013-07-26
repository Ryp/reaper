#version 420 core

layout(location = 0) in vec3 VertexPosition_modelspace;
layout(location = 1) in vec2 VertexUV_modelspace;
layout(location = 2) in vec3 VertexNormal_modelspace;

uniform mat4 MVP;
uniform mat4 MV;
uniform mat4 M;
uniform mat4 V;

uniform vec3 LighPosition_worldspace;

out vec2 UV;
out vec3 VextexNormal_cameraspace;
out vec3 EyeDirection_cameraspace;
out vec3 LightDirection_cameraspace;

void main()
{
  vec3 VextexPosition_cameraspace = vec3(MV * vec4(VertexPosition_modelspace, 1));

  vec3 LightPosition_cameraspace = vec3(V * vec4(LighPosition_worldspace, 1));
  LightDirection_cameraspace = LightPosition_cameraspace - VextexPosition_cameraspace;
  EyeDirection_cameraspace = - VextexPosition_cameraspace;

  gl_Position = MVP * vec4(VertexPosition_modelspace, 1);
  UV = VertexUV_modelspace;
  VextexNormal_cameraspace = vec3(MV * vec4(normalize(VertexNormal_modelspace), 0.5));
}
