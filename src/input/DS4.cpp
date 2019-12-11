////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "DS4.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#include <fcntl.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <unistd.h>

const float DS4::AxisDeadzone = 0.12f;

DS4::DS4(const std::string& device)
    : AbstractController(TotalButtonsNumber, TotalAxesNumber)
    , _device(device)
    , _connected(connect())
{
    AbstractController::reset();
}

void DS4::update()
{
    float           val;
    struct js_event event;

    if (!_connected)
        return;
    while (read(_fd, static_cast<void*>(&event), sizeof(event)) != -1)
    {
        event.type &= ~JS_EVENT_INIT;
        if (event.type == JS_EVENT_AXIS)
        {
            val = static_cast<float>(event.value) / static_cast<float>(AxisAbsoluteResolution);
            axes[event.number] = ((fabs(val) < AxisDeadzone) ? (0.0f) : (val));
        }
        else if (event.type == JS_EVENT_BUTTON)
            buttons[event.number].new_held = event.value != 0;
    }
    AbstractController::update();
}

void DS4::destroy()
{
    if (_connected && (close(_fd) == -1))
        AssertUnreachable();
}

bool DS4::connect()
{
    char deviceName[256];

    _fd = open(_device.c_str(), O_RDONLY | O_NONBLOCK);
    if (_fd == -1)
        return false;
    Assert(ioctl(_fd, JSIOCGNAME(256), deviceName) != -1, "could not retrieve device name");
    Assert(std::string("Sony Interactive Entertainment Wireless Controller") == deviceName, "not a DS4 controller");
    update();
    AbstractController::reset();
    return true;
}