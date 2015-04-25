////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015 Thibault Schueller
///
/// @file imageloader.cpp
/// @author Thibault Schueller <ryp.sqrt@gmail.com>
////////////////////////////////////////////////////////////////////////////////

#include "imageloader.hpp"

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

using namespace gl;

#include <mogl/object/texture.hpp>

#include <gli/gli.hpp>
#include <gli/core/load_dds.hpp>

#include <ImfRgbaFile.h>

void ImageLoader::loadDDS(const std::string& file, mogl::Texture& texture)
{
    gli::texture2D gliTexture(gli::load_dds(file.c_str()));
    if (gliTexture.empty())
        throw (std::runtime_error("could not load texture"));
    texture.set(GL_TEXTURE_BASE_LEVEL, 0);
    texture.set(GL_TEXTURE_MAX_LEVEL, GLint(gliTexture.levels() - 1));
    texture.set(GL_TEXTURE_SWIZZLE_R, GL_RED);
    texture.set(GL_TEXTURE_SWIZZLE_G, GL_GREEN);
    texture.set(GL_TEXTURE_SWIZZLE_B, GL_BLUE);
    texture.set(GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
    texture.setStorage2D(
                   GLint(gliTexture.levels()),
                   GLenum(gli::internal_format(gliTexture.format())),
                   GLsizei(gliTexture.dimensions().x),
                   GLsizei(gliTexture.dimensions().y));
    for (gli::texture2D::size_type Level = 0; Level < gliTexture.levels(); ++Level)
    {
        if (gli::is_compressed(gliTexture.format()))
            texture.setCompressedSubImage2D(GLint(Level), 0, 0,
                                            GLsizei(gliTexture[Level].dimensions().x),
                                            GLsizei(gliTexture[Level].dimensions().y),
                                      GLenum(gli::internal_format(gliTexture.format())),
                                      GLsizei(gliTexture[Level].size()),
                                      gliTexture[Level].data()
            );
        else
            texture.setSubImage2D(GLint(Level), 0, 0,
                                GLsizei(gliTexture[Level].dimensions().x),
                                GLsizei(gliTexture[Level].dimensions().y),
                                GLenum(gli::external_format(gliTexture.format())),
                                GLenum(gli::type_format(gliTexture.format())),
                                gliTexture[Level].data()
            );
    }
}

void ImageLoader::loadEXR(const std::string& file, mogl::Texture& texture)
{
    Imf::Rgba*          pixelBuffer;
    Imf::RgbaInputFile  in(file.c_str());
    Imath::Box2i        win = in.dataWindow();
    Imath::V2i          dim(win.max.x - win.min.x + 1, win.max.y - win.min.y + 1);

    pixelBuffer = new Imf::Rgba[dim.x * dim.y];

    int dx = win.min.x;
    int dy = win.min.y;

    in.setFrameBuffer(pixelBuffer - dx - dy * dim.x, 1, dim.x);
    in.readPixels(win.min.y, win.max.y);
    texture.set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    texture.set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    texture.set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    texture.set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    mogl::setPixelStore(GL_UNPACK_ALIGNMENT, 1);
    texture.setStorage2D(1, GL_RGBA16F, dim.x, dim.y);
    texture.setSubImage2D(0, 0, 0, dim.x, dim.y, GL_RGBA, GL_HALF_FLOAT, pixelBuffer);
}
