////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Mesh.h"

unsigned int getTriangleCount(Mesh& mesh)
{
    if (mesh.isIndexed)
        return mesh.indexes.size();
    else
        return mesh.vertices.size() / 3;
}

void computeNormalsSimple(Mesh& mesh)
{
    unsigned int trianglesNo = getTriangleCount(mesh);

    mesh.normals.resize(mesh.vertices.size());
    for (unsigned int i = 0; i < trianglesNo; ++i)
    {
        glm::vec3 a = mesh.vertices[i * 3 + 1] - mesh.vertices[i * 3 + 0];
        glm::vec3 b = mesh.vertices[i * 3 + 2] - mesh.vertices[i * 3 + 0];
        glm::vec3 n = glm::normalize(glm::cross(a, b));

        mesh.normals[i * 3 + 0] = n;
        mesh.normals[i * 3 + 1] = n;
        mesh.normals[i * 3 + 2] = n;
    }
}
