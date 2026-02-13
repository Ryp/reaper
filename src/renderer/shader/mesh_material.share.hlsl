////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2026 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef MESH_MATERIAL_SHARE_INCLUDED
#define MESH_MATERIAL_SHARE_INCLUDED

#include "shared_types.hlsl"

struct MeshMaterial
{
    hlsl_uint albedo_texture_index;
    hlsl_uint roughness_texture_index;
    hlsl_uint normal_texture_index;
    hlsl_uint ao_texture_index;
};

#endif
