////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "gamelogic/WorldUpdater.h"
#include "gamelogic/entitydb/EntityDb.h"

TEST_CASE("World Updater")
{
    EntityDb     db;
    WorldUpdater wu(&db);

    SUBCASE("Multiple alloc/free")
    {
        db.load();
        wu.load();

        for (int i = 0; i < 2000; ++i)
            wu.updateModules(0.15f);

        wu.unload();
        db.unload();
    }
}
