#ifndef POSTPROCESSOREXCEPTION_HPP
#define POSTPROCESSOREXCEPTION_HPP

#include <string>
#include "ReaperException.hpp"

class PostProcessorException : public ReaperException
{
public:
  PostProcessorException(const std::string& message) : ReaperException("PostProcessor::" + message) {};
  virtual ~PostProcessorException() throw() {};
};

#endif // POSTPROCESSOREXCEPTION_HPP
