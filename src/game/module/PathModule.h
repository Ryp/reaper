////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_PATHMODULE_INCLUDED
#define REAPER_PATHMODULE_INCLUDED

#include "game/ModuleUpdater.h"
#include "game/map/MapInfo.h"

#include "MovementModule.h"

struct PathModuleDescriptor {
//     u32 placeHolder;
};

struct PathModule {
    TDPath path;
    u16 pathId;
    u16 pathSubId;
};

class PathUpdater : public ModuleUpdater<PathModule>
{
public:
    PathUpdater(AbstractWorldUpdater* worldUpdater, MapInfo& mapInfo);
    ~PathUpdater();

public:
    void update(float dt, ModuleAccessor<MovementModule> movementModuleAccessor);
    void createModule(EntityId id, PathModuleDescriptor const* descriptor);

private:
    void computeConstructibleFlags();
    void debugPathFinder();

private:
    MapInfo&            _mapInfo;
    std::vector<TDPath> _mainPaths;
};

#endif // REAPER_PATHMODULE_INCLUDED
