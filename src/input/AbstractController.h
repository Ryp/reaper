////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "InputExport.h"

#include <vector>

class REAPER_INPUT_API AbstractController
{
public:
    AbstractController(int button_count, int axis_count);
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

public:
    std::vector<Button> buttons;
    std::vector<Axe>    axes;
};
