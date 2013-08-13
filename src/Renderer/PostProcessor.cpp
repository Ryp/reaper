#include "PostProcessor.hpp"
#include "Exceptions/PostProcessorException.hpp"

const GLfloat PostProcessor::quadVertexData[] = {
  -1.0f, -1.0f, 0.0f,
  1.0f, -1.0f, 0.0f,
  -1.0f,  1.0f, 0.0f,
  -1.0f,  1.0f, 0.0f,
  1.0f, -1.0f, 0.0f,
  1.0f,  1.0f, 0.0f,
};

PostProcessor::PostProcessor(GLContext& context, bool enabled)
  : _enabled(enabled),
    _context(context)
{
  ShaderObject vs("rc/shader/post.v.glsl", GL_VERTEX_SHADER);
  ShaderObject fs("rc/shader/post.f.glsl", GL_FRAGMENT_SHADER);
  _shader.attach(vs);
  _shader.attach(fs);
  _shader.link();

  glGenBuffers(1, &_quadBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, _quadBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertexData), quadVertexData, GL_STATIC_DRAW);

  glGenFramebuffers(1, &_inputFBOHandle);
  glBindFramebuffer(GL_FRAMEBUFFER, _inputFBOHandle);

  glGenTextures(1, &_frameTextureHandle);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _frameTextureHandle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _context.getWindowSize()[0], _context.getWindowSize()[1], 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _frameTextureHandle, 0);

  GLuint depthBuffer;
  glGenRenderbuffers(1, &depthBuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, _context.getWindowSize()[0], _context.getWindowSize()[1]);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

  GLenum DrawBuffers[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, DrawBuffers);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    throw (PostProcessorException("bad framebuffer"));
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

PostProcessor::~PostProcessor()
{

}

GLuint PostProcessor::getOutputFramebufferHandle()
{
  if (_enabled)
    return (_inputFBOHandle);
  else
    return (0);
}

void PostProcessor::render()
{
  if (!_enabled)
    return ;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, _context.getWindowSize()[0], _context.getWindowSize()[1]);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  _shader.use();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _frameTextureHandle);
  glUniform1i(_shader.getUniformLocation("renderedTexture"), 0);
  glUniform1f(_shader.getUniformLocation("time"), _context.getTime());

  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, _quadBuffer);
  glVertexAttribPointer(
    0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
    3,                  // size
    GL_FLOAT,           // type
    GL_FALSE,           // normalized?
    0,                  // stride
    (void*)0            // array buffer offset
  );

  glDrawArrays(GL_TRIANGLES, 0, 6);
  glDisableVertexAttribArray(0);
}

void PostProcessor::enable()
{
  if (!_enabled)
    _enabled = true;
}

void PostProcessor::disable()
{
  if (_enabled)
    _enabled = false;
}

void PostProcessor::toggle()
{
  if (_enabled)
    disable();
  else
    enable();
}
