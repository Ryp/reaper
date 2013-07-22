#include <vector>
#include <fstream>
#include <iomanip>

#include "Shader.hh"
#include "Exceptions/ShaderException.hpp"

Shader::Shader(const std::string& vertex, const std::string& fragment)
{
  _handle = loadShader(vertex, fragment);
  retrieveLocations();

  for (std::map<std::string, GLuint>::const_iterator it = _attribs.begin(); it != _attribs.end(); ++it)
    std::cout << "Attrib: " << (*it).first << " (Location: " << (*it).second << ")" << std::endl;
  for (std::map<std::string, GLuint>::const_iterator it = _uniforms.begin(); it != _uniforms.end(); ++it)
    std::cout << "Uniform: " << (*it).first << " (Location: " << (*it).second << ")" << std::endl;
}

Shader::~Shader()
{
  glDeleteProgram(_handle);
}

GLuint Shader::getHandle() const
{
  return (_handle);
}

GLuint Shader::getAttribLocation(const std::string& name) const
{
  std::map<std::string, GLuint>::const_iterator	it;
  if ((it = _attribs.find(name)) != _attribs.end())
    return (it->second);
  return (0);
}

GLuint Shader::getUniformLocation(const std::string& name) const
{
  std::map<std::string, GLuint>::const_iterator	it;
  if ((it = _uniforms.find(name)) != _uniforms.end())
    return (it->second);
  return (0);
}

void Shader::use() const
{
  glUseProgram(_handle);
}

GLuint Shader::compileShader(const std::string& shader, GLenum type)
{
  GLuint	shaderId;
  GLint		success = GL_FALSE;
  GLint		logLength = 0;
  std::string	line;
  std::string	code;
  std::ifstream	file(shader.c_str(), std::ios::in);

  std::clog << "Compiling shader: " << shader << std::endl;
  if (!file.is_open() || !file.good())
    throw (ShaderException("Could not open file \"" + shader + "\""));
  while (std::getline(file, line))
    code += line + '\n';
  file.close();
  if (file.is_open())
    std::cerr << "Could not close file" << std::endl;

  if (!(shaderId = glCreateShader(type)))
    throw (ShaderException("Could not create shader"));

  char const * srcPtr = code.c_str();
  glShaderSource(shaderId, 1, &srcPtr, nullptr);
  glCompileShader(shaderId);
  glGetShaderiv(shaderId, GL_COMPILE_STATUS, &success);
  if (success == GL_FALSE)
  {
    glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
      std::vector<GLchar> infoLog(logLength);
      glGetShaderInfoLog(shaderId, logLength, &logLength, &infoLog[0]);
      infoLog[logLength - 1] = '\0';
      std::clog << &infoLog[0];
    }
    glDeleteShader(shaderId);
    throw (ShaderException("Could not compile \"" + shader + "\""));
  }
  return (shaderId);
}

GLuint Shader::loadShader(const std::string& vertex, const std::string& fragment)
{
  GLuint	programId;
  GLint		success = GL_FALSE;
  GLint		logLength = 0;
  GLuint	vertexId = compileShader(vertex, GL_VERTEX_SHADER);
  GLuint	fragmentId = compileShader(fragment, GL_FRAGMENT_SHADER);

  std::clog << "Linking shader" << std::endl;
  if (!(programId = glCreateProgram()))
    throw (ShaderException("Could not create shader program"));
  glAttachShader(programId, vertexId);
  glAttachShader(programId, fragmentId);
  glLinkProgram(programId);

  glGetProgramiv(programId, GL_LINK_STATUS, &success);
  if (success == GL_FALSE)
  {
    glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
      std::vector<GLchar> infoLog(logLength);
      glGetProgramInfoLog(programId, logLength, &logLength, &infoLog[0]);
      std::clog << "Shader linking log:" << std::endl << &infoLog[0];
    }
    glDeleteProgram(programId);
    glDeleteShader(vertexId);
    glDeleteShader(fragmentId);
    throw (ShaderException("Failed to link program"));
  }
  glDeleteShader(vertexId);
  glDeleteShader(fragmentId);
  std::clog << "Shader program created" << std::endl;
  return (programId);
}

void Shader::retrieveLocations()
{
  GLint		n, maxLen;
  GLint		size, location;
  GLsizei	written;
  GLenum	type;
  GLchar*	name;

  glGetProgramiv(_handle, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLen);
  glGetProgramiv(_handle, GL_ACTIVE_ATTRIBUTES, &n);
  name = new GLchar[maxLen];
  for (int i = 0; i < n; ++i)
  {
    glGetActiveAttrib(_handle, i, maxLen, &written, &size, &type, name);
    location = glGetAttribLocation(_handle, name);
    _attribs[name] = location;
  }
  delete name;
  glGetProgramiv(_handle, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLen);
  glGetProgramiv(_handle, GL_ACTIVE_UNIFORMS, &n);
  name = new GLchar[maxLen];
  for (int i = 0; i < n; ++i)
  {
    glGetActiveUniform(_handle, i, maxLen, &written, &size, &type, name);
    location = glGetUniformLocation(_handle, name);
    _uniforms[name] = location;
  }
  delete name;
}
