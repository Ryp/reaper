////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "CostMap.h"

#include <algorithm>

template <typename T>
T clamp(T value, T lower, T upper)
{
    return std::min(std::max(value, lower), upper);
}

inline u16 toKey(const Index& index, u16 size)
{
    return index.x + size * index.y;
}

CostMap createCostMap(u16 size, CostType initialCost)
{
    CostMap map(size);
    map.costs = new CostType[size * size];
    for (int i = 0; i < size * size; ++i)
        map.costs[i] = initialCost;
    return map;
}

void destroyCostMap(CostMap& map)
{
    delete[] map.costs;
}

CostType evalCostMap(const CostMap& map, const Index& index)
{
    Assert(index.x < map.size);
    Assert(index.y < map.size);
    return map.costs[toKey(index, map.size)];
}

void paintCost(CostMap& map, Index position, Brush& brush)
{
    Index min, max;
    Index current;

    min.x = clamp<u16>(position.x - brush.radius, 0, map.size - 1);
    min.y = clamp<u16>(position.y - brush.radius, 0, map.size - 1);
    max.x = clamp<u16>(position.x + brush.radius, 0, map.size - 1);
    max.y = clamp<u16>(position.y + brush.radius, 0, map.size - 1);
    for (current.y = min.y; current.y <= max.y; ++current.y)
    {
        for (current.x = min.x; current.x <= max.x; ++current.x)
        {
            Index relative = current - position;
            //             float radius2 = brush.radius * brush.radius;
            const float distance = static_cast<float>(relative.x * relative.x + relative.y * relative.y);
            const float falloff = 1.f / (1.f + distance);
            const float hardness = 1.f - (1.f / (1.f + brush.hardness * 0.1f));

            map.costs[toKey(current, map.size)] = static_cast<CostType>(maximalCost() * hardness * falloff);
        }
    }
}
