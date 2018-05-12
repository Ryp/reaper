////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2018 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

class AbstractController
{
public:
    AbstractController(int buttons, int axes);
    virtual ~AbstractController();

public:
    void  reset();
    bool  isHeld(unsigned idx) const;
    bool  isPressed(unsigned idx) const;
    bool  isReleased(unsigned idx) const;
    float getAxis(unsigned idx) const;

protected:
    virtual void update();
    virtual void destroy() = 0;

protected:
    struct Button
    {
        bool held;
        bool new_held;
        bool pressed;
        bool released;
    };
    using Axe = float;

protected:
    std::vector<Button> _buttons;
    std::vector<Axe>    _axes;
};
