////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper::Window
{
enum class EventType
{
    Invalid,
    Resize,
    KeyPress,
    Close
};

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
};

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
        struct KeyPress
        {
            u32 keyID; // UNUSED
        } keypress;
    } message;
};

Event createResizeEvent(u32 width, u32 height);
Event createKeyPressEvent(u32 id);
} // namespace Reaper::Window
