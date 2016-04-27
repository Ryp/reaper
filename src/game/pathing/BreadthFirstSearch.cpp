////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "BreadthFirstSearch.h"

#include "core/EnumHelper.h"

#include <queue>
#include <iostream>

namespace pathing
{
    void computeBreadthFirstSearch(uvec2 goal, CellMap& graph)
    {
        std::queue<uvec2> frontier;
        uvec2 current;
        uvec2 next;

        Assert(goal < graph.size, "goal is out of bounds");
        Assert((graph[goal].bfs & to_underlying(NodeInfo::Pathable)) != 0, "goal is not pathable");

        frontier.push(goal);
        while (!frontier.empty())
        {
            current = frontier.front();
            frontier.pop();

            std::cout << "Frontier size = " << frontier.size();
            std::cout << ", Visiting " << current.x << "," << current.y << std::endl;

            // Visiting neighbors
            // x - 1
            if (current.x > 0)
            {
                next = uvec2(current.x - 1, current.y);
                if (graph[next].bfs & to_underlying(NodeInfo::Pathable) &&
                    !(graph[next].bfs & to_underlying(NodeInfo::Visited)))
                {
                    frontier.push(next);
                    graph[next].bfs |= to_underlying(NodeInfo::Visited) | to_underlying(NodeInfo::PlusX);
                }
            }

            // x + 1
            if (current.x + 1 < graph.size.x)
            {
                next = uvec2(current.x + 1, current.y);
                if (graph[next].bfs & to_underlying(NodeInfo::Pathable) &&
                    !(graph[next].bfs & to_underlying(NodeInfo::Visited)))
                {
                    frontier.push(next);
                    graph[next].bfs |= to_underlying(NodeInfo::Visited) | to_underlying(NodeInfo::MinusX);
                }
            }

            // y - 1
            if (current.y > 0)
            {
                next = uvec2(current.x, current.y - 1);
                if (graph[next].bfs & to_underlying(NodeInfo::Pathable) &&
                    !(graph[next].bfs & to_underlying(NodeInfo::Visited)))
                {
                    frontier.push(next);
                    graph[next].bfs |= to_underlying(NodeInfo::Visited) | to_underlying(NodeInfo::PlusY);
                }
            }

            // y + 1
            if (current.y + 1 < graph.size.y)
            {
                next = uvec2(current.x, current.y + 1);
                if (graph[next].bfs & to_underlying(NodeInfo::Pathable) &&
                    !(graph[next].bfs & to_underlying(NodeInfo::Visited)))
                {
                    frontier.push(next);
                    graph[next].bfs |= to_underlying(NodeInfo::Visited) | to_underlying(NodeInfo::MinusY);
                }
            }
        }
        // Remove any direction we set
        graph[goal].bfs &= ~ (to_underlying(NodeInfo::PlusX) | to_underlying(NodeInfo::MinusX) | to_underlying(NodeInfo::PlusY) | to_underlying(NodeInfo::MinusY));
    }
}
