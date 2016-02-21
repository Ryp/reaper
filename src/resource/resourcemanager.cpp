////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2016 Thibault Schueller
/// This file is distributed under the MIT License
///
/// @file resourcemanager.cpp
/// @author Thibault Schueller <ryp.sqrt@gmail.com>
////////////////////////////////////////////////////////////////////////////////

#include "resourcemanager.hpp"

#include "Model.hh"

ResourceManager::ResourceManager(const std::string& path)
:   _path(path + '/')
{}

Model* ResourceManager::getModel(const std::string& file)
{
    if (!_models[file])
        return (loadModel(file));
    return (_models[file]);
}

Model* ResourceManager::loadModel(const std::string& file)
{
    return (_models[file] = _loader.load(_path + file));
}
