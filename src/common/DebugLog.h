////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Log.h"

#include <string>

class REAPER_COMMON_API DebugLog : public ILog
{
    public:
        DebugLog(LogLevel level = LogLevel::Debug);
        virtual ~DebugLog();

    public:
        virtual void log(LogLevel level, const std::string& message) override final;
        void setLogLevel(LogLevel level);

    private:
        LogLevel _logLevel;
};
