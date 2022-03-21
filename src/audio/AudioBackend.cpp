////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "AudioBackend.h"

namespace Reaper
{
AudioBackend create_audio_backend()
{
    AudioBackend backend = {};

    return backend;
}

void destroy_audio_backend(AudioBackend& backend)
{
    backend.audio_buffer.clear();
}
} // namespace Reaper
