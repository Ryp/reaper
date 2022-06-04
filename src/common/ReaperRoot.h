////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
struct Renderer;
class ILog;
struct AudioBackend;

struct ReaperRoot
{
    Renderer*     renderer;
    ILog*         log;
    AudioBackend* audio;
};
} // namespace Reaper
