////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_IMAGELOADER_INCLUDED
#define REAPER_IMAGELOADER_INCLUDED

#include <string>

namespace mogl { class Texture; }

class ImageLoader
{
    ImageLoader() = delete;

public:
    static void loadDDS(const std::string& file, mogl::Texture& texture);
    static void loadDDS3D(const std::string& file, mogl::Texture& texture);
    static void loadEXR(const std::string& file, mogl::Texture& texture);
};

#endif // REAPER_IMAGELOADER_INCLUDED
