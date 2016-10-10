////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include "doctest/doctest.h"

#include "gamelogic/pathing/CostMap.h"

TEST_CASE("Cost Map")
{
    int costMapSize = 100;

    SUBCASE("Multiple alloc/free")
    {
        CostMap map = createCostMap(costMapSize);

        CHECK(static_cast<u16>(map.costs[0]) == 0);

        destroyCostMap(map);
    }
}
