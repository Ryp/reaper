#include "pch/stdafx.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "core/memory/Allocator.h"

TEST_CASE("Offset alignment", "[align]")
{
    SECTION("Alignment16")
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
