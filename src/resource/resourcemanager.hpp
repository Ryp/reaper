////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015 Thibault Schueller
/// This file is distributed under the MIT License
///
/// @file resourcemanager.hpp
/// @author Thibault Schueller <ryp.sqrt@gmail.com>
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_RESOURCEMANAGER_INCLUDED
#define REAPER_RESOURCEMANAGER_INCLUDED

#include <unordered_map>
#include <string>

#include "ModelLoader.hh"

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
