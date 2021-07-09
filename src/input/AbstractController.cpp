////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "AbstractController.h"

AbstractController::AbstractController(int button_count, int axis_count)
    : buttons(button_count)
    , axes(axis_count)
{}

AbstractController::~AbstractController()
{}

void AbstractController::reset()
{
    Button b = {false, false, false, false};

    for (auto& button : buttons)
        button = b;
    for (auto& axe : axes)
        axe = 0.0f;
}

bool AbstractController::isHeld(unsigned int idx) const
{
    return (buttons[idx].held);
}

bool AbstractController::isPressed(unsigned int idx) const
{
    return (buttons[idx].pressed);
}

bool AbstractController::isReleased(unsigned int idx) const
{
    return (buttons[idx].released);
}

float AbstractController::getAxis(unsigned int idx) const
{
    return (axes[idx]);
}

void AbstractController::update()
{
    for (auto& button : buttons)
    {
        button.pressed = !button.held && button.new_held;
        button.released = button.held && !button.new_held;
        button.held = button.new_held;
    }
}
