#version 450 core

layout(location = 0) in vec3 in_PositionMS;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(in_PositionMS, 1.0);
}
