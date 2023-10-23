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
    return Event{.type = Window::EventType::Resize,
                 .message = {.resize = {
                                 .width = width,
                                 .height = height,
                             }}};
}

Event createButtonEvent(Window::MouseButton::type button, bool press)
{
    return Event{.type = Window::EventType::MouseButton,
                 .message = {.mouse_button = {
                                 .button = button,
                                 .press = press,
                             }}};
}

Event createMouseWheelEvent(i32 x_delta, i32 y_delta)
{
    return Event{.type = Window::EventType::MouseWheel,
                 .message = {.mouse_wheel = {
                                 .x_delta = x_delta,
                                 .y_delta = y_delta,
                             }}};
}

Event createKeyEvent(Window::KeyCode::type id, bool press, u32 internal_key_code)
{
    return Event{.type = Window::EventType::KeyPress,
                 .message = {.keypress = {
                                 .key = id,
                                 .press = press,
                                 .internal_key_code = internal_key_code,
                             }}};
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

const char* get_keyboard_key_string(KeyCode::type key)
{
    switch (key)
    {
    case Window::KeyCode::NUM_1:
        return "NUM_1";
    case Window::KeyCode::NUM_2:
        return "NUM_2";
    case Window::KeyCode::NUM_3:
        return "NUM_3";
    case Window::KeyCode::NUM_4:
        return "NUM_4";
    case Window::KeyCode::NUM_5:
        return "NUM_5";
    case Window::KeyCode::NUM_6:
        return "NUM_6";
    case Window::KeyCode::NUM_7:
        return "NUM_7";
    case Window::KeyCode::NUM_8:
        return "NUM_8";
    case Window::KeyCode::NUM_9:
        return "NUM_9";
    case Window::KeyCode::NUM_0:
        return "NUM_0";
    case Window::KeyCode::ESCAPE:
        return "ESCAPE";
    case Window::KeyCode::ENTER:
        return "ENTER";
    case Window::KeyCode::SPACE:
        return "SPACE";
    case Window::KeyCode::ARROW_RIGHT:
        return "ARROW_RIGHT";
    case Window::KeyCode::ARROW_LEFT:
        return "ARROW_LEFT";
    case Window::KeyCode::ARROW_DOWN:
        return "ARROW_DOWN";
    case Window::KeyCode::ARROW_UP:
        return "ARROW_UP";
    case Window::KeyCode::W:
        return "W";
    case Window::KeyCode::A:
        return "A";
    case Window::KeyCode::S:
        return "S";
    case Window::KeyCode::D:
        return "D";
    case Window::KeyCode::Invalid:
    default:
        AssertUnreachable();
        return "Invalid";
    }
}
} // namespace Reaper::Window
