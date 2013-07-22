#include "AController.hh"

AController::AController() {}

AController::~AController() {}

float AController::getRotation(int axis) const
{
  return (_rotation[axis]);
}

void AController::reset()
{
  for (int i = 0; i < 3; ++i)
  {
    _linear[i] = 0.0f;
    _rotation[i] = 0.0f;
  }
}
