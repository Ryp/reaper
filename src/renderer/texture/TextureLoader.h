////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_TEXTURELOADER_INCLUDED
#define REAPER_TEXTURELOADER_INCLUDED

#include <string>
#include <map>

#include "TextureCache.h"

class TextureLoader
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

#endif // REAPER_TEXTURELOADER_INCLUDED
