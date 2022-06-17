////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/RendererExport.h"

#include <core/Types.h>

namespace Reaper::Window
{
enum class EventType
{
    Invalid,
    Resize,
    ButtonPress,
    KeyPress,
    Close
};

namespace MouseButton
{
    enum type : u32
    {
        Invalid,
        Left,
        Middle,
        Right,
        // NOTE: XCB produces wheel events in pairs of pressed/released
        WheelUp,
        WheelDown,
        WheelLeft,
        WheelRight,
    };
}

namespace KeyCode
{
    enum type : u32
    {
        UNKNOWN,
        ESCAPE,
        ENTER,
        SPACE,
        RIGHT,
        LEFT,
        DOWN,
        UP,
        W,
        A,
        S,
        D,
    };
}

struct Event
{
    EventType type;
    union Message
    {
        struct Resize
        {
            u32 width;
            u32 height;
        } resize;
        struct ButtonPress
        {
            Window::MouseButton::type button;
            bool                      press;
        } buttonpress;
        struct KeyPress
        {
            Window::KeyCode::type key;
        } keypress;
    } message;
};

Event createResizeEvent(u32 width, u32 height);
Event createButtonEvent(Window::MouseButton::type button, bool press);
Event createKeyPressEvent(Window::KeyCode::type id);

REAPER_RENDERER_API const char* get_mouse_button_string(MouseButton::type button);
} // namespace Reaper::Window
