////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_CACHELINE_INCLUDED
#define REAPER_CACHELINE_INCLUDED

// The main function should assert the equality between the runtime and compiletime
// values.

// compile-time
#define REAPER_CACHELINE_SIZE 64

// runtime
REAPER_CORE_API size_t cacheLineSize();

#endif // REAPER_CACHELINE_INCLUDED
