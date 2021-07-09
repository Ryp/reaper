////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "gamelogic/ModuleUpdater.h"
#include "gamelogic/map/MapInfo.h"

#include "MovementModule.h"

struct PathModuleDescriptor : public ModuleDescriptor
{
    //     u32 placeHolder;
};

struct PathModule
{
    TDPath path;
    u32    pathId;
};

class PathUpdater : public ModuleUpdater<PathModule, PathModuleDescriptor>
{
    using parent_type = ModuleUpdater<PathModule, PathModuleDescriptor>;

public:
    PathUpdater(AbstractWorldUpdater* worldUpdater, MapInfo& mapInfo);
    ~PathUpdater();

public:
    void update(float dt, ModuleAccessor<MovementModule> movementModuleAccessor,
                ModuleAccessor<PositionModule> positionModuleAccessor);
    void createModule(EntityId id, const PathModuleDescriptor* descriptor) override;

private:
    void computeConstructibleFlags();
    void debugPathFinder();

private:
    MapInfo&            _mapInfo;
    std::vector<TDPath> _mainPaths;
};
