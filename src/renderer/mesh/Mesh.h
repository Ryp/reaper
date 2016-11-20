////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_MESH_INCLUDED
#define REAPER_MESH_INCLUDED

#include <vector>

#include <glm/glm.hpp>

struct Mesh {
    std::vector<glm::vec3>    vertices;
    std::vector<glm::vec3>    normals;
    std::vector<glm::vec2>    uvs;
    std::vector<unsigned int> indexes;
    std::vector<glm::vec3>    tangents;
    std::vector<glm::vec3>    bitangents;

    bool    isIndexed;
    bool    hasUVs;
    bool    hasNormals;
};

std::size_t getTriangleCount(Mesh& mesh);
void computeNormalsSimple(Mesh& mesh);

#endif // REAPER_MESH_INCLUDED
