#version 420 core

in vec2 UV;
in vec3 VextexNormal;

out vec3 color;

uniform sampler2D TexSampler;
uniform sampler2D TexSpecularSampler;

void main()
{
  vec3 MaterialDiffuseColor = texture2D(TexSampler, UV).rgb;
  vec3 MaterialSpecularColor = texture2D(TexSpecularSampler, UV).rgb;

  vec3 lightColor = vec3(1, 1, 1);
  vec3 ambientColor = vec3(1, 1, 1);
  
  float ambientAmount = 0.05;
  float cosTheta = clamp(dot(VextexNormal, vec3(-1, -0.5, 0)), 0, 1);

  color = MaterialSpecularColor * (0.05 * cosTheta / 5)
	  + MaterialDiffuseColor * ambientColor * ambientAmount
	  + MaterialDiffuseColor * lightColor * cosTheta;
}
