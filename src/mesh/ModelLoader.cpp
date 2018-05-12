////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2018 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ModelLoader.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string.h>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/geometric.hpp>

ModelLoader::ModelLoader()
{
    //_loaders["obj"] = &ModelLoader::loadOBJCustom;
    _loaders["obj"] = &ModelLoader::loadOBJAssimp;
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
}

static inline glm::vec3 to_glm3(aiVector3D& v)
{
    return glm::vec3(v.x, v.y, v.z);
}

static inline glm::vec2 to_glm2(aiVector3D& v)
{
    return glm::vec2(v.x, v.y);
}

Mesh ModelLoader::loadOBJAssimp(std::ifstream& src)
{
    Mesh             mesh;
    Assimp::Importer importer;
    std::string      content(std::istreambuf_iterator<char>(static_cast<std::istream&>(src)),
                             std::istreambuf_iterator<char>());
    const aiScene*   scene;
    aiMesh*          assimpMesh = nullptr;
    unsigned int     flags =
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace;

    scene = importer.ReadFileFromMemory(content.c_str(), content.size(), flags);

    Assert(scene && !(scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode,
           std::string("loadOBJAssimp::") + importer.GetErrorString());

    // Only support simplistic scene structures
    Assert(scene->HasMeshes(), "loadOBJAssimp::no mesh found");

    assimpMesh = scene->mMeshes[0];

    mesh.vertices.reserve(assimpMesh->mNumVertices);
    mesh.normals.reserve(assimpMesh->mNumVertices);
    mesh.tangents.reserve(assimpMesh->mNumVertices);
    mesh.bitangents.reserve(assimpMesh->mNumVertices);
    mesh.uvs.reserve(assimpMesh->mNumVertices);
    for (std::size_t i = 0; i < assimpMesh->mNumVertices; ++i)
    {
        mesh.vertices.push_back(to_glm3(assimpMesh->mVertices[i]));
        mesh.normals.push_back(to_glm3(assimpMesh->mNormals[i]));
        if (assimpMesh->mTangents)
            mesh.tangents.push_back(to_glm3(assimpMesh->mTangents[i]));
        if (assimpMesh->mBitangents)
            mesh.bitangents.push_back(to_glm3(assimpMesh->mBitangents[i]));
        if (assimpMesh->mTextureCoords[0])
            mesh.uvs.push_back(to_glm2(assimpMesh->mTextureCoords[0][i]));
        else
            mesh.uvs.push_back(glm::vec2(0.0f, 0.0f));
    }
    mesh.hasNormals = assimpMesh->mNormals != nullptr;
    mesh.hasUVs = assimpMesh->mTextureCoords[0] != nullptr;
    for (std::size_t i = 0; i < assimpMesh->mNumFaces; ++i)
    {
        aiFace face = assimpMesh->mFaces[i];
        for (std::size_t j = 0; j < face.mNumIndices; ++j)
            mesh.indexes.push_back(face.mIndices[j]);
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
