#ifndef REAPER_HH
#define REAPER_HH

#include "GLContext.hh"
#include "GLErrorLogger.hpp"
#include "Joystick/SixAxis.hh"

class Reaper
{
public:
  Reaper(GLContext& context);
  virtual ~Reaper();

public:
  void run();

private:
  GLContext&	_context;
  GLErrorLogger _errorLogger;
  SixAxis	_controller;
};

#endif // REAPER_HH
