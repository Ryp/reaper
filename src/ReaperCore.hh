#ifndef REAPERCORE_HH
#define REAPERCORE_HH

#include <iostream>
#include <fstream>
#include "GLContext.hh"

class ReaperCore
{
public:
  ReaperCore();
  virtual ~ReaperCore();

public:
  void	run();

private:
  std::ofstream		_logFile;
  std::streambuf*	_clogBuffSave;
  std::streambuf*	_cerrBuffSave;
  GLContext		_glContext;
};

#endif // REAPERCORE_HH
