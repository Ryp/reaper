#include "GLErrorLogger.hpp"

GLErrorLogger::GLErrorLogger(std::ostream& log)
  : _log(log)
{
  _strings[GL_INVALID_ENUM] = "Invalid enum";
  _strings[GL_INVALID_VALUE] = "Invalid value";
  _strings[GL_INVALID_OPERATION] = "Invalid operation";
  _strings[GL_STACK_OVERFLOW] = "Stack overflow";
  _strings[GL_STACK_UNDERFLOW] = "Stack underflow";
  _strings[GL_OUT_OF_MEMORY] = "Out of memory";
}

GLErrorLogger::~GLErrorLogger() {}

void GLErrorLogger::operator()()
{
  while ((_errorCode = glGetError()) != GL_NO_ERROR)
    _log << "GLError: " << _strings[_errorCode] << " (" << _errorCode << ")" << std::endl;
}
