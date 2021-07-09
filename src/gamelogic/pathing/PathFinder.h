////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <iostream>
#include <list>
#include <map>

#include "CostMap.h"

#define NODE_BLOCKED (1 << 5)

using t_path = std::list<Index>;

struct t_node
{
    Index    parent;
    CostType loc_cost;
    CostType mvt_cost;
    CostType tot_cost;
};

using t_nodelist = std::map<int, t_node>;

class PathFinder
{
public:
    PathFinder() = default;

    /**
     * Find shortest path between two position on a map
     * Note: this algorithm doesn't modify any of its arguments passed
     * @param map - map containing movement costs for each cell
     * @param size - size of the map
     * @param start - starting point
     * @param dest - destination point
     * @return true if a path was found (recover it with getPath function)
     */
    bool findPath(const CostMap& map, const Index& size, const Index& start, const Index& dest);
    /**
     * Return path under the form of a std::list of Index
     * Note: path doesn't include the start position
     * @return path
     */
    t_path getPath();

private:
    int   getManhattanDistance(const Index& a, const Index& b);
    Index getBestOpenPos();
    void  addToClosedList(const Index& n);
    void  addAdjacentNodes(const Index& n);
    int   toKey(const Index& index) const;
    Index toIndex(const int& key) const;
    bool  isOpen(const Index& n);
    bool  isClosed(const Index& n);
    void  buildPath();

    CostMap    _map;
    Index      _map_size;
    Index      _start;
    Index      _dest;
    t_nodelist _open;
    t_nodelist _closed;
    t_path     _path;
    bool       _isFound;
};
