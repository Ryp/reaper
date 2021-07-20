////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include <glm/glm.hpp>

struct Mesh
{
    std::vector<u32> indexes;

    // Vertex data
    std::vector<glm::fvec3> positions;
    std::vector<glm::fvec3> normals;
    std::vector<glm::fvec2> uvs;
};

void computeNormalsSimple(Mesh& mesh);
