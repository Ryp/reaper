////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Log.h"

void log_message(ILog* log, LogLevel level, const std::string& message)
{
    Assert(log != nullptr);

    if (log)
        log->log(level, message);
}

