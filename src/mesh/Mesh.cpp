////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Mesh.h"

u32 getTriangleCount(const Mesh& mesh)
{
    if (mesh.isIndexed)
        return static_cast<u32>(mesh.indexes.size());
    else
        return static_cast<u32>(mesh.vertices.size()) / 3;
}

void computeNormalsSimple(Mesh& mesh)
{
    std::size_t trianglesNo = getTriangleCount(mesh);

    mesh.normals.resize(mesh.vertices.size());
    for (std::size_t i = 0; i < trianglesNo; ++i)
    {
        glm::vec3 a = mesh.vertices[i * 3 + 1] - mesh.vertices[i * 3 + 0];
        glm::vec3 b = mesh.vertices[i * 3 + 2] - mesh.vertices[i * 3 + 0];
        glm::vec3 n = glm::normalize(glm::cross(a, b));

        mesh.normals[i * 3 + 0] = n;
        mesh.normals[i * 3 + 1] = n;
        mesh.normals[i * 3 + 2] = n;
    }
}
