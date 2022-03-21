////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <iostream>

namespace Reaper::Audio
{
REAPER_AUDIO_API void write_wav(std::ostream& out, const u8* raw_audio, u32 audio_size, u32 bit_depth, u32 sample_rate);
} // namespace Reaper::Audio
