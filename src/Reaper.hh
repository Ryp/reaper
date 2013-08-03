#ifndef REAPER_HH
#define REAPER_HH

#include "GLContext.hh"
#include "GLErrorLogger.hpp"

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
};

#endif // REAPER_HH
