#version 450 core

// Attributes per vertex: position, normal and texture coordinates
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec3 vertexNormal_modelspace;
layout(location = 2) in vec2 vertexUV;

uniform mat4 MVP;
uniform mat4 MV;
uniform vec3 lightPos;
uniform vec4 color;

smooth out vec4 vertOutFragColor;
smooth out vec2 vertOutTexCoords;

void main(void)
{
    mat3 normalMatrix;
    normalMatrix[0] = normalize (MV[0].xyz);
    normalMatrix[1] = normalize (MV[1].xyz);
    normalMatrix[2] = normalize (MV[2].xyz);

    vec3 N = normalize (normalMatrix * vertexNormal_modelspace);

    // Get vertex position in eye coordinates
    vec4 vertexPos = MV * vertexPosition_modelspace;
    vec3 vertexEyePos = vertexPos.xyz / vertexPos.w;

    // Get vector to light source
    vec3 L = normalize(lightPos - vertexEyePos);

    float dotProd = max (0.0, dot (N, L));

    vertOutFragColor.rgb = color.rgb * dotProd;
    vertOutFragColor.a = color.a;

    // Pass along the texture coordinates
    vertOutTexCoords = vertexUV;

    // Don't forget to transform the geometry!
    gl_Position = MVP * vertex;
}
