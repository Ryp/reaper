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
struct VertexAttributes
{
    glm::fvec3 normal;
    u32        _pad0 = 0;
    glm::fvec2 uv;
    u32        _pad1 = 0;
    u32        _pad2 = 0;
    glm::fvec4 tangent; // GLTF uses w for the bitangent sign (-1.0 or 1.0)
};

struct Mesh
{
    std::vector<u32> indexes;

    // Vertex data
    std::vector<glm::fvec3>       positions;
    std::vector<VertexAttributes> attributes;
};

REAPER_MESH_API
Mesh duplicate_mesh(const Mesh& mesh);
} // namespace Reaper
