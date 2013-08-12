#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cmath>
#include <cstring>

#include "SixAxis.hpp"
#include "Exceptions/ControllerException.hpp"

const float SixAxis::AxisDeadzone = 0.12f;

SixAxis::SixAxis(const std::string& device)
  : AController(TotalButtonsNumber, TotalAxesNumber)
{
  char deviceName[256];

  if ((_fd = open(device.c_str(), O_RDONLY | O_NONBLOCK)) == -1)
    throw (ControllerException("could not open device: " + std::string(strerror(errno))));
  if (ioctl(_fd, JSIOCGNAME(256), deviceName) == -1)
    throw (ControllerException("could not retrieve device name: " + std::string(strerror(errno))));
  if (std::string("Sony PLAYSTATION(R)3 Controller") != deviceName)
    throw (ControllerException("not a playstation 3 controller"));
  update();
  AController::reset();
}

SixAxis::~SixAxis()
{
  if (close(_fd) == -1)
    throw (ControllerException("could not close controller fd" + std::string(strerror(errno))));
}

void SixAxis::update()
{
  int			ret;
  float			val;
  struct js_event	event;

  while ((ret = read(_fd, static_cast<void*>(&event), sizeof(event))) != -1)
  {
    event.type &= ~JS_EVENT_INIT;
    if (event.type == JS_EVENT_AXIS)
    {
      val = static_cast<float>(event.value) / static_cast<float>(AxisAbsoluteResolution);
      _axes[event.number] = ((fabs(val) < AxisDeadzone) ? (0.0f) : (val));
    }
    else if (event.type == JS_EVENT_BUTTON)
      _buttons[event.number].new_held = event.value != 0;
  }
  AController::update();
}
