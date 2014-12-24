#version 440 core

in vec3 vertexPosition_modelspace;
in vec3 vertexNormal_modelspace;

out vec3 vertexNormal_clipspace;

uniform mat4 MVP;

void main()
{
    vertexNormal_clipspace = (MVP * vec4(vertexNormal_modelspace, 1)).xyz;
    gl_Position = MVP * vec4(vertexPosition_modelspace, 1);
}
