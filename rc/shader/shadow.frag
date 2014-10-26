#version 420 core

in vec3 fragmentColor;
in vec4 ShadowCoord;
in vec3 vertexNormal_cameraspace;
in vec3 lightDirection_cameraspace;
in vec3 eyeDirection_cameraspace;

out vec3 color;

uniform sampler2DShadow shadowMap;

void main()
{
    // Light emission properties
    vec3    lightColor = vec3(1,1,1);
    float   lightPower = 1.0;
    vec3    MaterialDiffuseColor = fragmentColor;
    vec3    MaterialAmbientColor = fragmentColor * vec3(0.2, 0.2, 0.2);
    vec3    MaterialSpecularColor = vec3(1.0, 1.0, 1.0);
    vec3    n = normalize(vertexNormal_cameraspace);
    vec3    l = normalize(lightDirection_cameraspace);
    float   cosTheta = clamp(dot(n, l), 0, 1);
    vec3    e = normalize(eyeDirection_cameraspace);
    vec3    r = reflect(-l, n);
    float   cosAlpha = clamp(dot(e, r), 0, 1);

    float bias = 0.005 * tan(acos(cosTheta));
    bias = clamp(bias, 0.0, 0.01);

    float visibility = texture(shadowMap, vec3(ShadowCoord.xy, (ShadowCoord.z - bias) / ShadowCoord.w));

    color = MaterialAmbientColor +
            MaterialDiffuseColor * lightColor * visibility * lightPower * cosTheta +
            MaterialSpecularColor * lightColor * visibility * lightPower * pow(cosAlpha, 5);
}
