////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ModelLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string.h>
#include <vector>

#include "tiny_obj_loader.h"

#include <glm/geometric.hpp>

ModelLoader::ModelLoader()
{
    _loaders["obj"] = &ModelLoader::loadOBJTinyObjLoader;
}

void ModelLoader::load(std::string filename, MeshCache& cache)
{
    std::ifstream file;
    std::size_t   extensionLength;
    std::string   extension;

    extensionLength = filename.find_last_of(".");
    Assert(extensionLength != std::string::npos, "Invalid file name \'" + filename + "\'");

    extension = filename.substr(extensionLength + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    Assert(_loaders.count(extension) > 0, "Unknown file extension \'" + extension + "\'");

    file.open(filename);
    Assert(file.good(), "Could not open file \'" + filename + "\'");

    Mesh mesh = (_loaders.at(extension))(file);
    cache.loadMesh(filename, mesh);

    Assert(mesh.vertices.size() > 0, "loaded empty mesh");
}

Mesh ModelLoader::loadOBJ(std::ifstream& src)
{
    return loadOBJTinyObjLoader(src);
}

Mesh ModelLoader::loadOBJ(const std::string& filename)
{
    std::ifstream file(filename);
    return loadOBJ(file);
}

// A lot of cache miss with this method.
Mesh ModelLoader::loadOBJTinyObjLoader(std::ifstream& src)
{
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      err;
    const bool                       triangulate = true;

    const bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, &src, nullptr, triangulate);

    Assert(err.empty(), err);
    Assert(ret, "could not load obj");
    Assert(shapes.size() > 0, "no shapes to load");

    Mesh mesh;
    mesh.vertices.reserve(attrib.vertices.size());
    mesh.normals.reserve(attrib.normals.size());
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
                mesh.vertices.push_back(vertex);

                if (!attrib.normals.empty())
                {
                    tinyobj::real_t  nx = attrib.normals[3 * indexInfo.normal_index + 0];
                    tinyobj::real_t  ny = attrib.normals[3 * indexInfo.normal_index + 1];
                    tinyobj::real_t  nz = attrib.normals[3 * indexInfo.normal_index + 2];
                    const glm::fvec3 normal(nx, ny, nz);
                    mesh.normals.push_back(normal);
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

Mesh ModelLoader::loadOBJCustom(std::ifstream& src)
{
    Mesh                    mesh;
    std::string             line;
    glm::vec3               tmpVec3;
    glm::vec2               tmpVec2;
    glm::uvec3              vIdx;
    std::vector<glm::vec3>  vertices;
    std::vector<glm::vec3>  normals;
    std::vector<glm::vec2>  uvs;
    std::vector<glm::uvec3> indexes;

    while (std::getline(src, line))
    {
        std::istringstream ss(line);
        ss >> line;
        if (line == "v")
        {
            ss >> tmpVec3[0] >> tmpVec3[1] >> tmpVec3[2];
            vertices.push_back(tmpVec3);
        }
        else if (line == "vn")
        {
            ss >> tmpVec3[0] >> tmpVec3[1] >> tmpVec3[2];
            normals.push_back(tmpVec3);
        }
        else if (line == "vt")
        {
            ss >> tmpVec2[0] >> tmpVec2[1];
            tmpVec2[1] = 1.f - tmpVec2[1]; // Revert V coordinate
            uvs.push_back(tmpVec2);
        }
        else if (line == "f")
        {
            std::string val;
            std::size_t pos;
            for (int i = 0; i < 3; ++i)
            {
                ss >> line;
                for (int j = 0; j < 3; ++j)
                {
                    val = line.substr(0, line.find_first_of('/'));
                    if (!val.empty())
                        vIdx[j] = std::abs(static_cast<long>(std::stoul(val))) - 1;
                    else
                        vIdx[j] = 0;
                    if ((pos = line.find_first_of('/')) != std::string::npos)
                        line = line.substr(pos + 1);
                    else
                        line = "";
                }
                indexes.push_back(vIdx);
            }
            ss >> line;
            if (!line.empty())
            {
                for (int j = 0; j < 3; ++j)
                {
                    val = line.substr(0, line.find_first_of('/'));
                    if (!val.empty())
                        vIdx[j] = std::abs(static_cast<long>(std::stoul(val))) - 1;
                    else
                        vIdx[j] = 0;
                    if ((pos = line.find_first_of('/')) != std::string::npos)
                        line = line.substr(pos + 1);
                    else
                        line = "";
                }
                indexes.push_back(indexes[indexes.size() - 3]);
                indexes.push_back(indexes[indexes.size() - 2]);
                indexes.push_back(vIdx);
            }
        }
    }
    mesh.hasUVs = !(uvs.empty());
    mesh.hasNormals = !(normals.empty());

    Assert(!vertices.empty() && !indexes.empty(), "Empty model");

    // CW Winding test
    if (mesh.hasNormals)
    {
        glm::vec3 a = vertices[indexes[1][0]] - vertices[indexes[0][0]];
        glm::vec3 b = vertices[indexes[2][0]] - vertices[indexes[0][0]];
        glm::vec3 n = normals[indexes[0][2]] + normals[indexes[1][2]] + normals[indexes[2][2]];
        if (glm::dot(glm::cross(a, b), n) < 0.0f)
        {
            Assert(false, "model has been reverted");
            glm::vec3 t;
            for (unsigned i = 0; (i + 2) < indexes.size(); i += 3)
            {
                t = indexes[i + 1];
                indexes[i + 1] = indexes[i + 2];
                indexes[i + 2] = t;
            }
        }
    }
    for (unsigned i = 0; i < indexes.size(); ++i)
    {
        mesh.vertices.push_back(vertices[indexes[i][0]]);
        if (mesh.hasUVs)
            mesh.uvs.push_back(uvs[indexes[i][1]]);
        if (mesh.hasNormals)
            mesh.normals.push_back(normals[indexes[i][2]]);
        mesh.indexes.push_back(i);
    }

    if (!mesh.hasNormals)
        computeNormalsSimple(mesh);

    return mesh;
}

// FIXME safety checks for ptr+count ?
void SaveMeshesAsObj(std::ostream& output, const Mesh* meshes, u32 meshCount)
{
    u32 vertexOffset = 0;
    u32 uvOffset = 0;
    u32 normalOffset = 0;

    for (u32 meshIndex = 0; meshIndex < meshCount; meshIndex++)
    {
        const Mesh& mesh = meshes[meshIndex];
        const u32   vertexCount = static_cast<u32>(mesh.vertices.size());
        const u32   uvCount = static_cast<u32>(mesh.uvs.size());
        const u32   normalCount = static_cast<u32>(mesh.normals.size());
        const u32   indexCount = static_cast<u32>(mesh.indexes.size());

        Assert(indexCount % 3 == 0);

        for (u32 i = 0; i < vertexCount; i++)
        {
            output << 'v';
            for (u32 j = 0; j < 3; ++j)
                output << ' ' << mesh.vertices[i][j];
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
