////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "DebugLog.h"

#include <iostream>

#include "core/EnumHelper.h"

DebugLog::DebugLog(LogLevel level)
: _logLevel(level)
{}

DebugLog::~DebugLog()
{}

void DebugLog::log(LogLevel level, const std::string& message)
{
    if (to_underlying(level) > to_underlying(_logLevel))
        return;

    switch (level)
    {
        case LogLevel::Error:
            std::cout << "  [error] "; break;
        case LogLevel::Warning:
            std::cout << "[warning] "; break;
        case LogLevel::Info:
            std::cout << "   [info] "; break;
        case LogLevel::Debug:
            std::cout << "  [debug] "; break;
        default:
            AssertUnreachable();
            break;
    }
    std::cout << message << std::endl;
}

void DebugLog::setLogLevel(LogLevel level)
{
    _logLevel = level;
}

