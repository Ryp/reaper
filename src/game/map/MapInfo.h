////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_MAPINFO_INCLUDED
#define REAPER_MAPINFO_INCLUDED

#include <vector>

#include "core/VectorTypes.h"

using TDPath = std::vector<uvec2>;

struct Cell {
    u8 flags;
    u8 bfs;
//     u8vec2 pos;
    Cell() : flags(0), bfs(0)/*, pos(0, 0)*/ {}
};

class CellMap
{
public:
    CellMap(uvec2 mapSize)
    : size(mapSize)
    , _cells(new Cell*[size.x])
    {
        for (u32 i = 0; i < size.x; ++i)
            _cells[i] = new Cell[size.y];
    }

    ~CellMap()
    {
        for (u32 i = 0; i < size.x; ++i)
            delete [] _cells[i];
        delete [] _cells;
    }

public:
    Cell* operator[](u32 index) { return _cells[index]; }
    const Cell* operator[](u32 index) const { return _cells[index]; }
    Cell& operator[](uvec2 index) { return _cells[index.x][index.y]; }
    const Cell& operator[](uvec2 index) const { return _cells[index.x][index.y]; }

public:
    const uvec2 size;

private:
    Cell** const    _cells;
};

struct MapDescriptor;

class MapInfo
{
public:
    MapInfo();
    ~MapInfo();

    MapInfo(const MapInfo &other) = delete;
    MapInfo &operator=(const MapInfo &other) = delete;

public:
    void load(MapDescriptor* mapDescriptor);
    void unload();
    CellMap& getCells();

private:
    CellMap*    _cells;
};

#endif // REAPER_MAPINFO_INCLUDED
