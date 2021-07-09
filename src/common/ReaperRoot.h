////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
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
} // namespace Reaper
