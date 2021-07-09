////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Allocator.h"

// NOTE the deleter is NOT implemented
template <typename T>
class UniquePtr
{
public:
    template <typename... Args>
    UniquePtr(AbstractAllocator& allocator, Args&&... args)
        : _ptr(placementNew<T>(allocator, args...))
    {}
    ~UniquePtr() { _ptr->~T(); }

    UniquePtr(const UniquePtr& other) = delete;
    UniquePtr& operator=(const UniquePtr& other) = delete;

public:
    // TODO implement const operators
    T* get() { return _ptr; };
    T* operator->() { return _ptr; }
    T& operator*() { return *_ptr; }

private:
    T* const _ptr;
};
