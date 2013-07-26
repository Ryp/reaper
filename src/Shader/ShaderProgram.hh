#ifndef SHADERPROGRAM_HH
#define SHADERPROGRAM_HH

#include <string>
#include <map>

#include "GLHeaders.hpp"
#include "ShaderObject.hh"

class ShaderProgram
{
public:
  ShaderProgram();
  ~ShaderProgram();

public:
  GLuint	getHandle() const;
  GLuint	getAttribLocation(const std::string& name) const;
  GLuint	getUniformLocation(const std::string& name) const;
  void		use() const;
  void		debugPrintLocations();
  void		attach(const ShaderObject& object);
  void		detach(const ShaderObject& object);
  void		link();

private:
  void		retrieveLocations();

private:
  std::map<std::string, GLuint>	_attribs;
  std::map<std::string, GLuint>	_uniforms;
  GLuint			_handle;
};

#endif // SHADERPROGRAM_HH
