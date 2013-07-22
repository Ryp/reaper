#ifndef CONTROLLEREXCEPTION_HPP
#define CONTROLLEREXCEPTION_HPP

#include <string>
#include "ReaperException.hpp"

class ControllerException : public ReaperException
{
public:
  ControllerException(const std::string& message) : ReaperException("Controller::" + message) {};
  virtual ~ControllerException() throw() {};
};

#endif // CONTROLLEREXCEPTION_HPP
