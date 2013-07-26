#ifndef SHADEROBJECTEXCEPTION_HPP
#define SHADEROBJECTEXCEPTION_HPP

#include <string>
#include "ReaperException.hpp"

class ShaderObjectException : public ReaperException
{
public:
  ShaderObjectException(const std::string& message) : ReaperException("ShaderObject::" + message) {};
  virtual ~ShaderObjectException() throw() {};
};

#endif // SHADEROBJECTEXCEPTION_HPP
