////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace crashpad
{
class CrashpadClient;
}

namespace Reaper
{
struct CrashpadContext
{
    crashpad::CrashpadClient* client;
    bool                      is_started;
};

CrashpadContext create_crashpad_context();
void            destroy_crashpad_context(const CrashpadContext& crashpad_context);
} // namespace Reaper
