#version 450 core

layout (location = 0) out vec4 color0;
layout (location = 1) out vec4 color1;

in VS_OUT
{
    vec3 N;
    vec3 L;
    vec3 V;
    vec2 UV;
    flat int material_index;
    float intensity;
} fs_in;

// Material properties
uniform float bloom_thresh_min;
uniform float bloom_thresh_max;

uniform sampler2D envmap;

struct material_t
{
    vec3    diffuse_color;
    vec3    specular_color;
    float   specular_power;
    vec3    ambient_color;
    vec3    albedo;
    float   fresnel;
    float   roughness;
};

layout (binding = 1, std140) uniform MATERIAL_BLOCK
{
    material_t  material[32];
} materials;

void main(void)
{
    // Normalize the incoming N, L and V vectors
    vec3 N = normalize(fs_in.N);
    vec3 L = normalize(fs_in.L);
    vec3 V = normalize(fs_in.V);
    vec3 H = (N + L) * 0.5;

    // Calculate R locally
    vec3 R = reflect(-L, N);

    material_t m = materials.material[fs_in.material_index];

    float cosThetaS = dot(H, V);
    float cosThetaS2 = cosThetaS * cosThetaS;
    float cosThetaS5 = cosThetaS2 * cosThetaS2 * cosThetaS;    
    
    float schlick = m.fresnel + (1.0 - m.fresnel) * (1 - cosThetaS5);
    // Compute the diffuse and specular components for each fragment
    vec3 diffuse = max(dot(N, L), 0.0) * m.albedo * fs_in.intensity;
    
//     vec3 specular = pow(max(dot(R, V), 0.0), m.specular_power) * m.specular_color;
    vec3 specular = max(dot(R, V), 0.0) * m.specular_color * schlick * fs_in.intensity;
    vec3 ambient = m.ambient_color;

    // Add ambient, diffuse and specular to find final color
    vec3 color = ambient + diffuse + specular;

    // Write final color to the framebuffer
    color0 = vec4(color, 1.0) * texture(envmap, fs_in.UV);
//     color0 = vec4(color, 1.0) * textureLod(envmap, fs_in.UV, 2.5);

    // Calculate luminance
    float Y = dot(color, vec3(0.299, 0.587, 0.144));

    // Threshold color based on its luminance and write it to
    // the second output
    color = color * 4.0 * smoothstep(bloom_thresh_min, bloom_thresh_max, Y);
    color1 = vec4(color, 1.0);
}
