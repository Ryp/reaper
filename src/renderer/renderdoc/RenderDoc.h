////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
struct ReaperRoot;

namespace RenderDoc
{
    void start_integration(ReaperRoot& root);
    void stop_integration(ReaperRoot& root);
} // namespace RenderDoc
} // namespace Reaper
