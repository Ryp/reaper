////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ModelLoader.h"

#include <core/Assert.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>

#include "tiny_obj_loader.h"

#include <glm/geometric.hpp>

namespace Reaper
{
namespace
{
    // A lot of cache miss with this method.
    Mesh load_obj_tiny_obj_loader(std::ifstream& src)
    {
        tinyobj::attrib_t                attrib;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials;
        std::string                      warn;
        std::string                      err;
        const bool                       triangulate = true;

        const bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &src, nullptr, triangulate);

        Assert(err.empty(), err);
        Assert(ret, "could not load obj");
        Assert(shapes.size() > 0, "no shapes to load");

        // FIXME reserve() not at the right size
        Mesh mesh;
        mesh.positions.reserve(attrib.vertices.size());
        mesh.normals.reserve(attrib.normals.size());
        mesh.tangents.reserve(attrib.normals.size());
        mesh.uvs.reserve(attrib.texcoords.size());

        // Loop over shapes
        for (size_t s = 0; s < shapes.size(); s++)
        {
            // Loop over faces(polygon)
            size_t index_offset = 0;
            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
            {
                u32 fv = shapes[s].mesh.num_face_vertices[f];
                Assert(fv == 3, "only triangle faces are supported");

                // Loop over vertices in the face.
                for (size_t v = 0; v < fv; v++)
                {
                    // access to vertex
                    const u32              index = static_cast<u32>(index_offset + v);
                    const tinyobj::index_t indexInfo = shapes[s].mesh.indices[index];

                    tinyobj::real_t  vx = attrib.vertices[3 * indexInfo.vertex_index + 0];
                    tinyobj::real_t  vy = attrib.vertices[3 * indexInfo.vertex_index + 1];
                    tinyobj::real_t  vz = attrib.vertices[3 * indexInfo.vertex_index + 2];
                    const glm::fvec3 vertex(vx, vy, vz);
                    mesh.positions.push_back(vertex);

                    if (!attrib.normals.empty())
                    {
                        tinyobj::real_t  nx = attrib.normals[3 * indexInfo.normal_index + 0];
                        tinyobj::real_t  ny = attrib.normals[3 * indexInfo.normal_index + 1];
                        tinyobj::real_t  nz = attrib.normals[3 * indexInfo.normal_index + 2];
                        const glm::fvec3 normal(nx, ny, nz);
                        mesh.normals.push_back(normal);
                        mesh.tangents.push_back(glm::fvec4(1.0, 0.0, 0.0, 1.0));
                    }

                    if (!attrib.texcoords.empty())
                    {
                        tinyobj::real_t  tx = attrib.texcoords[2 * indexInfo.texcoord_index + 0];
                        tinyobj::real_t  ty = attrib.texcoords[2 * indexInfo.texcoord_index + 1];
                        const glm::fvec2 uv(tx, ty);
                        mesh.uvs.push_back(uv);
                    }

                    mesh.indexes.push_back(index);
                }
                index_offset += fv;
            }
        }
        return mesh;
    }
} // namespace

Mesh load_obj(const std::string& filename)
{
    std::ifstream file(filename);
    return load_obj_tiny_obj_loader(file);
}

void save_obj(std::ostream& output, std::span<const Mesh> meshes)
{
    u32 vertexOffset = 0;
    u32 uvOffset = 0;
    u32 normalOffset = 0;

    for (u32 meshIndex = 0; meshIndex < meshes.size(); meshIndex++)
    {
        const Mesh& mesh = meshes[meshIndex];
        const u32   vertexCount = static_cast<u32>(mesh.positions.size());
        const u32   uvCount = static_cast<u32>(mesh.uvs.size());
        const u32   normalCount = static_cast<u32>(mesh.normals.size());
        const u32   indexCount = static_cast<u32>(mesh.indexes.size());

        Assert(indexCount % 3 == 0);

        for (u32 i = 0; i < vertexCount; i++)
        {
            output << 'v';
            for (u32 j = 0; j < 3; ++j)
                output << ' ' << mesh.positions[i][j];
            output << std::endl;
        }

        for (u32 i = 0; i < uvCount; i++)
        {
            output << "vt";
            for (u32 j = 0; j < 2; ++j)
                output << ' ' << mesh.uvs[i][j];
            output << std::endl;
        }

        for (u32 i = 0; i < normalCount; i++)
        {
            output << "vn";
            for (u32 j = 0; j < 3; ++j)
                output << ' ' << mesh.normals[i][j];
            output << std::endl;
        }

        for (u32 i = 0; i < (indexCount / 3); i++)
        {
            output << 'f';
            output << ' ' << mesh.indexes[i * 3 + 0] + 1 + vertexOffset;
            output << '/' << mesh.indexes[i * 3 + 0] + 1 + uvOffset;
            output << '/' << mesh.indexes[i * 3 + 0] + 1 + normalOffset;
            output << ' ' << mesh.indexes[i * 3 + 1] + 1 + vertexOffset;
            output << '/' << mesh.indexes[i * 3 + 1] + 1 + uvOffset;
            output << '/' << mesh.indexes[i * 3 + 1] + 1 + normalOffset;
            output << ' ' << mesh.indexes[i * 3 + 2] + 1 + vertexOffset;
            output << '/' << mesh.indexes[i * 3 + 2] + 1 + uvOffset;
            output << '/' << mesh.indexes[i * 3 + 2] + 1 + normalOffset;
            output << std::endl;
        }

        vertexOffset += vertexCount;
        uvOffset += uvCount;
        normalOffset += normalCount;
    }
}
} // namespace Reaper
