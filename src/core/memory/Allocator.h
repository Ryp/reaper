////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

#include "core/BitTricks.h"
#include "core/CoreExport.h"

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

REAPER_CORE_API std::size_t alignOffset(std::size_t offset, std::size_t alignment);
