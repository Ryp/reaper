////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "DebugLog.h"

#include <iostream>

#include "core/EnumHelper.h"

namespace Reaper
{
DebugLog::DebugLog(LogLevel level)
    : m_logLevel(level)
{}

void DebugLog::log(LogLevel level, const std::string& message)
{
    if (to_underlying(level) > to_underlying(m_logLevel))
        return;

    switch (level)
    {
    case LogLevel::Error:
        std::cout << "  [error] ";
        break;
    case LogLevel::Warning:
        std::cout << "[warning] ";
        break;
    case LogLevel::Info:
        std::cout << "   [info] ";
        break;
    case LogLevel::Debug:
        std::cout << "  [debug] ";
        break;
    default:
        AssertUnreachable();
        break;
    }
    std::cout << message << std::endl;
}

void DebugLog::setLogLevel(LogLevel level)
{
    m_logLevel = level;
}
} // namespace Reaper
