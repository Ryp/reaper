#ifndef MODELLOADEREXCEPTION_HPP
#define MODELLOADEREXCEPTION_HPP

#include <string>
#include "ReaperException.hpp"

class ModelLoaderException : public ReaperException
{
public:
  ModelLoaderException(const std::string& message) : ReaperException("ModelLoader::" + message) {};
  virtual ~ModelLoaderException() throw() {};
};

#endif // MODELLOADEREXCEPTION_HPP
