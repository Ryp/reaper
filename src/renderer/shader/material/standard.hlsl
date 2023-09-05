////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef MATERIAL_STANDARD_INCLUDED
#define MATERIAL_STANDARD_INCLUDED

struct StandardMaterial
{
    float3 albedo;
    float3 normal_vs;
    float roughness;
    float f0;
    float ao;
};

#endif
