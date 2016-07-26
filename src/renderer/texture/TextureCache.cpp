////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TextureCache.h"

void TextureCache::loadTexture(TextureId id, Texture mesh)
{
    Assert(_textures.count(id) == 0, "texture already in cache");
    _textures.emplace(id, mesh);
}

const Texture& TextureCache::operator[](const TextureId& id) const
{
    Assert(_textures.count(id) > 0, "texture missing from cache");
    return _textures.at(id);
}
