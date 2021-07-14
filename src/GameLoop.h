////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
struct ReaperRoot;
struct VulkanBackend;

void execute_game_loop(ReaperRoot& root, VulkanBackend& backend);
} // namespace Reaper
