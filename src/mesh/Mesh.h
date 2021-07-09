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
    std::vector<glm::fvec3> vertices;
    std::vector<glm::fvec3> normals;
    std::vector<glm::fvec2> uvs;
    std::vector<u32>        indexes;
    std::vector<glm::fvec3> tangents;
    std::vector<glm::fvec3> bitangents;

    bool isIndexed;
    bool hasUVs;
    bool hasNormals;
};

u32 getTriangleCount(const Mesh& mesh);

void computeNormalsSimple(Mesh& mesh);
