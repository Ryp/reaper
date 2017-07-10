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

private:
    void loadOBJAssimp(const std::string& filename, std::ifstream& src, MeshCache& cache);
    void loadOBJCustom(const std::string& filename, std::ifstream& src, MeshCache& cache);

private:
    std::map<std::string, void (ModelLoader::*)(const std::string&, std::ifstream&, MeshCache&)> _loaders;
};

REAPER_MESH_API
void SaveMeshAsObj(std::ostream& output, const Mesh& mesh);
