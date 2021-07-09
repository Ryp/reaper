////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "core/memory/StackAllocator.h"

TEST_CASE("Stack allocator")
{
    const std::size_t size = 512;

    SUBCASE("Alloc")
    {
        StackAllocator ator(size);

        void* ptr = nullptr;

        ptr = ator.alloc(42);

        CHECK(ptr != nullptr);

        ator.clear();
    }

    SUBCASE("Use marker")
    {
        struct Dummy
        {
            bool  isUseful;
            char  pid;
            float temp;
        };

        StackAllocator ator(size);
        StackAllocator sa(1000);
        Dummy*         a = nullptr;
        Dummy*         b = nullptr;
        Dummy*         c = nullptr;

        a = static_cast<Dummy*>(sa.alloc(sizeof(Dummy)));

        CHECK(a != nullptr);

        auto mk = sa.getMarker();

        CHECK(mk > 0);

        b = static_cast<Dummy*>(sa.alloc(sizeof(Dummy)));

        CHECK(b != nullptr);

        sa.freeToMarker(mk);

        CHECK(mk == sa.getMarker());

        c = static_cast<Dummy*>(sa.alloc(sizeof(Dummy)));

        CHECK(b == c);
    }
}
