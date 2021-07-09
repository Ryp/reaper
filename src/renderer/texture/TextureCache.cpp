////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TextureCache.h"

#include <cstring>

TextureCache::TextureCache(const std::size_t cacheSize)
    : _textureAllocator(cacheSize)
{}

void TextureCache::loadTexture(TextureId id, Texture texture)
{
    void* rawDataCachePtr = _textureAllocator.alloc(texture.size);

    Assert(rawDataCachePtr != nullptr);

    std::memcpy(rawDataCachePtr, texture.rawData, texture.size);
    texture.rawData = rawDataCachePtr;

    Assert(_textures.count(id) == 0, "texture already in cache");
    _textures.emplace(id, texture);
}

const Texture& TextureCache::operator[](const TextureId& id) const
{
    Assert(_textures.count(id) > 0, "texture missing from cache");
    return _textures.at(id);
}
