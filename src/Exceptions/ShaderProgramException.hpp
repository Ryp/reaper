#ifndef SHADERPROGRAMEXCEPTION_HPP
#define SHADERPROGRAMEXCEPTION_HPP

#include <string>
#include "ReaperException.hpp"

class ShaderProgramException : public ReaperException
{
public:
  ShaderProgramException(const std::string& message) : ReaperException("ShaderProgram::" + message) {};
  virtual ~ShaderProgramException() throw() {};
};

#endif // SHADERPROGRAMEXCEPTION_HPP
