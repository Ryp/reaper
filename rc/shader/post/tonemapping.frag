#version 450 core

in vec2 UV;

out vec3 color;

uniform sampler2D renderedTexture;

subroutine vec3 tonemapOperator(vec3);
subroutine uniform tonemapOperator tonemapSelector;

subroutine (tonemapOperator)
vec3 reinhardtonemap(vec3 x)
{
    return x / (x + 1.0);
}

subroutine (tonemapOperator)
vec3 notonemap(vec3 x)
{
    return x;
}

subroutine (tonemapOperator)
vec3 uncharted2tonemap(vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

void main()
{
    color = tonemapSelector(texture(renderedTexture, UV).xyz);
}
