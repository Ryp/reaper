////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_MAPDESCRIPTOR_INCLUDED
#define REAPER_MAPDESCRIPTOR_INCLUDED

#include <vector>

#include "core/VectorTypes.h"
#include "core/BitTricks.h"

struct MapAccess {
    uvec2 entrance;
    uvec2 exit;
};

enum CellFlags {
    None = 0,
    Constructible = bit(1),
    Pathable = bit(2)
};

struct MapDescriptor {
    std::vector<MapAccess> accesses;
    uvec2 dimensions;
};

#endif // REAPER_MAPDESCRIPTOR_INCLUDED
