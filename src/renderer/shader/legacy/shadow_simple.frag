#version 450 core

in vec3 fragmentColor;
in vec4 ShadowCoord;
in vec3 vertexNormal_cameraspace;
in vec3 lightDirection_cameraspace;

out vec3 color;

uniform sampler2DShadow shadowMap;

void main()
{
    vec3    MaterialDiffuseColor = fragmentColor;
    vec3    n = normalize(vertexNormal_cameraspace);
    vec3    l = normalize(lightDirection_cameraspace);
    float   cosTheta = clamp(dot(n, l), 0, 1);

    float bias = 0.005 * tan(acos(cosTheta));
    bias = clamp(bias, 0.0, 0.01);

    float visibility = texture(shadowMap, vec3(ShadowCoord.xy, (ShadowCoord.z - bias) / ShadowCoord.w));

    color = visibility * MaterialDiffuseColor * cosTheta;
}
