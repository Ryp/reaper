////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "AbstractController.h"

AbstractController::AbstractController(int buttons, int axes)
:   _buttons(buttons),
    _axes(axes)
{}

AbstractController::~AbstractController() {}

void AbstractController::reset()
{
    Button b({false, false, false, false});

    for (auto& button : _buttons)
        button = b;
    for (auto& axe : _axes)
        axe = 0.0f;
}

bool AbstractController::isHeld(unsigned int idx) const
{
    return (_buttons[idx].held);
}

bool AbstractController::isPressed(unsigned int idx) const
{
    return (_buttons[idx].pressed);
}

bool AbstractController::isReleased(unsigned int idx) const
{
    return (_buttons[idx].released);
}

float AbstractController::getAxis(unsigned int idx) const
{
    return (_axes[idx]);
}

void AbstractController::update()
{
    for (auto& button : _buttons)
    {
        button.pressed = !button.held && button.new_held;
        button.released = button.held && !button.new_held;
        button.held = button.new_held;
    }
}
