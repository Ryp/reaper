////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_COSTMAP_INCLUDED
#define REAPER_COSTMAP_INCLUDED

#include "core/Types.h"

using CostType = u8;

// constexpr CostType unusableCost() { return -1; }
constexpr CostType unusableCost() { return 0; }
constexpr CostType minimalCost() { return 1; }
constexpr CostType maximalCost() { return (1 << (sizeof(CostType) * 8)) - 1; }

struct CostMap {
    const u16   size;
    CostType*   costs;
    CostMap(u16 sSize) : size(sSize) {}
};

struct Index {
    u16 x;
    u16 y;
    Index(u16 X = 0, u16 Y = 0) : x(X), y(Y) {}
    bool operator==(Index& other) const { return x == other.x && y == other.y; }
    Index operator-(Index& other) const { return Index(x - other.x, y - other.y); }
};

CostMap createCostMap(u16 size, CostType initialCost = minimalCost());
void    destroyCostMap(CostMap& map);
CostType evalCostMap(const CostMap& map, const Index& index);

struct Brush {
    float   hardness;
    u16     radius;
};

void    paintCost(CostMap& map, Index position, Brush& brush);

#endif // REAPER_COSTMAP_INCLUDED
