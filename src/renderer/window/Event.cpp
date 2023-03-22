////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Event.h"

#include <core/Assert.h>

namespace Reaper::Window
{
Event createResizeEvent(u32 width, u32 height)
{
    Event event = {};
    event.type = EventType::Resize;
    event.message.resize.width = width;
    event.message.resize.height = height;
    return event;
}

Event createButtonEvent(Window::MouseButton::type button, bool press)
{
    Event event = {};
    event.type = Window::EventType::ButtonPress;
    event.message.buttonpress.button = button;
    event.message.buttonpress.press = press;
    return event;
}

Event createKeyEvent(Window::KeyCode::type id, bool press, u8 key_code)
{
    Event event = {};
    event.type = press ? EventType::KeyPress : EventType::KeyRelease;
    event.message.keypress.key = id;
    event.message.keypress.key_code = key_code;
    return event;
}

const char* get_mouse_button_string(MouseButton::type button)
{
    switch (button)
    {
    case Window::MouseButton::Left:
        return "Left";
    case Window::MouseButton::Middle:
        return "Middle";
    case Window::MouseButton::Right:
        return "Right";
    case Window::MouseButton::WheelUp:
        return "WheelUp";
    case Window::MouseButton::WheelDown:
        return "WheelDown";
    case Window::MouseButton::WheelLeft:
        return "WheelLeft";
    case Window::MouseButton::WheelRight:
        return "WheelRight";
    case Window::MouseButton::Invalid:
    default:
        AssertUnreachable();
        return "Invalid";
    }
}
} // namespace Reaper::Window
