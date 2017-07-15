////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <fstream>
#include <string>
#include <map>

#include "Mesh.h"
#include "MeshCache.h"

class REAPER_MESH_API ModelLoader
{
public:
    ModelLoader();
    ~ModelLoader() = default;

public:
    void load(std::string filename, MeshCache& cache);

public:
    static Mesh loadOBJAssimp(std::ifstream& src);
    static Mesh loadOBJCustom(std::ifstream& src);

private:
    using LoaderFunc = Mesh (*)(std::ifstream&);

    std::map<std::string, LoaderFunc> _loaders;
};

REAPER_MESH_API
void SaveMeshesAsObj(std::ostream& output, const Mesh* meshes, u32 meshCount);
