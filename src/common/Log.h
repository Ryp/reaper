////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CommonExport.h"
#include "ReaperRoot.h"

#include <fmt/format.h>

namespace Reaper
{
enum class LogLevel
{
    Error,
    Warning,
    Info,
    Debug
};

class REAPER_COMMON_API ILog
{
public:
    virtual ~ILog() {}
    virtual void log(LogLevel level, const std::string& message) = 0;
};

REAPER_COMMON_API void log_message(ILog* log, LogLevel level, const std::string& message);

template <typename... Args>
void log_debug(ReaperRoot& root, const char* format, const Args&... args)
{
    log_message(root.log, LogLevel::Debug, fmt::vformat(format, fmt::make_format_args(args...)));
}

template <typename... Args>
void log_info(ReaperRoot& root, const char* format, const Args&... args)
{
    log_message(root.log, LogLevel::Info, fmt::vformat(format, fmt::make_format_args(args...)));
}

template <typename... Args>
void log_warning(ReaperRoot& root, const char* format, const Args&... args)
{
    log_message(root.log, LogLevel::Warning, fmt::vformat(format, fmt::make_format_args(args...)));
}

template <typename... Args>
void log_error(ReaperRoot& root, const char* format, const Args&... args)
{
    log_message(root.log, LogLevel::Error, fmt::vformat(format, fmt::make_format_args(args...)));
}
} // namespace Reaper
