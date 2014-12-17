#version 440 core

in vec3 vertexNormal_worldspace;

out vec3 color;

uniform float farPlane;
uniform float nearPlane;

void main()
{
    float linearDepth = 1.0 / gl_FragCoord.w;

    gl_FragDepth = (linearDepth - nearPlane) / (farPlane - nearPlane);
    color = normalize(vertexNormal_worldspace);
}
