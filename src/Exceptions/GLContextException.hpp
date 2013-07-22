#ifndef GLCONTEXTEXCEPTION_HPP
#define GLCONTEXTEXCEPTION_HPP

#include <string>
#include "ReaperException.hpp"

class GLContextException : public ReaperException
{
public:
  GLContextException(const std::string& message) : ReaperException("GLContext::" + message) {};
  virtual ~GLContextException() throw() {};
};

#endif // GLCONTEXTEXCEPTION_HPP
