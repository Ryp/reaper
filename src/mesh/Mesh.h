////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MeshExport.h"

#include <core/Types.h>
#include <vector>

#include <glm/glm.hpp>

namespace Reaper
{
struct Mesh
{
    std::vector<u32> indexes;

    // Vertex data
    std::vector<glm::fvec3> positions;
    std::vector<glm::fvec3> normals;
    std::vector<glm::fvec4> tangents; // GLTF uses w for the bitangent sign (-1.0 or 1.0)
    std::vector<glm::fvec2> uvs;
};

REAPER_MESH_API
Mesh duplicate_mesh(const Mesh& mesh);
} // namespace Reaper
