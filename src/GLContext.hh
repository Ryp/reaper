#ifndef GLCONTEXT_HH
#define GLCONTEXT_HH

#include "GLHeaders.hpp"

class GLContext
{
  static const int DefaultWindowWidth = 1280;
  static const int DefaultWindowHeight = 768;
  static const int DefaultFullscreen = false;

public:
  GLContext();
  virtual ~GLContext();

public:
  void create(int width = DefaultWindowWidth, int height = DefaultWindowHeight, bool fullscreen = DefaultFullscreen);
  bool isOpen();
  void swapBuffers();
  void destroy();

public:
  void printLog(bool full);

private:
  GLFWwindow*	_window;
};

#endif // GLCONTEXT_HH
