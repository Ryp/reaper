////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "core/BitTricks.h"

#include "gamelogic/map/MapInfo.h"

namespace pathing
{
enum class NodeInfo : u8
{
    None = 0,
    Pathable = bit(1),
    Visited = bit(2),
    // Directions
    PlusX = bit(3),
    MinusX = bit(4),
    PlusY = bit(5),
    MinusY = bit(6),
    // Special
    IsGoal = bit(7)
};

// Compute a basic Breadth First Search without any early exit to make sure
// every reachable node is covered.
// Don't forget to clear any flags set by a previous call of this function
void computeBreadthFirstSearch(uvec2 goal, CellMap& graph);

void buildPathFromBFS(uvec2 start, TDPath& path, const CellMap& graph);
} // namespace pathing
