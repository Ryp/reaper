////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PathModule.h"

#include "core/EnumHelper.h"

#include <iostream>

#include "game/pathing/BreadthFirstSearch.h"
#include "game/map/MapDescriptor.h"

PathUpdater::PathUpdater(AbstractWorldUpdater* worldUpdater, MapInfo& mapInfo)
:   ModuleUpdater<PathModule>(worldUpdater)
,   _mapInfo(mapInfo)
{}

PathUpdater::~PathUpdater()
{}

void PathUpdater::update(float dt, ModuleAccessor<MovementModule> movementModuleAccessor)
{
    debugPathFinder();
}

void PathUpdater::createModule(EntityId id, const PathModuleDescriptor* /*descriptor*/)
{
    PathModule module;
    module.pathId = 0;
    module.pathSubId = 0;

    addModule(id, module);
}

void PathUpdater::computeConstructibleFlags()
{
    CellMap&    map = _mapInfo.getCells();
    uvec2       it;

    for (it.x = 0; it.x < map.size.x; ++it.x)
    {
        for (it.y = 0; it.y < map.size.y; ++it.y)
        {
            if (!(map[it].flags & CellFlags::Constructible))
                continue;

            // TODO more checks

            map[it].flags &= CellFlags::Constructible;
        }
    }
}

void PathUpdater::debugPathFinder()
{
    CellMap&    map = _mapInfo.getCells();
    uvec2       it;
    uvec2       goal(2, 2);

    for (it.x = 0; it.x < map.size.x; ++it.x)
    {
        for (it.y = 0; it.y < map.size.y; ++it.y)
        {
            if (map[it].flags & CellFlags::Pathable)
                map[it].bfs = to_underlying(pathing::NodeInfo::Pathable);
            else
                map[it].bfs = to_underlying(pathing::NodeInfo::None);
        }
    }

    pathing::computeBreadthFirstSearch(goal, map);

    for (it.y = 0; it.y < map.size.y; ++it.y)
    {
        for (it.x = 0; it.x < map.size.x; ++it.x)
        {
            if (map[it].bfs & to_underlying(pathing::NodeInfo::PlusX))
                std::cout << '>';
            else if (map[it].bfs & to_underlying(pathing::NodeInfo::MinusX))
                std::cout << '<';
            else if (map[it].bfs & to_underlying(pathing::NodeInfo::PlusY))
                std::cout << 'v';
            else if (map[it].bfs & to_underlying(pathing::NodeInfo::MinusY))
                std::cout << '^';
            else
                std::cout << '.';
        }
        std::cout << std::endl;
    }
    AssertUnreachable();
}
