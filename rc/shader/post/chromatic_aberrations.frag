#version 450 core

layout (binding = 0) uniform sampler2D image;

out vec4 fragColor;

uniform float amount;
uniform vec2 imageSize;

void main()
{
    vec2 UV = vec2(gl_FragCoord.x / imageSize.x, gl_FragCoord.y / imageSize.y);
    vec2 UVr_shift = vec2(0.0007, 0.0007);
    vec2 UVg_shift = vec2(-0.001, 0.000);
    vec2 UVb_shift = vec2(0.0007, -0.0007);
    vec4 color = vec4(1.0);

    color.r = texture(image, UV + UVr_shift * amount).r;
    color.g = texture(image, UV + UVg_shift * amount).g;
    color.b = texture(image, UV + UVb_shift * amount).b;
    fragColor = color;
}
