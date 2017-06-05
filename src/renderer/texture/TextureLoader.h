////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <map>

#include "TextureCache.h"

class REAPER_RENDERER_API TextureLoader
{
public:
    TextureLoader();
    ~TextureLoader() = default;

public:
    void load(std::string filename, TextureCache& cache);

private:
    void loadDDS(const std::string& filename, TextureCache& cache);

private:
    std::map<std::string, void (TextureLoader::*)(const std::string&, TextureCache&)> _loaders;
};
