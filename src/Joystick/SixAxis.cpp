#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>

#include <iostream>

#include "SixAxis.hh"
#include "Exceptions/ControllerException.hpp"

SixAxis::SixAxis(const std::string& device)
{
  if ((_fd = open(device.c_str(), O_RDONLY | O_NONBLOCK)) == -1)
    throw (ControllerException("could not open device name: " + std::string(strerror(errno))));
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
  struct js_event	event;

  while ((ret = read(_fd, static_cast<void*>(&event), sizeof(event))) != -1)
  {
    event.type &= ~JS_EVENT_INIT;
    if (event.type == JS_EVENT_AXIS)
    {
      if (event.number == 25)
	;
      else
      {
	printf("Axis Id=%i Val=%i\n", event.number, event.value);
	if (event.number == 26)
	  _axisX = static_cast<int>(event.value);
	else if (event.number == 24)
	  _axisZ = static_cast<int>(event.value);
      }
    }
    if (event.type == JS_EVENT_BUTTON)
    {
      printf("Button Id=%i Val=%i\n", event.number, event.value);
    }
  }
  _rotation[1] += (static_cast<float>(_axisX) / AxisAbsoluteResolution) * 8.0f;
  _rotation[2] += (static_cast<float>(_axisZ) / AxisAbsoluteResolution) * - 8.0f;
}
