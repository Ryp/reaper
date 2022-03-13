////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#if defined(REAPER_USE_ALSA)
#    define ALSA_PCM_NEW_HW_PARAMS_API
#    include <alsa/asoundlib.h>
#endif

namespace Reaper
{
struct ReaperRoot;

struct AudioBackend
{
#if defined(REAPER_USE_ALSA)
    snd_pcm_t*        pcm_handle;
    snd_pcm_uframes_t period_size;
#endif

    bool enable_output;

    u32             frame_index;
    std::vector<u8> audio_buffer;
};

struct AudioConfig
{
    u32 sample_rate = 44100;
    u32 bit_depth = 32;
    u32 channel_count = 2;
};

REAPER_AUDIO_API AudioBackend create_audio_backend(ReaperRoot& root, const AudioConfig& config);
REAPER_AUDIO_API void         destroy_audio_backend(ReaperRoot& root, AudioBackend& backend);

REAPER_AUDIO_API void play_something(AudioBackend& backend, const AudioConfig& config);
REAPER_AUDIO_API void print_audio_backend_diagnostics();

REAPER_AUDIO_API void audio_execute_frame(ReaperRoot& root, AudioBackend& backend);
} // namespace Reaper
