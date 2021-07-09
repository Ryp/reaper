////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Log.h"

namespace Reaper
{
void log_message(ILog* log, LogLevel level, const std::string& message)
{
    Assert(log != nullptr);

    if (log != nullptr)
        log->log(level, message);
}
} // namespace Reaper
