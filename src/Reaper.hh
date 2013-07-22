#ifndef REAPER_HH
#define REAPER_HH

#include "GLContext.hh"

class Reaper
{
public:
  Reaper(GLContext& context);
  virtual ~Reaper();

public:
  void run();

private:
  GLContext&	_context;
};

#endif // REAPER_HH
