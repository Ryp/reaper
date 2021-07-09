////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MapInfo.h"

#include "MapDescriptor.h"

MapInfo::MapInfo()
    : _cells(nullptr)
{}

MapInfo::~MapInfo()
{
    Assert(_cells == nullptr);
}

void MapInfo::load(MapDescriptor* mapDescriptor)
{
    Assert(_cells == nullptr);
    _cells = new CellMap(mapDescriptor->dimensions);

    Assert(mapDescriptor->dimensions <= uvec2(255, 255), "map is too big");

    uvec2 it;
    for (it.x = 0; it.x < _cells->size.x; ++it.x)
    {
        for (it.y = 0; it.y < _cells->size.y; ++it.y)
            (*_cells)[it].flags = CellFlags::Pathable | CellFlags::Constructible;
    }

    _accesses.resize(mapDescriptor->accesses.size());

    for (unsigned int i = 0; i < mapDescriptor->accesses.size(); ++i)
    {
        const MapAccess& access = mapDescriptor->accesses[i];

        Assert(access.entrance != access.exit, "entrance and exit are at the same position");
        Assert(access.entrance < mapDescriptor->dimensions, "entrance is outside map");
        Assert(access.exit < mapDescriptor->dimensions, "exit is outside map");

        (*_cells)[access.entrance].flags &= ~CellFlags::Constructible;
        (*_cells)[access.exit].flags &= ~CellFlags::Constructible;

        _accesses[i] = access;
    }
}

void MapInfo::unload()
{
    Assert(_cells != nullptr);
    delete _cells;
    _cells = nullptr;
}

CellMap& MapInfo::getCells()
{
    Assert(_cells != nullptr);
    return *_cells;
}

const std::vector<MapAccess>& MapInfo::getMapAccesses() const
{
    return _accesses;
}
