////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "core/VectorTypes.h"

#include "MapDescriptor.h"

using TDPath = std::vector<uvec2>;

struct Cell
{
    u8 flags;
    u8 bfs;
    //     u8vec2 pos;
    Cell()
        : flags(0)
        , bfs(0) /*, pos(0, 0)*/
    {}
};

template <typename T>
class Array2D
{
public:
    Array2D(uvec2 mapSize)
        : size(mapSize)
        , _cells(new Cell*[size.x])
    {
        for (u32 i = 0; i < size.x; ++i)
            _cells[i] = new Cell[size.y];
    }

    ~Array2D()
    {
        for (u32 i = 0; i < size.x; ++i)
            delete[] _cells[i];
        delete[] _cells;
    }

public:
    T*       operator[](u32 index) { return _cells[index]; }
    const T* operator[](u32 index) const { return _cells[index]; }
    T&       operator[](uvec2 index) { return _cells[index.x][index.y]; }
    const T& operator[](uvec2 index) const { return _cells[index.x][index.y]; }

public:
    const uvec2 size;

private:
    T** const _cells;
};

using CellMap = Array2D<Cell>;

class MapInfo
{
public:
    MapInfo();
    ~MapInfo();

    MapInfo(const MapInfo& other) = delete;
    MapInfo& operator=(const MapInfo& other) = delete;

public:
    void                          load(MapDescriptor* mapDescriptor);
    void                          unload();
    CellMap&                      getCells();
    const std::vector<MapAccess>& getMapAccesses() const;

private:
    std::vector<MapAccess> _accesses;
    CellMap*               _cells;
};
