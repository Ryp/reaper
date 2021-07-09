////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "core/BitTricks.h"
#include "core/VectorTypes.h"

struct MapAccess
{
    uvec2 entrance;
    uvec2 exit;
};

enum CellFlags
{
    None = 0,
    Constructible = bit(1),
    Pathable = bit(2)
};

struct MapDescriptor
{
    std::vector<MapAccess> accesses;
    uvec2                  dimensions;
};
