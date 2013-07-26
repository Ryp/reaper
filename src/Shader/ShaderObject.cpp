#include <fstream>
#include <vector>
#include <iostream>

#include "ShaderObject.hh"
#include "Exceptions/ShaderObjectException.hpp"

ShaderObject::ShaderObject(const std::string& source, GLenum type)
  : _source(source),
    _handle(0),
    _type(type)
{
  compile();
}

GLuint ShaderObject::getHandle() const
{
  return (_handle);
}

GLuint ShaderObject::getType() const
{
  return (_type);
}

ShaderObject::~ShaderObject()
{
  glDeleteShader(_handle);
}

void ShaderObject::compile()
{
  GLuint	handle;
  GLint		success = GL_FALSE;
  GLint		logLength = 0;
  std::string	line;
  std::string	code;
  std::ifstream	file(_source.c_str(), std::ios::in);

  if (!file.is_open() || !file.good())
    throw (ShaderObjectException("Could not open file \"" + _source + "\""));
  while (std::getline(file, line))
    code += line + '\n';
  file.close();
  if (file.is_open())
    std::cerr << "Could not close file" << std::endl;

  if (!(handle = glCreateShader(_type)))
    throw (ShaderObjectException("Could not create shader"));

  char const * srcPtr = code.c_str();
  glShaderSource(handle, 1, &srcPtr, nullptr);
  glCompileShader(handle);
  glGetShaderiv(handle, GL_COMPILE_STATUS, &success);
  if (success == GL_FALSE)
  {
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
      std::vector<GLchar> infoLog(logLength);
      glGetShaderInfoLog(handle, logLength, &logLength, &infoLog[0]);
      infoLog[logLength - 1] = '\0';
      std::clog << &infoLog[0];
    }
    glDeleteShader(handle);
    throw (ShaderObjectException("Could not compile \"" + _source + "\""));
  }
  _handle = handle;
}
