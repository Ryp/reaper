#version 420 core

uniform sampler2D TexSampler;
uniform sampler2D TexSpecularSampler;
uniform sampler2D TexNormalSampler;

in GS_FS_VERTEX
{
  vec2 UV;
  vec3 VextexNormal_cameraspace;
  vec3 EyeDirection_cameraspace;
  vec3 LightDirection_cameraspace;
} vertexIn;

out vec3 color;

void main()
{
  vec3 MaterialDiffuseColor = texture2D(TexSampler, vertexIn.UV).rgb;
  vec3 MaterialSpecularColor = texture2D(TexSpecularSampler, vertexIn.UV).rgb;

  vec3 lightColor = vec3(1, 1, 1);
  vec3 ambientColor = vec3(1, 1, 1);

  // Ambient
  float ambientAmount = 0.05;

  // Diffuse
  float cosTheta = clamp(dot(vertexIn.VextexNormal_cameraspace, vertexIn.LightDirection_cameraspace), 0, 1);

  //Specular
  vec3 E = normalize(vertexIn.EyeDirection_cameraspace);
  vec3 R = reflect(normalize(vertexIn.LightDirection_cameraspace), normalize(vertexIn.VextexNormal_cameraspace));
  float cosAlpha = clamp(dot(E, R), 0, 1);

  color = MaterialSpecularColor * lightColor * pow(cosAlpha, 6)
	  + MaterialDiffuseColor * ambientColor * ambientAmount
	  + MaterialDiffuseColor * lightColor * cosTheta;
}
