////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "common/Log.h"
#include "common/DebugLog.h"

TEST_CASE("Log")
{
    ReaperRoot root = {};

    root.log = new DebugLog;

    log_debug(root, "Verbose");
    log_info(root, "NotSoVerbose");
    log_warning(root, "YouMayHaveDoneSomethingWrong");
    log_error(root, "FUBAR");

    delete root.log;
    root.log = nullptr;
}

