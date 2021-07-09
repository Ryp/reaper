////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "core/memory/CacheLine.h"

TEST_CASE("Cache line size")
{
    CHECK(cacheLineSize() == REAPER_CACHELINE_SIZE);
}
