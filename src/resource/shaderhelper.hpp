#ifndef SHADERHELPER_INCLUDED
#define SHADERHELPER_INCLUDED

#include <glbinding/gl/gl.h>
using namespace gl;

#include <mogl/shader/shaderprogram.hpp>

class ShaderHelper
{
    ShaderHelper() = delete;

public:
    static void loadSimpleShader(mogl::ShaderProgram& shader, const std::string& vertexFile, const std::string& fragmentFile);
};

#endif // SHADERHELPER_INCLUDED
