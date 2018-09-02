#version 450 core

smooth in vec2 vertOutTexCoords;

uniform sampler2D textureUnit0;
uniform vec4 color;

out vec4 outBright;
out vec4 outColor;

void main(void)
{
    const float bloomLimit = 1.0;

    outColor = color * texture (textureUnit0, vertOutTexCoords);
    outColor.a = 1.0;

    vec3 brightColor = max (color.rgb - vec3 (bloomLimit), vec3 (0.0));
    float bright = dot (brightColor, vec3 (1.0));
    bright = smoothstep (0.0, 0.5, bright);

    outBright.rgb = mix (vec3 (0.0), color.rgb, bright).rgb;
    outBright.a = 1.0;
}
