#ifndef GLCONTEXT_HH
#define GLCONTEXT_HH

#include "GLHeaders.hpp"
#include "Vector.hpp"

class GLContext
{
  static const int DefaultWindowWidth = 1280;
  static const int DefaultWindowHeight = 768;
  static const bool DefaultFullscreen = false;

public:
  GLContext();
  ~GLContext();

public:
  void		create(int width = DefaultWindowWidth, int height = DefaultWindowHeight, bool fullscreen = DefaultFullscreen);
  bool		isOpen();
  void		swapBuffers();
  void		destroy();
  const Vect2u&	getWindowSize() const;
  double	getTime() const;

public:
  void printLog(bool full);

private:
  GLFWwindow*	_window;
  Vect2u	_windowSize;
};

#endif // GLCONTEXT_HH
