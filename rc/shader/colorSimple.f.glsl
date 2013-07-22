#version 420 core

in vec3 fragmentColor;

out vec3 color;

void main()
{
  color = fragmentColor;
//   color.x = round(fragmentColor.x * 4) / 4;
//   color.y = round(fragmentColor.y * 4) / 4;
//   color.z = round(fragmentColor.z * 4) / 4;
}
