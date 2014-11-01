#include "AController.hpp"

AController::AController(int buttons, int axes)
:   _buttons(buttons),
    _axes(axes)
{}

AController::~AController() {}

void AController::reset()
{
    Button b;

    b.held = false;
    b.new_held = false;
    b.pressed = false;
    b.released = false;
    for (auto& button : _buttons)
        button = b;
    for (auto& axe : _axes)
        axe = 0;
}

void AController::update()
{
    for (auto& button : _buttons)
    {
        button.pressed = !button.held && button.new_held;
        button.released = button.held && !button.new_held;
        button.held = button.new_held;
    }
}

bool AController::isHeld(unsigned int idx) const
{
    return (_buttons[idx].held);
}

bool AController::isPressed(unsigned int idx) const
{
    return (_buttons[idx].pressed);
}

bool AController::isReleased(unsigned int idx) const
{
    return (_buttons[idx].released);
}

float AController::getAxis(unsigned int idx) const
{
    return (_axes[idx]);
}