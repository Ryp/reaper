////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TextureLoader.h"

#include <algorithm>

#if defined(REAPER_COMPILER_MSVC)
#pragma warning (push)
#pragma warning (disable : 4100) // 'identifier' : unreferenced formal parameter
#pragma warning (disable : 4310) // cast truncates constant value
#pragma warning (disable : 4458) // declaration of 'identifier' hides class member
#endif

#include <gli/gli.hpp>
#include <gli/load.hpp>

TextureLoader::TextureLoader()
{
    _loaders["dds"] = &TextureLoader::loadDDS;
}

void TextureLoader::load(std::string filename, TextureCache& cache)
{
    std::size_t extensionLength;
    std::string extension;

    extensionLength = filename.find_last_of(".");
    Assert(extensionLength != std::string::npos,"Invalid file name \'" + filename + "\'");

    extension = filename.substr(extensionLength + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    Assert(_loaders.count(extension) > 0, "Unknown file extension \'" + extension + "\'");
    ((*this).*(_loaders.at(extension)))(filename, cache);
}

namespace
{
    PixelFormat gliFormatToInternalPixelFormat(gli::format format)
    {
        switch (format)
        {
            case gli::format::FORMAT_RGBA8_UNORM_PACK8:         return PixelFormat::R8G8B8A8_UNORM;
            case gli::format::FORMAT_BGRA8_UNORM_PACK8:         return PixelFormat::B8G8R8A8_UNORM;
            case gli::format::FORMAT_RGBA_DXT3_UNORM_BLOCK16:   return PixelFormat::BC2_UNORM_BLOCK;
            default:                                            return PixelFormat::Unknown;
        }
    }
}

void TextureLoader::loadDDS(const std::string& filename, TextureCache& cache)
{
    gli::texture gliTexture = gli::load(filename.c_str());
    Texture texture;

    Assert(!gliTexture.empty(), "empty texture");

    texture.width = gliTexture.extent().x;
    texture.height = gliTexture.extent().y;
    texture.mipLevels = static_cast<u32>(gliTexture.levels());
    texture.layerCount = static_cast<u32>(gliTexture.layers());
    texture.format = gliFormatToInternalPixelFormat(gliTexture.format());
    texture.data = gliTexture.data();
    texture.size = gliTexture.size();
    texture.bytesPerPixel = gliTexture.base_layer() / (texture.height * texture.width);

    Assert(texture.width > 0);
    Assert(texture.height > 0);
    Assert(texture.format != PixelFormat::Unknown);
    Assert(texture.data != nullptr);

    cache.loadTexture(filename, texture);
}

#if defined(REAPER_COMPILER_MSVC)
#pragma warning (pop)
#endif
