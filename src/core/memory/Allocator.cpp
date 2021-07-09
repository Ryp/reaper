////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Allocator.h"

#include <cstdlib>
// #include <iostream>

/*
void* operator new(std::size_t size)
{
//     std::cout << "new(" << size << ")" << std::endl;
    return malloc(size * sizeof(char));
}

void* operator new[](std::size_t size)
{
//     std::cout << "new[](" << size << ")" << std::endl;
    return malloc(size * sizeof(char));
}

void operator delete(void* mem) noexcept
{
//     std::cout << "delete(" << mem << ")" << std::endl;
    free(mem);
}

void operator delete(void* mem, std::size_t) noexcept
{
//     std::cout << "delete(" << mem << ")" << std::endl;
    free(mem);
}

void operator delete[](void* mem) noexcept
{
//     std::cout << "delete[](" << mem << ")" << std::endl;
    free(mem);
}

void operator delete[](void* mem, std::size_t) noexcept
{
//     std::cout << "delete[](" << mem << ")" << std::endl;
    free(mem);
}
*/
