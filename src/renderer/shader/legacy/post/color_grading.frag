#version 450 core

layout (binding = 0) uniform sampler2D image;
layout (binding = 1) uniform sampler3D lut;

uniform vec2 imageSize;
uniform float lutSize;

out vec4 fragColor;

vec3 color_grade(in vec3 colorInput, in sampler3D lut, in float lutResolution)
{
    vec3 index = colorInput + vec3(0.5 / lutResolution);
    index.b = 1.0;
    return texture(lut, index).rgb;
}

void main()
{
    vec2 UV = vec2(gl_FragCoord.x / imageSize.x, gl_FragCoord.y / imageSize.y);
    vec3 color = texture(image, UV).rgb;

    fragColor = vec4(color_grade(color, lut, lutSize), 1.0);
}
