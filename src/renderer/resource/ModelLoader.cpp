////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ModelLoader.h"

#include <vector>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <string.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/geometric.hpp>

ModelLoader::ModelLoader()
{
    //     _parsers["obj"] = &ModelLoader::loadOBJCustom;
    _parsers["obj"] = &ModelLoader::loadOBJAssimp;
}

ModelLoader::~ModelLoader() {}

Model* ModelLoader::load(std::string filename)
{
    Model*          model = nullptr;
    std::ifstream   file;
    std::size_t     extensionLength;
    std::string     extension;

    if ((extensionLength = filename.find_last_of(".")) == std::string::npos)
        throw (std::runtime_error("Invalid file name \'" + filename + "\'"));
    extension = filename.substr(extensionLength + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    if (_parsers[extension] == nullptr)
        throw (std::runtime_error("Unknown file extension \'" + extension + "\'"));
    file.open(filename);
    if (!file.good())
        throw (std::runtime_error("Could not open file \'" + filename + "\'"));
    model = ((*this).*(_parsers[extension]))(file);
    return (model);
}

static inline glm::vec3 to_glm3(aiVector3D& v)
{
    return glm::vec3(v.x, v.y, v.z);
}

static inline glm::vec2 to_glm2(aiVector3D& v)
{
    return glm::vec2(v.x, v.y);
}

Model* ModelLoader::loadOBJAssimp(std::ifstream& src)
{
    Model*              model = new Model;
    Assimp::Importer    importer;
    std::string         content(std::istreambuf_iterator<char>(static_cast<std::istream&>(src)), std::istreambuf_iterator<char>());
    const aiScene*      scene;
    aiMesh*             mesh = nullptr;
    unsigned int        flags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace;

    scene = importer.ReadFileFromMemory(content.c_str(), content.size(), flags);
    if (!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        throw std::runtime_error(std::string("loadOBJAssimp::") + importer.GetErrorString());

    // Only support simplistic scene structures
    if (!scene->HasMeshes())
        throw std::runtime_error("loadOBJAssimp::no mesh found");
    mesh = scene->mMeshes[0];

    model->_vertices.reserve(mesh->mNumVertices);
    model->_normals.reserve(mesh->mNumVertices);
    model->_tangents.reserve(mesh->mNumVertices);
    model->_bitangents.reserve(mesh->mNumVertices);
    model->_uvs.reserve(mesh->mNumVertices);
    for (std::size_t i = 0; i < mesh->mNumVertices; ++i)
    {
        model->_vertices.push_back(to_glm3(mesh->mVertices[i]));
        model->_normals.push_back(to_glm3(mesh->mNormals[i]));
        if (mesh->mTangents)
            model->_tangents.push_back(to_glm3(mesh->mTangents[i]));
        if (mesh->mBitangents)
            model->_bitangents.push_back(to_glm3(mesh->mBitangents[i]));
        if (mesh->mTextureCoords[0])
            model->_uvs.push_back(to_glm2(mesh->mTextureCoords[0][i]));
        else
            model->_uvs.push_back(glm::vec2(0.0f, 0.0f));
    }
    model->_hasNormals = mesh->mNormals != nullptr;
    model->_hasUVs = mesh->mTextureCoords[0] != nullptr;
    for (std::size_t i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace face = mesh->mFaces[i];
        for(std::size_t j = 0; j < face.mNumIndices; ++j)
            model->_indexes.push_back(face.mIndices[j]);
    }
    return model;
}

Model* ModelLoader::loadOBJCustom(std::ifstream& src)
{
    Model*                  model = new Model;
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
            tmpVec2[1] = 1.0 - tmpVec2[1]; // Revert V coordinate
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
                        vIdx[j] = abs(std::stoul(val)) - 1;
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
                        vIdx[j] = abs(std::stoul(val)) - 1;
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
    model->_hasUVs = !(uvs.empty());
    model->_hasNormals = !(normals.empty());
    if (vertices.empty() || indexes.empty())
        throw (std::runtime_error("Empty model"));
    // CW Winding test
    if (model->_hasNormals)
    {
        glm::vec3   a = vertices[indexes[1][0]] - vertices[indexes[0][0]];
        glm::vec3   b = vertices[indexes[2][0]] - vertices[indexes[0][0]];
        glm::vec3   n = normals[indexes[0][2]] + normals[indexes[1][2]] + normals[indexes[2][2]];
        if (glm::dot(glm::cross(a, b), n) < 0.0f)
        {
            glm::vec3   t;
            for (unsigned i = 0; (i + 2) < indexes.size(); i += 3)
            {
                t = indexes[i + 1];
                indexes[i + 1] = indexes[i + 2];
                indexes[i + 2] = t;
            }
            // Model has been reverted
        }
    }
    for (unsigned i = 0; i < indexes.size(); ++i)
    {
        model->_vertices.push_back(vertices[indexes[i][0]]);
        if (model->_hasUVs)
            model->_uvs.push_back(uvs[indexes[i][1]]);
        if (model->_hasNormals)
            model->_normals.push_back(normals[indexes[i][2]]);
        model->_indexes.push_back(i);
    }
    if (!model->_hasNormals)
        model->computeNormalsSimple();
    return (model);
}
