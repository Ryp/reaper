#include "AController.hpp"

AController::AController(int buttons, int axes)
  : _buttons(buttons),
    _axes(axes)
{}

AController::~AController() {}

void AController::reset()
{
  for (std::vector<Button>::iterator it = _buttons.begin(); it != _buttons.end(); it++)
  {
    (*it).held = false;
    (*it).new_held = false;
    (*it).pressed = false;
    (*it).released = false;
  }
  for (std::vector<Axe>::iterator it = _axes.begin(); it != _axes.end(); it++)
    (*it) = 0;
}

void AController::update()
{
  for (std::vector<Button>::iterator it = _buttons.begin(); it != _buttons.end(); it++)
  {
    (*it).pressed = !(*it).held && (*it).new_held;
    (*it).released = (*it).held && !(*it).new_held;
    (*it).held = (*it).new_held;
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
