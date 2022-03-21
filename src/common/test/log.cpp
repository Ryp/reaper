////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <doctest/doctest.h>

#include "common/DebugLog.h"
#include "common/Log.h"

namespace Reaper
{
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
} // namespace Reaper
