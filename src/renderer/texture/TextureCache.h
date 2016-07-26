////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_TEXTURECACHE_INCLUDED
#define REAPER_TEXTURECACHE_INCLUDED

#include <map>
#include <string>

#include "Texture.h"

class TextureCache {
public:
    TextureCache() = default;
    ~TextureCache() = default;

public:
    void loadTexture(TextureId id, Texture mesh);
    const Texture& operator[](const TextureId& id) const;

private:
    std::map<TextureId, Texture> _textures;
};

#endif // REAPER_TEXTURECACHE_INCLUDED
