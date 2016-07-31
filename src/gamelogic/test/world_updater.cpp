#include "pch/stdafx.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "gamelogic/WorldUpdater.h"
#include "gamelogic/entitydb/EntityDb.h"

TEST_CASE("World Updater", "[worldupdater]")
{
    EntityDb        db;
    WorldUpdater    wu(&db);

    SECTION("Multiple alloc/free")
    {
        db.load();
        wu.load();

        for (int i = 0; i < 2000; ++i)
            wu.updateModules(0.15f);

        wu.unload();
        db.unload();
    }
}
