////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
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
