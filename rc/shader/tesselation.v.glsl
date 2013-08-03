#version 420 core

layout(location = 0) in vec3 VertexPosition_modelspace;

out vec3 vPosition;

void main()
{
  vPosition = VertexPosition_modelspace;
}
