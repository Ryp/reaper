#version 450 core

layout (binding = 0) uniform sampler2D image;
layout (binding = 1) uniform sampler2D lut;

uniform vec2 imageSize;
uniform float lutSize;

out vec4 fragColor;

vec3 color_grade2(in vec3 colorInput, in sampler2D lut, in float lutResolution)
{
    colorInput = clamp(colorInput, 0.0, 1.0);
    float width = lutResolution;
    float sliceSize = 1.0 / width;              // space of 1 slice
    float slicePixelSize = sliceSize / width;           // space of 1 pixel
    float sliceInnerSize = slicePixelSize * (width - 1.0);  // space of width pixels
    float zSlice0 = min(floor(colorInput.z * width), width - 1.0);
    float zSlice1 = min(zSlice0 + 1.0, width - 1.0);
    float xOffset = slicePixelSize * 0.5 + colorInput.x * sliceInnerSize;
    float s0 = xOffset + (zSlice0 * sliceSize);
    float s1 = xOffset + (zSlice1 * sliceSize);
    vec3 slice0Color = texture2D(lut, vec2(s0, colorInput.y)).bgr;
    vec3 slice1Color = texture2D(lut, vec2(s1, colorInput.y)).bgr;
    float zOffset = mod(colorInput.z * width, 1.0);
    vec3 result = mix(slice0Color, slice1Color, zOffset);
    return result;
}

vec3 color_grade(in vec3 colorInput, in sampler2D lut, in float lutResolution)
{
    vec2 index;

    float blueColor = colorInput.b * 63;
    index.y = floor(floor(blueColor) / 8.0);
    index.x = floor(blueColor) - (index.y * 8.0);
    index += colorInput.rg + vec2(0.5 / lutResolution);
//     index += vec2(colorInput.b / lutResolution, fract(colorInput.b / lutResolution));
//     index += vec2(fract(colorInput.b / lutResolution), colorInput.b / lutResolution);
    return texture(lut, index).rgb;
}

void main()
{
    vec2 UV = vec2(gl_FragCoord.x / imageSize.x, gl_FragCoord.y / imageSize.y);
    vec3 color = texture(image, UV).rgb;

    fragColor = vec4(color_grade2(color, lut, lutSize), 1.0);
}
