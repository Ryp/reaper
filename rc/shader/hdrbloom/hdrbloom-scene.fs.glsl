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
    vec3    albedo;
    float   fresnel;
    float   roughness;
};

uniform const float pi = 3.14159;
uniform const float invPi = 1.0 / pi;

layout (binding = 1, std140) uniform MATERIAL_BLOCK
{
    material_t  material[32];
} materials;

// Cook-Torrance BRDF

// D() Normal Distribution Functions (NDF)
// GGX (Trowbridge-Reitz)
float D(float dotNH, float alpha)
{
    float alpha2 = alpha * alpha;
    float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (pi * denom * denom);
}

// F() Fresnel
// Schlick's approximation
float F(float dotVH, float f0)
{
    float dotVH2 = dotVH * dotVH;
    return f0 + (1.0 - f0) * (1.0 - dotVH2 * dotVH2 * dotVH);
}

// G() Geometric Shadowing / Visibility
// Smith-Schlick
float G1(float dotProd, float k)
{
    return 1.0 / (dotProd * (1.0 - k) + k);
}

void main(void)
{
    vec3 N = normalize(fs_in.N);
    vec3 L = normalize(fs_in.L);
    vec3 V = normalize(fs_in.V);
    vec3 H = normalize(V + L);
    
    float dotNL = max(dot(N, L), 0.0);
    float dotNH = max(dot(N, H), 0.0);
    float dotVH = max(dot(V, H), 0.0);
    float dotLH = max(dot(L, H), 0.0);
    
    material_t m = materials.material[fs_in.material_index];
    
    float alpha = m.roughness * m.roughness; // remap roughness
    
    // Lambertian Diffuse
    float diffuse = dotNL * invPi;
    
    // UE4 Specular
    float specular = dotNL * F(dotVH, m.fresnel) * D(dotNH, alpha) * G1(dotNL, alpha * 0.5) * G1(dotLH, alpha * 0.5);

    // FIXME
    // Hotness remap
    // /4 ?
    
    // Add ambient, diffuse and specular to find final color
    vec3 color = m.albedo * (diffuse + specular) * fs_in.intensity;
//     color = color * textureLod(envmap, fs_in.UV, 2.5).xyz;

    // Write final color to the framebuffer
    color0 = vec4(color, 1.0);

    // Calculate luminance
    float Y = dot(color, vec3(0.299, 0.587, 0.144));

    // Threshold color based on its luminance and write it to
    // the second output
    color = color * 4.0 * smoothstep(bloom_thresh_min, bloom_thresh_max, Y);
    color1 = vec4(color, 1.0);
}
