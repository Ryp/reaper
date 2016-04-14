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

#include "Model.hh"

class ModelLoader
{
public:
    ModelLoader();
    ~ModelLoader();

public:
    Model*    load(std::string filename);

private:
    Model*    loadOBJAssimp(std::ifstream& src);
    Model*    loadOBJCustom(std::ifstream& src);

private:
    std::map<std::string, Model* (ModelLoader::*)(std::ifstream&)> _parsers;
};

#endif // REAPER_MODELLOADER_INCLUDED
