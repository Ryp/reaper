////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PathModule.h"

#include "core/EnumHelper.h"

#include <iostream>

#include "gamelogic/map/MapDescriptor.h"
#include "gamelogic/pathing/BreadthFirstSearch.h"

PathUpdater::PathUpdater(AbstractWorldUpdater* worldUpdater, MapInfo& mapInfo)
    : parent_type(worldUpdater)
    , _mapInfo(mapInfo)
{}

PathUpdater::~PathUpdater()
{}

void PathUpdater::update(float /*dt*/, ModuleAccessor<MovementModule> movementModuleAccessor,
                         ModuleAccessor<PositionModule> positionModuleAccessor)
{
    CellMap& map = _mapInfo.getCells();

    {
        uvec2 it;
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
    }

    const u32 accessIndex = 0;
    uvec2     goal = _mapInfo.getMapAccesses()[accessIndex].exit;
    TDPath    path;

    pathing::computeBreadthFirstSearch(goal, map);
    pathing::buildPathFromBFS(_mapInfo.getMapAccesses()[accessIndex].entrance, path, map);

    for (auto& it : _modules)
    {
        auto&          pathInstance = it.second;
        const EntityId entityId = it.first;

        MovementModule* movementModule = movementModuleAccessor[entityId];
        if (movementModule->path.empty())
        {
            uvec2           startPos = path.front();
            PositionModule* positionModule = positionModuleAccessor[entityId];

            // FIXME apply node spacing and offset
            positionModule->position = glm::fvec3(startPos.x, startPos.y, 0.f) /* * nodeSpacing*/;
            pathInstance.path = path;

            MovementUpdater::buildAIPath(movementModule, path);
        }
    }

    //     for (it.y = 0; it.y < map.size.y; ++it.y)
    //     {
    //         for (it.x = 0; it.x < map.size.x; ++it.x)
    //         {
    //             if (map[it].bfs & to_underlying(pathing::NodeInfo::PlusX))
    //                 std::cout << '>';
    //             else if (map[it].bfs & to_underlying(pathing::NodeInfo::MinusX))
    //                 std::cout << '<';
    //             else if (map[it].bfs & to_underlying(pathing::NodeInfo::PlusY))
    //                 std::cout << 'v';
    //             else if (map[it].bfs & to_underlying(pathing::NodeInfo::MinusY))
    //                 std::cout << '^';
    //             else
    //                 std::cout << '.';
    //         }
    //         std::cout << std::endl;
    //     }
    //     AssertUnreachable();
}

void PathUpdater::createModule(EntityId id, const PathModuleDescriptor* /*descriptor*/)
{
    PathModule module;
    module.pathId = 0;

    addModule(id, module);
}

void PathUpdater::computeConstructibleFlags()
{
    CellMap& map = _mapInfo.getCells();
    uvec2    it;

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
