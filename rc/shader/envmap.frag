#version 450 core

layout (location = 0) out vec4 color0;
layout (location = 1) out vec4 color1;

in vec3 n;

const float pi = 3.14159;
const float invPi = 1.0 / pi;

vec2 toSphericalProj(vec3 dir)
{
    return vec2(atan(dir.z, dir.x) * 0.5 * invPi + 0.5, acos(dir.y) * invPi);
}

uniform sampler2D envmap;

void main(void)
{
    color0 = texture(envmap, toSphericalProj(n));
    color1 = vec4(0.0);
}
