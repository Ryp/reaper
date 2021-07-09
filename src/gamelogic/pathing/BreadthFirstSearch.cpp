////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "BreadthFirstSearch.h"

#include "core/EnumHelper.h"

#include <queue>

namespace pathing
{
void computeBreadthFirstSearch(uvec2 goal, CellMap& graph)
{
    std::queue<uvec2> frontier;
    uvec2             current;
    uvec2             next;

    Assert(goal < graph.size, "goal is out of bounds");
    Assert((graph[goal].bfs & to_underlying(NodeInfo::Pathable)) != 0, "goal is not pathable");

    graph[goal].bfs |= to_underlying(NodeInfo::IsGoal);

    frontier.push(goal);
    while (!frontier.empty())
    {
        current = frontier.front();
        frontier.pop();

        // Visiting neighbors
        // x - 1
        if (current.x > 0)
        {
            next = uvec2(current.x - 1, current.y);
            if (graph[next].bfs & to_underlying(NodeInfo::Pathable)
                && !(graph[next].bfs & to_underlying(NodeInfo::Visited)))
            {
                frontier.push(next);
                graph[next].bfs |= to_underlying(NodeInfo::Visited) | to_underlying(NodeInfo::PlusX);
            }
        }

        // x + 1
        if (current.x + 1 < graph.size.x)
        {
            next = uvec2(current.x + 1, current.y);
            if (graph[next].bfs & to_underlying(NodeInfo::Pathable)
                && !(graph[next].bfs & to_underlying(NodeInfo::Visited)))
            {
                frontier.push(next);
                graph[next].bfs |= to_underlying(NodeInfo::Visited) | to_underlying(NodeInfo::MinusX);
            }
        }

        // y - 1
        if (current.y > 0)
        {
            next = uvec2(current.x, current.y - 1);
            if (graph[next].bfs & to_underlying(NodeInfo::Pathable)
                && !(graph[next].bfs & to_underlying(NodeInfo::Visited)))
            {
                frontier.push(next);
                graph[next].bfs |= to_underlying(NodeInfo::Visited) | to_underlying(NodeInfo::PlusY);
            }
        }

        // y + 1
        if (current.y + 1 < graph.size.y)
        {
            next = uvec2(current.x, current.y + 1);
            if (graph[next].bfs & to_underlying(NodeInfo::Pathable)
                && !(graph[next].bfs & to_underlying(NodeInfo::Visited)))
            {
                frontier.push(next);
                graph[next].bfs |= to_underlying(NodeInfo::Visited) | to_underlying(NodeInfo::MinusY);
            }
        }
    }
    // Remove any direction we set
    graph[goal].bfs &= ~(to_underlying(NodeInfo::PlusX) | to_underlying(NodeInfo::MinusX)
                         | to_underlying(NodeInfo::PlusY) | to_underlying(NodeInfo::MinusY));
}

void buildPathFromBFS(uvec2 start, TDPath& path, const CellMap& graph)
{
    uvec2 current = start;

    Assert((graph[start].bfs & to_underlying(NodeInfo::Visited)) > 0, "goal unreachable from this start");
    Assert(path.empty());

    while (!(graph[current].bfs & to_underlying(NodeInfo::IsGoal)))
    {
        path.push_back(current);

        if (graph[current].bfs & to_underlying(NodeInfo::PlusX))
            ++current.x;
        else if (graph[current].bfs & to_underlying(NodeInfo::MinusX))
            --current.x;
        else if (graph[current].bfs & to_underlying(NodeInfo::PlusY))
            ++current.y;
        else if (graph[current].bfs & to_underlying(NodeInfo::MinusY))
            --current.y;

        Assert(current < graph.size);
    }

    path.push_back(current);
}
} // namespace pathing
