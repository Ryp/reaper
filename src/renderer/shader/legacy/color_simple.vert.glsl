#version 450 core

in vec3 vertexPosition_modelspace;
in vec3 vertexNormal;

out vec3 fragmentColor;

uniform mat4 MVP;

void main()
{
  gl_Position = MVP * vec4(vertexPosition_modelspace, 1);

  fragmentColor.r = dot(MVP * normalize(vec4(vertexNormal, 1)), vec4(1, 0, 0, 1)) / 6;
  fragmentColor.g = dot(MVP * normalize(vec4(vertexNormal, 1)), vec4(0, -1, 0, 1)) / 10;
  fragmentColor.b = dot(MVP * normalize(vec4(vertexNormal, 1)), vec4(0, 0, 1, 1)) / 10;
  fragmentColor = vertexNormal;
}
