#version 440 core

in vec3 fragmentColor;
in vec4 ShadowCoord;
in vec3 vertexNormal_cameraspace;
in vec3 lightDirection_cameraspace;
in vec3 eyeDirection_cameraspace;
in vec4 vertexPosition_clipspace;

out vec3 color;

uniform mat4 MVP;
uniform mat4 DepthBias;
uniform sampler2DShadow shadowMapTex;
uniform sampler2DShadow depthBufferTex;
// uniform sampler2DShadow noiseTex;

vec2 poissonDisk[16] = vec2[](
   vec2(-0.94201624, -0.39906216),
   vec2(0.94558609, -0.76890725),
   vec2(-0.094184101, -0.92938870),
   vec2(0.34495938, 0.29387760),
   vec2(-0.91588581, 0.45771432),
   vec2(-0.81544232, -0.87912464),
   vec2(-0.38277543, 0.27676845),
   vec2(0.97484398, 0.75648379),
   vec2(0.44323325, -0.97511554),
   vec2(0.53742981, -0.47373420),
   vec2(-0.26496911, -0.41893023),
   vec2(0.79197514, 0.19090188),
   vec2(-0.24188840, 0.99706507),
   vec2(-0.81409955, 0.91437590),
   vec2(0.19984126, 0.78641367),
   vec2(0.14383161, -0.14100790)
);

// Returns a random number based on a vec3 and an int.
float random(vec3 seed, int i){
    vec4    seed4 = vec4(seed, i);
    float   dot_product = dot(seed4, vec4(12.9898, 78.233, 45.164, 94.673));
    return fract(sin(dot_product) * 43758.5453);
}

float ssao(vec4 pixel_clipspace)
{
    vec4 uv = DepthBias * pixel_clipspace;
    float occlusion = 0;

    occlusion += textureProjOffset(depthBufferTex, uv, ivec2(1, 1));
    occlusion += textureProjOffset(depthBufferTex, uv, ivec2(1, -1));
    occlusion += textureProjOffset(depthBufferTex, uv, ivec2(-1, 1));
    occlusion += textureProjOffset(depthBufferTex, uv, ivec2(-1, -1));
    occlusion += textureProjOffset(depthBufferTex, uv, ivec2(2, 2));
    occlusion += textureProjOffset(depthBufferTex, uv, ivec2(2, -2));
    occlusion += textureProjOffset(depthBufferTex, uv, ivec2(-2, 2));
    occlusion += textureProjOffset(depthBufferTex, uv, ivec2(-2, -2));
    occlusion /= 8;
    return occlusion;
}

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

    //SSAO
    float ssao_level = ssao(vertexPosition_clipspace);

    // Sample neighbors
    float shadow = 0.0;
    shadow += textureProjOffset(shadowMapTex, ShadowCoord, ivec2(1, 1));
    shadow += textureProjOffset(shadowMapTex, ShadowCoord, ivec2(1, -1));
    shadow += textureProjOffset(shadowMapTex, ShadowCoord, ivec2(-1, 1));
    shadow += textureProjOffset(shadowMapTex, ShadowCoord, ivec2(-1, -1));
    shadow *= 0.25;

    shadow = shadow * 0.6 + 0.4; // Rescaling

    shadow *= ssao_level;

    color = MaterialAmbientColor +
            MaterialDiffuseColor * lightColor * shadow * lightPower * cosTheta +
            MaterialSpecularColor * lightColor * shadow * lightPower * pow(cosAlpha, 5);
}
