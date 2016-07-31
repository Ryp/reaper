#include "pch/stdafx.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "gamelogic/pathing/CostMap.h"

TEST_CASE("Cost Map", "[costmap]")
{
    int costMapSize = 100;

    SECTION("Multiple alloc/free")
    {
        CostMap map = createCostMap(costMapSize);

        CHECK(static_cast<u16>(map.costs[0]) == 0);

        destroyCostMap(map);
    }
}
