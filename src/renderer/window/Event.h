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
    enum type
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
    enum type
    {
        Invalid,
        NUM_1,
        NUM_2,
        NUM_3,
        NUM_4,
        NUM_5,
        NUM_6,
        NUM_7,
        NUM_8,
        NUM_9,
        NUM_0,
        ESCAPE,
        ENTER,
        SPACE,
        ARROW_RIGHT,
        ARROW_LEFT,
        ARROW_DOWN,
        ARROW_UP,
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
            u8                    key_code; // FIXME internal
            bool                  press;
        } keypress;
    } message;
};

Event createResizeEvent(u32 width, u32 height);
Event createButtonEvent(Window::MouseButton::type button, bool press);
Event createKeyEvent(Window::KeyCode::type id, bool press, u8 key_code);

REAPER_RENDERER_API const char* get_mouse_button_string(MouseButton::type button);
REAPER_RENDERER_API const char* get_keyboard_key_string(KeyCode::type key);
} // namespace Reaper::Window
