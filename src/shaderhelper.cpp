#include "shaderhelper.hpp"

#include <fstream>

void ShaderHelper::loadSimpleShader(mogl::ShaderProgram& shader, const std::string& vertexFile, const std::string& fragmentFile)
{
    std::ifstream       vsFile(vertexFile);
    std::ifstream       fsFile(fragmentFile);
    mogl::ShaderObject  vertex(vsFile, mogl::ShaderObject::ShaderType::VertexShader);
    mogl::ShaderObject  fragment(fsFile, mogl::ShaderObject::ShaderType::FragmentShader);

    if (!vertex.compile())
        throw(std::runtime_error(vertex.getLog()));
    if (!fragment.compile())
        throw(std::runtime_error(fragment.getLog()));

    shader.attach(vertex);
    shader.attach(fragment);

    if (!shader.link())
        throw(std::runtime_error(vertexFile + " " + shader.getLog()));
}
