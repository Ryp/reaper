#version 420 core

in vec2 UV;
in vec3 VextexNormal_cameraspace;
in vec3 EyeDirection_cameraspace;
in vec3 LightDirection_cameraspace;

out vec3 color;

uniform sampler2D TexSampler;
uniform sampler2D TexSpecularSampler;
uniform sampler2D TexNormalSampler;

void main()
{
  vec3 MaterialDiffuseColor = texture2D(TexSampler, UV).rgb;
  vec3 MaterialSpecularColor = texture2D(TexSpecularSampler, UV).rgb;

  vec3 lightColor = vec3(1, 1, 1);
  vec3 ambientColor = vec3(1, 1, 1);

  // Ambient
  float ambientAmount = 0.05;

  // Diffuse
  float cosTheta = clamp(dot(VextexNormal_cameraspace, LightDirection_cameraspace), 0, 1);

  //Specular
  vec3 E = normalize(EyeDirection_cameraspace);
  vec3 R = reflect(normalize(LightDirection_cameraspace), normalize(VextexNormal_cameraspace));
  float cosAlpha = clamp(dot(E, R), 0, 1);

  color = MaterialSpecularColor * lightColor * pow(cosAlpha, 6)
	  + MaterialDiffuseColor * ambientColor * ambientAmount
	  + MaterialDiffuseColor * lightColor * cosTheta;
}
