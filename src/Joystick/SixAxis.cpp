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
  : AController(19, 27)
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
      if (event.number != 25)
      {
	//printf("Axis Id=%i Val=%i\n", event.number, event.value);
      }
      _axes[event.number] = static_cast<float>(event.value) / static_cast<float>(AxisAbsoluteResolution);
    }
    else if (event.type == JS_EVENT_BUTTON)
      _buttons[event.number].new_held = event.value != 0;
  }
  AController::update();
}
