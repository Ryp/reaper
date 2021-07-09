////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "core/memory/BuddyAllocator.h"

#include <deque>
#include <random>

TEST_CASE("Buddy allocator")
{
    const std::size_t buddyAtorSize = 512;
    BuddyAllocator    ator(buddyAtorSize);

    SUBCASE("Alloc and free")
    {
        void* ptr = nullptr;

        ptr = ator.alloc(42);

        CHECK(ptr != nullptr);

        ator.free(ptr, 42);
    }

    SUBCASE("Alloc char and write to it")
    {
        char* ptr = nullptr;

        ptr = static_cast<char*>(ator.alloc(sizeof(char)));

        CHECK(ptr != nullptr);

        *ptr = 'a';

        ator.free(ptr, sizeof(char));
    }

    SUBCASE("Alloc int and write to it")
    {
        int* ptr = nullptr;

        ptr = static_cast<int*>(ator.alloc(sizeof(int)));

        CHECK(ptr != nullptr);

        *ptr = 0xDEADC0DE;

        ator.free(ptr, sizeof(int));
    }

    SUBCASE("Multiple alloc/free")
    {
        void* ptrA = nullptr;
        void* ptrB = nullptr;
        void* ptrC = nullptr;

        ptrA = ator.alloc(66);
        ptrB = ator.alloc(4);
        ptrC = ator.alloc(63);

        CHECK(ptrA != nullptr);
        CHECK(ptrB != nullptr);
        CHECK(ptrC != nullptr);

        ator.free(ptrB, 4);
        ator.free(ptrA, 66);
        ator.free(ptrC, 63);
    }

    SUBCASE("Alloc/free all sizes")
    {
        for (std::size_t size = 1; size <= buddyAtorSize; ++size)
        {
            void* ptr = ator.alloc(size);

            CHECK(ptr != nullptr);
            ator.free(ptr, size);
        }
    }

    SUBCASE("Random allocs")
    {
        struct Alloc
        {
            void*       ptr;
            std::size_t size;
        };

        std::deque<Alloc> ptrs;
        std::size_t       maxSize = buddyAtorSize / 3;
        std::size_t       maxAllocs = 16;
        u32               iterations = 10;
        std::size_t       totalSize = 0;

        for (u32 i = 0; i < iterations; ++i)
        {
            std::size_t size = std::rand() % (maxSize + 1);

            // Allocate a block of random size or free one if no space available
            if (size + totalSize <= maxSize && ptrs.size() < maxAllocs)
            {
                totalSize += size;

                Alloc alloc = {ator.alloc(size), size};

                CHECK(alloc.ptr != nullptr);

                ptrs.push_back(alloc);
            }
            else
            {
                Alloc alloc = ptrs.front();

                ator.free(alloc.ptr, alloc.size);

                totalSize -= alloc.size;
                ptrs.pop_front();
            }
        }

        // Free remaining blocks
        for (auto& alloc : ptrs)
            ator.free(alloc.ptr, alloc.size);
    }

    SUBCASE("Multiple allocators with different sizes")
    {
        BuddyAllocator ba1(128, 64);
        BuddyAllocator ba2(512, 64);
        BuddyAllocator ba3(4096, 64);
        BuddyAllocator ba4(4096, 2);

        ba1.free(ba1.alloc(64), 64);
        ba1.free(ba1.alloc(1), 1);

        ba2.free(ba2.alloc(256), 256);
        ba2.free(ba2.alloc(1), 1);

        ba3.free(ba3.alloc(2048), 2048);
        ba3.free(ba3.alloc(1), 1);

        ba4.free(ba4.alloc(2048), 2048);
        ba4.free(ba4.alloc(1), 1);
    }
}
