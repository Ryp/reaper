////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Debugger.h"

#if defined(REAPER_PLATFORM_WINDOWS)

// Does not check for remote debugger
bool isInDebugger()
{
    return IsDebuggerPresent() == TRUE;
}

#elif defined(REAPER_PLATFORM_LINUX)

#    include <cstdlib>
#    include <fcntl.h>
#    include <string.h>
#    include <sys/stat.h>
#    include <unistd.h>

bool isInDebugger()
{
    static bool isSet = false;
    static bool gdbPresent;

    if (isSet)
        return gdbPresent;

    char buf[1024];

    int status_fd = open("/proc/self/status", O_RDONLY);
    if (status_fd == -1)
        return 0;

    ssize_t num_read = read(status_fd, buf, sizeof(buf));

    if (num_read > 0)
    {
        static const char TracerPid[] = "TracerPid:";
        char*             tracer_pid;

        buf[num_read] = 0;
        tracer_pid = strstr(buf, TracerPid);
        if (tracer_pid)
            gdbPresent = std::atoi(tracer_pid + sizeof(TracerPid) - 1) > 0;
    }
    isSet = true;
    return gdbPresent;
}

#elif defined(REAPER_PLATFORM_MACOSX)

#    include <assert.h>
#    include <stdbool.h>
#    include <sys/sysctl.h>
#    include <sys/types.h>
#    include <unistd.h>

/**
 * From https://developer.apple.com/library/mac/qa/qa1361/_index.html
 * Returns true if the current process is being debugged (either
 * running under the debugger or has a debugger attached post facto).
 */
bool isInDebugger()
{
    int               junk;
    int               mib[4];
    struct kinfo_proc info;
    size_t            size;

    // Initialize the flags so that, if sysctl fails for some bizarre
    // reason, we get a predictable result.
    info.kp_proc.p_flag = 0;

    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    // Call sysctl.
    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    assert(junk == 0);

    // We're being debugged if the P_TRACED flag is set.
    return ((info.kp_proc.p_flag & P_TRACED) != 0);
}

#else

#    error "debugger detection not available!"

#endif
