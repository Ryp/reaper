////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "core/memory/Allocator.h"

TEST_CASE("Offset alignment")
{
    SUBCASE("Basic")
    {
        CHECK(0 == alignOffset(0, 16));
        CHECK(16 == alignOffset(1, 16));

        CHECK(4 == alignOffset(3, 4));
        CHECK(4 == alignOffset(4, 4));
        CHECK(5 == alignOffset(5, 1));
    }

    SUBCASE("Alignment16")
    {
        const std::size_t alignmentMultiples = 4;
        const std::size_t alignment = 16;

        CHECK(0 == alignOffset(0, alignment));

        for (std::size_t base = 0; base < alignmentMultiples; ++base)
        {
            std::size_t desiredAlignedOffset = (base + 1) * alignment;

            for (std::size_t i = 1; i <= alignment; ++i)
            {
                CHECK(desiredAlignedOffset == alignOffset(base * alignment + i, alignment));
            }
        }
    }
}
