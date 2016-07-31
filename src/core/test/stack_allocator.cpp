#include "pch/stdafx.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "core/memory/StackAllocator.h"

TEST_CASE("Stack allocator", "[stack]")
{
    const std::size_t size = 512;

    SECTION("Alloc")
    {
        StackAllocator ator(size);

        void* ptr = nullptr;

        ptr = ator.alloc(42);

        CHECK(ptr != nullptr);

        ator.clear();
    }

    SECTION("Use marker")
    {
        struct Dummy
        {
            bool isUseful;
            char pid;
            float temp;
        };

        StackAllocator ator(size);
        StackAllocator  sa(1000);
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
