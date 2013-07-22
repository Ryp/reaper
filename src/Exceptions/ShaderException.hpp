#ifndef SHADEREXCEPTION_HPP
#define SHADEREXCEPTION_HPP

#include <string>
#include "ReaperException.hpp"

class ShaderException : public ReaperException
{
public:
  ShaderException(const std::string& message) : ReaperException("Shader::" + message) {};
  virtual ~ShaderException() throw() {};
};

#endif // SHADEREXCEPTION_HPP
