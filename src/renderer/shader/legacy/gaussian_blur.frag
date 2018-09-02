#version 450 core

layout (binding = 0) uniform sampler2D image;

out vec4 fragColor;

uniform vec2 imageSize;

uniform float offset[3] = float[](0.0, 1.3846153846, 3.2307692308);
uniform float weight[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

subroutine vec4 blurDirection(vec2);
subroutine uniform blurDirection blurSelector;

subroutine (blurDirection)
vec4 blurH(vec2 invScreenSize)
{
//     vec4 color = texture(image, vec2(gl_FragCoord) * invScreenSize);
    vec4 color = texture(image, vec2(gl_FragCoord) * invScreenSize) * weight[0];
    for (int i = 1; i < 3; i++)
    {
        color += texture(image, (vec2(gl_FragCoord) + vec2(offset[i], 0.0)) * invScreenSize) * weight[i];
        color += texture(image, (vec2(gl_FragCoord) - vec2(offset[i], 0.0)) * invScreenSize) * weight[i];
    }
    return color;
}

subroutine (blurDirection)
vec4 blurV(vec2 invScreenSize)
{
    vec4 color = texture(image, vec2(gl_FragCoord) * invScreenSize) * weight[0];
    for (int i = 1; i < 3; i++)
    {
        color += texture(image, (vec2(gl_FragCoord) + vec2(0.0, offset[i])) * invScreenSize) * weight[i];
        color += texture(image, (vec2(gl_FragCoord) - vec2(0.0, offset[i])) * invScreenSize) * weight[i];
    }
    return color;
}

void main(void)
{
    fragColor = blurSelector(vec2(1.0 / imageSize.x, 1.0 / imageSize.y));
}
