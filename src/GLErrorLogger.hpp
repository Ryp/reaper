#ifndef GLERRORLOGGER_HPP
#define GLERRORLOGGER_HPP

#include <ostream>
#include <map>
#include <string>

#include "GLHeaders.hpp"

class GLErrorLogger
{
public:
  GLErrorLogger(std::ostream& log);
  ~GLErrorLogger();

public:
  void operator()();

private:
  std::ostream&			_log;
  std::map<GLenum, std::string>	_strings;
  GLenum			_errorCode;
};

#endif // GLERRORLOGGER_HPP
