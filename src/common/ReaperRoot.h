////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_ROOT_INCLUDED
#define REAPER_ROOT_INCLUDED

class GameLogic;
class VulkanRenderer;
class ILog;

struct ReaperRoot
{
    GameLogic*      game;
    VulkanRenderer* renderer;
    ILog*           log;
};

#endif // REAPER_ROOT_INCLUDED

