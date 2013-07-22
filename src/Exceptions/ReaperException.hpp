#ifndef REAPEREXCEPTION_HPP
#define REAPEREXCEPTION_HPP

#include <exception>
#include <string>

class ReaperException : public std::exception
{
public:
  ReaperException(const std::string& message) : _message(message) {};
  virtual ~ReaperException() throw() {};

public:
  const char* what() const throw() { return (_message.c_str()); }

private:
  const std::string	_message;
};

#endif // REAPEREXCEPTION_HPP
