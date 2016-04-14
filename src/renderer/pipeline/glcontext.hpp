////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_GLCONTEXT_INCLUDED
#define REAPER_GLCONTEXT_INCLUDED

#include <string>

class GLFWwindow;

struct Vect2u {
    unsigned int x;
    unsigned int y;
    Vect2u(unsigned int x_, unsigned int y_) : x(x_), y(y_) {}
};

class GLContext
{
public:
    GLContext();

public:
    void    create(unsigned int width, unsigned int height, unsigned int major, unsigned int minor, bool fullscreen = false, bool debug = false);
    void    destroy(); // Call this only when closing manually
    bool    isOpen();
    bool    isExtensionSupported(const std::string& extension) const;
    void    swapBuffers();
    void    makeCurrent();

public:
    const Vect2u&   getWindowSize() const;
    double          getTime() const;
    Vect2u          getCursorPosition() const;
    void            setCursorPosition(const Vect2u& position);
    void            centerCursor();
    void            setTitle(const std::string& title);

private:
    GLFWwindow* _window;
    Vect2u      _windowSize;
};

#endif // REAPER_GLCONTEXT_INCLUDED
