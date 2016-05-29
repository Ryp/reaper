#version 450 core

in vec2 UV;
in vec3 fragmentColor;
in vec4 ShadowCoord;
in vec3 vertexNormal_cameraspace;
in vec3 lightDirection_cameraspace;
in vec3 eyeDirection_cameraspace;
noperspective in vec3 eyeDirection_worldspace;
in vec4 vertexPosition_clipspace;

out vec3 color;

uniform mat4 MVP;
uniform mat4 P;
uniform mat4 DepthBias;
layout (binding = 0) uniform sampler2DShadow shadowMapTex;
layout (binding = 1) uniform sampler2DShadow depthBufferTex;
layout (binding = 2) uniform sampler2D       materialTex;
layout (binding = 3) uniform sampler2D       noiseTex;

// SSAO
const int MAX_KERNEL_SIZE = 128;
uniform int kernelSize;
uniform vec3 kernelOffsets[MAX_KERNEL_SIZE];
uniform float radius;
uniform float power;
uniform vec2 frameBufSize;

// For PCF shadows
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
   vec2(0.53742981, -0.47373450),
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

float linearizeDepth(in float depth, in mat4 projMatrix) {
//  return projMatrix[3][2] / (depth - projMatrix[2][2]);
    return projMatrix[2][3] / (depth - projMatrix[2][2]);
}

// float ssao_ext(in mat3 kernelBasis, in vec3 originPos) {
//     float occlusion = 0.0;
//     for (int i = 0; i < kernelSize; ++i) {
//     // get sample position:
//         vec3 samplePos = kernelBasis * kernelOffsets[i];
//         samplePos = samplePos * radius + originPos;
//
//     // project sample position:
//         vec4 offset = P * vec4(samplePos, 1.0);
//         offset.xy /= offset.w; // only need xy
//         offset.xy = offset.xy * 0.5 + 0.5; // scale/bias to texcoords
//
//     // get sample depth:
//         float sampleDepth = texture(depthBufferTex, offset.xy).r;
//         sampleDepth = linearizeDepth(sampleDepth, P);
//
//         float rangeCheck = smoothstep(0.0, 1.0, radius / abs(originPos.z - sampleDepth));
//         occlusion += rangeCheck * step(sampleDepth, samplePos.z);
//     }
//
//     occlusion = 1.0 - (occlusion / float(kernelSize));
//     return pow(occlusion, power);
// }

float ssao(in vec4 pixel_clipspace)
{
    float occlusion = 0;
    vec4    depthCoord = DepthBias * pixel_clipspace;

    for (int i = 0; i < kernelSize; ++i)
        occlusion += textureProj(depthBufferTex, depthCoord + MVP * vec4(kernelOffsets[i] * 6.0, 1.0));
    occlusion /= float(kernelSize);
    return occlusion;
}

void main()
{
    // Light emission properties
    vec3    lightColor = vec3(1,1,0);
    float   lightPower = 1;
    vec3    MaterialDiffuseColor = texture2D(materialTex, UV).rgb + fragmentColor * 0.01;
    vec3    MaterialAmbientColor = texture2D(materialTex, UV).rgb * vec3(0.2, 0.2, 0.2);
    vec3    MaterialSpecularColor = vec3(1.0, 1.0, 1.0);
    vec3    n = normalize(vertexNormal_cameraspace);
    vec3    l = normalize(lightDirection_cameraspace);
    float   cosTheta = clamp(dot(n, l), 0, 1);
    vec3    e = normalize(eyeDirection_cameraspace);
    vec3    r = reflect(-l, n);
    float   cosAlpha = clamp(dot(e, r), 0, 1);

    //SSAO
    float ssao_level = ssao(vertexPosition_clipspace);
//     float ssao_level = 1.0;

    ///////////////////////////////////////////////////////////////////
    vec2 UVscreen = vec2(gl_FragCoord.x / frameBufSize.x, gl_FragCoord.y / frameBufSize.y);
    vec2 noiseTexCoords = vec2(textureSize(depthBufferTex, 0)) / vec2(textureSize(noiseTex, 0));
    noiseTexCoords *= UVscreen;

    // get view space origin:
//     float originDepth = texture(depthBufferTex, UVscreen).r;
    float originDepth = 1.0;
    originDepth = linearizeDepth(originDepth, P);
    vec3 originPos = eyeDirection_worldspace * originDepth;

    // construct kernel basis matrix:
    vec3 rvec = normalize(texture(noiseTex, noiseTexCoords).rgb * 2.0 - 1.0);
//     vec3 rvec = normalize(vec3(1.0) * 2.0 - 1.0);
    vec3 tangent = normalize(rvec - n * dot(rvec, n));
    vec3 bitangent = cross(tangent, n);
//     mat3 kernelBasis = mat3(tangent, bitangent, n);
    mat3 kernelBasis = mat3(tangent, bitangent, -n);
    /////////////////////////////////////////////////////////////////////
//     ssao_level *= ssao_ext(kernelBasis, originPos);

    // Sample neighbors
    float shadow = 0.0;
    shadow += textureProjOffset(shadowMapTex, ShadowCoord, ivec2(1, 1));
    shadow += textureProjOffset(shadowMapTex, ShadowCoord, ivec2(1, -1));
    shadow += textureProjOffset(shadowMapTex, ShadowCoord, ivec2(-1, 1));
    shadow += textureProjOffset(shadowMapTex, ShadowCoord, ivec2(-1, -1));
    shadow *= 0.25;

    shadow = shadow * 0.8 + 0.2; // Rescaling

//     shadow *= ssao_level;

    color = MaterialAmbientColor +
            MaterialDiffuseColor * lightColor * shadow * lightPower * cosTheta +
            MaterialSpecularColor * lightColor * shadow * lightPower * pow(cosAlpha, 5);
//     color = vec3(ssao_level, ssao_level, ssao_level);
// //     color = vec3(n) * 0.5 + 0.5;
}
