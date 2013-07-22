#include <iostream>
#include <ctime>
#include "Exceptions/CoreException.hpp"
#include "ReaperCore.hh"
#include "Reaper.hh"

ReaperCore::ReaperCore()
{
  std::string file("log");
  _logFile.open(file.c_str(), std::ios::out | std::ios::trunc);
  if (!_logFile.is_open())
    throw (CoreException("Could not open log file \"" + file + "\""));
  if (!_logFile.good())
    throw (CoreException("Invalid log file"));
  _clogBuffSave = std::clog.rdbuf();
  _cerrBuffSave = std::cerr.rdbuf();

  // FIXME uncomment redirections
  std::clog.rdbuf(_logFile.rdbuf());
  std::cerr.rdbuf(_logFile.rdbuf());
}

ReaperCore::~ReaperCore()
{
  std::clog.rdbuf(_clogBuffSave);
  std::cerr.rdbuf(_cerrBuffSave);
  _logFile.close();
  if (_logFile.fail())
    std::cerr << "Could not close log" << std::endl;
}

void ReaperCore::run()
{
  std::time_t t = std::time(nullptr);
  _logFile << "Log started on " << std::ctime(&t);
  try
  {
    Reaper prgm(_glContext);
    _glContext.create();
    prgm.run();
    _glContext.destroy();
    t = std::time(nullptr);
    _logFile << "Log ended on " << std::ctime(&t);
  }
  catch (const ReaperException& e)
  {
    std::cerr << "Exception caught: " << e.what() << std::endl;
  }
}
