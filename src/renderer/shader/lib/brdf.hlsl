////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_BRDF_INCLUDED
#define LIB_BRDF_INCLUDED

// Quick and dirty copy/paste from this blog:
// http://filmicworlds.com/blog/optimizing-ggx-shaders-with-dotlh/
float G1V(float dotNV, float k)
{
    return 1.0f/(dotNV*(1.0f-k)+k);
}

float LightingFuncGGX_REF(float3 N, float3 V, float3 L, float roughness, float F0)
{
    float alpha = roughness*roughness;

    float3 H = normalize(V+L);

    float dotNL = saturate(dot(N,L));
    float dotNV = saturate(dot(N,V));
    float dotNH = saturate(dot(N,H));
    float dotLH = saturate(dot(L,H));

    float F, D, vis;

    // D
    float alphaSqr = alpha*alpha;
    float pi = 3.14159f;
    float denom = dotNH * dotNH *(alphaSqr-1.0) + 1.0f;
    D = alphaSqr/(pi * denom * denom);

    // F
    float dotLH5 = pow(1.0f-dotLH,5);
    F = F0 + (1.0-F0)*(dotLH5);

    // V
    float k = alpha/2.0f;
    vis = G1V(dotNL,k)*G1V(dotNV,k);

    float specular = dotNL * D * F * vis;
    return specular;
}

struct StandardMaterial
{
    float3 albedo;
    float roughness;
    float f0;
};

float3 specular_brdf(StandardMaterial material, float3 n, float3 v, float3 l)
{
    return LightingFuncGGX_REF(n, v, l, material.roughness, material.f0);
}

#endif
