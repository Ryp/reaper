////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "AudioExport.h"

#include <fstream>

namespace Reaper::Audio
{
REAPER_AUDIO_API void test_alsa();
REAPER_AUDIO_API void test_alsa2();
REAPER_AUDIO_API void test_alsa3();
REAPER_AUDIO_API void test_mmap();
} // namespace Reaper::Audio
