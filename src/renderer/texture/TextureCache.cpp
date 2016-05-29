////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TextureCache.h"

void TextureCache::loadTexture(TextureId id, Texture mesh)
{
    Assert(_meshes.count(id) == 0, "texture already in cache");
    _meshes.emplace(id, mesh);
}
