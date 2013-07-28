#version 420 core

uniform sampler2D TexSampler;
uniform sampler2D TexSpecularSampler;

uniform vec3 WireframeColor;
uniform int WireframeThickness;

in GS_FS_VERTEX
{
  vec2 UV;
  vec3 VextexNormal_cameraspace;
  vec3 EyeDirection_cameraspace;
  vec3 LightDirection_cameraspace;
  noperspective vec3 EgdeDistance;
} vertexIn;

out vec3 color;

void main()
{
  vec3 MaterialDiffuseColor = texture2D(TexSampler, vertexIn.UV).rgb;
  vec3 MaterialSpecularColor = texture2D(TexSpecularSampler, vertexIn.UV).rgb;

  vec3 lightColor = vec3(1, 1, 1);

  // Ambient
  vec3 ambientColor = vec3(1, 1, 1);
  float ambientAmount = 0.05;

  // Diffuse
  float cosTheta = clamp(dot(vertexIn.VextexNormal_cameraspace, vertexIn.LightDirection_cameraspace), 0, 1);

  //Specular
  vec3 E = normalize(vertexIn.EyeDirection_cameraspace);
  vec3 R = reflect(-normalize(vertexIn.LightDirection_cameraspace), normalize(vertexIn.VextexNormal_cameraspace));
  float cosAlpha = clamp(dot(E, R), 0, 1);

  color = MaterialSpecularColor * lightColor * pow(cosAlpha, 6)
	  + MaterialDiffuseColor * ambientColor * ambientAmount
	  + MaterialDiffuseColor * lightColor * cosTheta;

  //Wireframe color test
  float d = min(min(vertexIn.EgdeDistance.x, vertexIn.EgdeDistance.y), vertexIn.EgdeDistance.z);
  float mixVal = smoothstep(WireframeThickness - 1, WireframeThickness + 1, d);
  color = mix(WireframeColor, color, mixVal);
}
