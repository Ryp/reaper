////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <string>

#include "core/memory/StackAllocator.h"
#include "resource/Resource.h"

#include "Texture.h"

class REAPER_RENDERER_API TextureCache
{
public:
    TextureCache(const std::size_t cacheSize);
    ~TextureCache() = default;

public:
    void           loadTexture(TextureId id, Texture texture);
    const Texture& operator[](const TextureId& id) const;

private:
    StackAllocator               _textureAllocator;
    std::map<TextureId, Texture> _textures;
};
