////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace google_breakpad
{
class ExceptionHandler;
}

namespace Reaper
{
struct BreakpadContext
{
    google_breakpad::ExceptionHandler* handler;
};

BreakpadContext create_breakpad_context();
void            destroy_breakpad_context(const BreakpadContext& breakpad_context);
} // namespace Reaper
