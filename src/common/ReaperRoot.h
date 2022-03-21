////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
class GameLogic;
class ILog;
struct Renderer;
struct AudioBackend;

struct ReaperRoot
{
    GameLogic*    game;
    Renderer*     renderer;
    ILog*         log;
    AudioBackend* audio;
};
} // namespace Reaper
