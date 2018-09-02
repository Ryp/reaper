#version 450 core

layout (location = 0) in vec4 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 vertexUV;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;

out VS_OUT
{
    vec3 N_world;
    vec3 N;
    vec3 L;
    vec3 V;
    vec2 UV;
    vec3 tangt;
    vec3 bitgt;
    flat int material_index;
    float intensity; // Does not need to be computed per-fragment
} vs_out;

// Position of light
uniform vec3    light_pos;
uniform float   light_power;

layout (binding = 0, std140) uniform TRANSFORM_BLOCK
{
    mat4    mat_proj;
    mat4    mat_view;
    mat4    mat_model[32];
} transforms;

void main(void)
{
    mat4    mat_mv = transforms.mat_view * transforms.mat_model[gl_InstanceID];

    // Calculate view-space coordinate
    vec4 P = mat_mv * position;

    vs_out.N_world = mat3(transforms.mat_model[gl_InstanceID]) * normal;

    // Calculate normal in view-space
    vs_out.N = mat3(mat_mv) * normal;
    vs_out.tangt = mat3(mat_mv) * tangent;
    vs_out.bitgt = mat3(mat_mv) * bitangent;

    // Calculate light vector
    vs_out.L = (transforms.mat_view * vec4(light_pos, 1.0)).xyz - P.xyz;

    float ld = length(vs_out.L);
    vs_out.intensity = light_power / (ld * ld);

    // Calculate view vector
    vs_out.V = -P.xyz;

    // Pass material index
    vs_out.material_index = gl_InstanceID;
    vs_out.UV = vertexUV;

    // Calculate the clip-space position of each vertex
    gl_Position = transforms.mat_proj * P;
}
