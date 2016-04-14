////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_ALLOCATOR_INCLUDED
#define REAPER_ALLOCATOR_INCLUDED

#include <cstdint>

// Remove harmless gcc warning
// warning: redundant redeclaration of 'void* operator new(std::size_t)' in same scope [-Wredundant-decls]
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"

void* operator new(std::size_t size);
void* operator new[](std::size_t size);
void operator delete(void* mem) noexcept;
void operator delete(void* mem, std::size_t) noexcept;
void operator delete[](void* mem) noexcept;
void operator delete[](void* mem, std::size_t) noexcept;

#pragma GCC diagnostic pop

class AbstractAllocator
{
public:
    virtual ~AbstractAllocator() {}
    virtual void* alloc(std::size_t sizeBytes) = 0;
};

template <typename T, typename... Args>
T* placementNew(AbstractAllocator& allocator, Args&&... args)
{
    T*  ptr = nullptr;

    ptr = static_cast<T*>(allocator.alloc(sizeof(T)));
    Assert(ptr != nullptr);
    return new(ptr) T(args...);
}

#endif // REAPER_ALLOCATOR_INCLUDED
