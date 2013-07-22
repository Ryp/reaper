#ifndef COREEXCEPTION_HPP
#define COREEXCEPTION_HPP

#include <string>
#include "ReaperException.hpp"

class CoreException : public ReaperException
{
public:
  CoreException(const std::string& message) : ReaperException("Core::" + message) {};
  virtual ~CoreException() throw() {};
};

#endif // COREEXCEPTION_HPP
