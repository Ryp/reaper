#include <iostream>
#include "GLContext.hh"
#include "Exceptions/GLContextException.hpp"

GLContext::GLContext()
  : _window(nullptr),
    _windowSize(0)
{}

GLContext::~GLContext() {}

void GLContext::create(int width, int height, bool fullscreen)
{
  if (!glfwInit())
    throw (GLContextException("Failed to initialize GLFW"));
  GLFWmonitor* monitor;
  if (!(monitor = glfwGetPrimaryMonitor()))
    throw (GLContextException("Failed to retrieve primary monitor"));
  if (!(_window = glfwCreateWindow(width, height, "ReaperGL", fullscreen ? monitor : nullptr, nullptr)))
  {
    destroy();
    throw (GLContextException("Failed to open GLFW window"));
  }
  _windowSize = Vect2u(width, height);
  glfwMakeContextCurrent(_window);

  // FIXME Needed for core profile (may generate GLenum error)
  glewExperimental = true;

  if (glewInit() != GLEW_OK)
    throw (GLContextException("Failed to initialize GLEW"));
  glfwSwapInterval(1);
}

const Vect2u& GLContext::getWindowSize() const
{
  return (_windowSize);
}

bool GLContext::isOpen()
{
  return (!glfwWindowShouldClose(_window));
}

void GLContext::swapBuffers()
{
  glfwSwapBuffers(_window);
}

void GLContext::destroy()
{
  if (_window)
    glfwDestroyWindow(_window);
  glfwTerminate();
}

void GLContext::printLog(bool full)
{
  GLint major, minor;
  GLint nExtensions;

  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  glGetIntegerv(GL_NUM_EXTENSIONS, &nExtensions);
  std::clog << "########## ContextLog ##########" << std::endl;
  std::clog << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
  std::clog << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
  std::clog << "Version string: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
  std::clog << "Version number: " << major << "." << minor << std::endl;
  if (full)
  {
    std::clog << "OpenGL extensions supported:" << std::endl;
    for (int i = 0; i < nExtensions; ++i)
      std::clog << glGetStringi(GL_EXTENSIONS, i) << std::endl;
  }
  std::clog << "################################" << std::endl;
}
