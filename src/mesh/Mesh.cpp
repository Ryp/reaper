////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Mesh.h"

namespace
{
u32 getTriangleCount(const Mesh& mesh)
{
    if (!mesh.indexes.empty())
        return static_cast<u32>(mesh.indexes.size());
    else
        return static_cast<u32>(mesh.positions.size()) / 3;
}
} // namespace

void computeNormalsSimple(Mesh& mesh)
{
    std::size_t trianglesNo = getTriangleCount(mesh);

    mesh.normals.resize(mesh.positions.size());
    for (std::size_t i = 0; i < trianglesNo; ++i)
    {
        u32 index0 = i * 3 + 0;
        u32 index1 = i * 3 + 1;
        u32 index2 = i * 3 + 2;

        glm::vec3 a = mesh.positions[index1] - mesh.positions[index0];
        glm::vec3 b = mesh.positions[index2] - mesh.positions[index0];
        glm::vec3 n = glm::normalize(glm::cross(a, b));

        mesh.normals[index0] = n;
        mesh.normals[index1] = n;
        mesh.normals[index2] = n;
    }
}
