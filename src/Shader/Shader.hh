#ifndef SHADER_HH
#define SHADER_HH

#include <iostream>
#include <string>
#include <map>

#include "GLHeaders.hpp"

class Shader
{
public:
  Shader(const std::string& vertex, const std::string& fragment);
  virtual ~Shader();

public:
  GLuint	getHandle() const;
  GLuint	getAttribLocation(const std::string& name) const;
  GLuint	getUniformLocation(const std::string& name) const;
  void		use() const;

public:
  static GLuint	compileShader(const std::string& file, GLenum type);
  static GLuint	loadShader(const std::string& vertex, const std::string& fragment);

private:
  void		retrieveLocations();

private:
  std::map<std::string, GLuint>	_attribs;
  std::map<std::string, GLuint>	_uniforms;
  GLuint			_handle;
};

#endif // SHADER_HH
