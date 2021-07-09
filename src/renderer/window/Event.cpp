////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Event.h"

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

Event createKeyPressEvent(u32 id)
{
    Event event = {};
    event.type = EventType::KeyPress;
    event.message.keypress.keyID = id;
    return event;
}
} // namespace Reaper::Window
