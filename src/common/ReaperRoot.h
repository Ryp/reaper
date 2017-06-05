////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

class GameLogic;
class ILog;
struct Renderer;
struct AssetCache;

struct ReaperRoot
{
    GameLogic*  game;
    Renderer*   renderer;
    ILog*       log;
    AssetCache* assetCache;
};
