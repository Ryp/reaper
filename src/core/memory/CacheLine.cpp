////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "CacheLine.h"

#if defined(REAPER_PLATFORM_LINUX)

#    include <unistd.h>

static size_t cacheLineSizeImpl()
{
    long lineSize = sysconf(_SC_LEVEL1_ICACHE_LINESIZE);

    Assert(lineSize != -1);
    return static_cast<size_t>(lineSize);
}

#elif defined(REAPER_PLATFORM_MACOSX)

#    include <sys/sysctl.h>

static size_t cacheLineSizeImpl()
{
    size_t lineSize = 0;
    size_t sizeOfLineSize = sizeof(lineSize);

    sysctlbyname("hw.cachelinesize", &lineSize, &sizeOfLineSize, 0, 0);
    return lineSize;
}

#elif defined(REAPER_PLATFORM_WINDOWS)

#    include <stdlib.h>
#    include <windows.h>
static size_t cacheLineSizeImpl()
{
    size_t                                lineSize = 0;
    DWORD                                 bufferSize = 0;
    DWORD                                 i = 0;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer = 0;

    GetLogicalProcessorInformation(0, &bufferSize);
    buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(bufferSize);
    GetLogicalProcessorInformation(&buffer[0], &bufferSize);

    for (i = 0; i != bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i)
    {
        if (buffer[i].Relationship == RelationCache && buffer[i].Cache.Level == 1)
        {
            lineSize = buffer[i].Cache.LineSize;
            break;
        }
    }

    free(buffer);
    return lineSize;
}

#else
#    error "cacheLineSize() not implemented for this platform"
#endif

size_t cacheLineSize()
{
    static size_t lineSize = cacheLineSizeImpl();
    return lineSize;
}
