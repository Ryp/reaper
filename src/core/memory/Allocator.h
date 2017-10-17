////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

#include "core/BitTricks.h"

// Remove harmless gcc warning
// warning: redundant redeclaration of 'void* operator new(std::size_t)' in same scope [-Wredundant-decls]
#ifdef REAPER_COMPILER_GCC
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

void* operator new(std::size_t size);
void* operator new[](std::size_t size);
void  operator delete(void* mem) noexcept;
void  operator delete(void* mem, std::size_t) noexcept;
void  operator delete[](void* mem) noexcept;
void  operator delete[](void* mem, std::size_t) noexcept;

#ifdef REAPER_COMPILER_GCC
#    pragma GCC diagnostic pop
#endif

class REAPER_CORE_API AbstractAllocator
{
public:
    virtual ~AbstractAllocator() {}
    virtual void* alloc(std::size_t sizeBytes) = 0;
};

template <typename T, typename... Args>
T* placementNew(AbstractAllocator& allocator, Args&&... args)
{
    T* ptr = nullptr;

    ptr = static_cast<T*>(allocator.alloc(sizeof(T)));
    Assert(ptr != nullptr);
    return new (ptr) T(args...);
}

inline std::size_t alignOffset(std::size_t offset, std::size_t alignment)
{
    Assert(isPowerOfTwo(alignment), "npot alignment");
    return ((offset - 1) & ~(alignment - 1)) + alignment;
}

// User-defined literals
// Here we assume kB = kiB = 1024 bytes
constexpr std::size_t operator"" _kB(unsigned long long int sizeInkB)
{
    return sizeInkB << 10;
}

constexpr std::size_t operator"" _MB(unsigned long long int sizeInMB)
{
    return sizeInMB << 20;
}
