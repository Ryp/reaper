#ifndef SHADEROBJECT_HH
#define SHADEROBJECT_HH

#include <string>

#include "GLHeaders.hpp"

class ShaderObject
{
public:
  ShaderObject(const std::string& source, GLenum type);
  ~ShaderObject();

  GLuint	getHandle() const;
  GLenum	getType() const;

private:
  void		compile();

private:
  const std::string	_source;
  GLuint		_handle;
  GLenum		_type;
};

#endif // SHADEROBJECT_HH
