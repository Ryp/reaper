#version 420 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

uniform mat4 ViewportMatrix;

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
  noperspective vec3 EgdeDistance;
} vertexOut;

void main()
{
  vec3	p[3];
  float	alt[3];
  float	hg[3];

  for (int i = 0; i < 3; ++i)
    p[i] = vec3(ViewportMatrix * (gl_in[i].gl_Position / gl_in[i].gl_Position.w));

  for (int i = 0; i < 3; ++i)
    alt[i] = length(p[(i + 1) % 3] - p[(i + 2) % 3]);

  float alpha = acos((alt[1] * alt[1] + alt[2] * alt[2] - alt[0] * alt[0]) / (2.0 * alt[1] * alt[2]));
  float beta = acos((alt[0] * alt[0] + alt[2] * alt[2] - alt[1] * alt[1]) / (2.0 * alt[0] * alt[2]));

  hg[0] = abs(alt[2] * sin(beta));
  hg[1] = abs(alt[2] * sin(alpha));
  hg[2] = abs(alt[1] * sin(alpha));

  for (int i = 0; i < 3; ++i)
  {
    gl_Position = gl_in[i].gl_Position;
    vertexOut.UV = vertexIn[i].UV;
    vertexOut.VextexNormal_cameraspace = vertexIn[i].VextexNormal_cameraspace;
    vertexOut.EyeDirection_cameraspace = vertexIn[i].EyeDirection_cameraspace;
    vertexOut.LightDirection_cameraspace = vertexIn[i].LightDirection_cameraspace;
    vertexOut.EgdeDistance = vec3(0, 0, 0);
    vertexOut.EgdeDistance[i] = hg[i];
    EmitVertex();
  }
  EndPrimitive();
}
