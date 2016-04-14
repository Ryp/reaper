////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_SHADERHELPER_INCLUDED
#define REAPER_SHADERHELPER_INCLUDED

#include <glbinding/gl/gl.h>
using namespace gl;

#include <mogl/object/shader/shaderprogram.hpp>

class ShaderHelper
{
    ShaderHelper() = delete;

public:
    static void loadSimpleShader(mogl::ShaderProgram& shader, const std::string& vertexFile, const std::string& fragmentFile);
};

#endif // REAPER_SHADERHELPER_INCLUDED
