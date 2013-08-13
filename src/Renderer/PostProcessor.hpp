#ifndef POSTPROCESSOR_HPP
#define POSTPROCESSOR_HPP

#include "GLHeaders.hpp"

#include "Vector.hpp"
#include "Shader/ShaderProgram.hh"
#include "GLContext.hh"

class PostProcessor
{
public:
  PostProcessor(GLContext& context, bool enabled = true);
  ~PostProcessor();

public:
  GLuint	getOutputFramebufferHandle();
  void		render();

public:
  void		enable();
  void		disable();
  void		toggle();

private:
  bool		_enabled;
  ShaderProgram	_shader;
  GLContext&	_context;

private:
  GLuint	_inputFBOHandle;
  GLuint	_frameTextureHandle;
  GLuint	_quadBuffer;

private:
  static const GLfloat	quadVertexData[];
};

#endif // POSTPROCESSOR_HPP
