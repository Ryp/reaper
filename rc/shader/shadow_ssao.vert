#version 450 core

layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec3 vertexNormal_modelspace;
layout(location = 2) in vec2 vertexUV;

out vec2 UV;
out vec3 fragmentColor;
out vec4 ShadowCoord;
out vec3 vertexNormal_cameraspace;
out vec3 lightDirection_cameraspace;
out vec3 eyeDirection_cameraspace;
noperspective out vec3 eyeDirection_worldspace;
out vec4 vertexPosition_clipspace;

uniform mat4 V;
uniform mat4 M;
uniform mat4 MV;
uniform mat4 MVP;
uniform mat4 DepthBiasMVP;
uniform float tanHalfFov;
uniform float aspectRatio;

uniform vec3 lightInvDirection_worldspace;

void main()
{
    vertexPosition_clipspace = MVP * vec4(vertexPosition_modelspace, 1);
    gl_Position = vertexPosition_clipspace;

    // Same, but with the light's view matrix
    ShadowCoord = DepthBiasMVP * vec4(vertexPosition_modelspace, 1);

    lightDirection_cameraspace = (V * vec4(lightInvDirection_worldspace, 0)).xyz;
    eyeDirection_cameraspace = vec3(0, 0, 0) - (MV * vec4(vertexPosition_modelspace, 1)).xyz;

    eyeDirection_worldspace = vec3(vertexPosition_clipspace.y * tanHalfFov, vertexPosition_clipspace.x * tanHalfFov * aspectRatio, 1.0);

    vertexNormal_cameraspace = (MV * vec4(vertexNormal_modelspace, 0)).xyz;

    fragmentColor = normalize((M * vec4(vertexNormal_modelspace, 0))).xyz * 0.5 + 0.5;
    UV = vertexUV;
}
