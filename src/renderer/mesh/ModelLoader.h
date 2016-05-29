////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_MODELLOADER_INCLUDED
#define REAPER_MODELLOADER_INCLUDED

#include <fstream>
#include <string>
#include <map>

#include "Mesh.h"
#include "MeshCache.h"

class ModelLoader
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

#endif // REAPER_MODELLOADER_INCLUDED
