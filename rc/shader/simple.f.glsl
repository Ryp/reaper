#version 330 core

// in vec3 fragmentColor;
//
// layout(location = 0) out vec3 color;
//
// void main()
// {
//   color = fragmentColor;
// }

// Ouput data
layout (location = 0) out vec4 color;

void main()
{
  color = vec4(1, 0, 0, 1);
}
