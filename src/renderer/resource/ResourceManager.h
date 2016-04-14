////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_RESOURCEMANAGER_INCLUDED
#define REAPER_RESOURCEMANAGER_INCLUDED

#include <unordered_map>
#include <string>

#include "ModelLoader.h"

class ResourceManager
{
public:
    ResourceManager(const std::string& path);
    ~ResourceManager() = default;

    ResourceManager(const ResourceManager& other) = delete;
    ResourceManager& operator=(const ResourceManager& other) = delete;

public:
    Model*  getModel(const std::string& file);

private:
    Model*  loadModel(const std::string& file);

public:
    std::string                             _path;
    std::unordered_map<std::string, Model*> _models;
    ModelLoader                             _loader;
};

#endif // REAPER_RESOURCEMANAGER_INCLUDED
