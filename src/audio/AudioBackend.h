////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

namespace Reaper
{
struct AudioBackend
{
    std::vector<u8> audio_buffer;
};

REAPER_AUDIO_API AudioBackend create_audio_backend();
REAPER_AUDIO_API void         destroy_audio_backend(AudioBackend& backend);
} // namespace Reaper
