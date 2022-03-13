////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "AudioBackend.h"

#include "common/Log.h"
#include "core/Profile.h"

#include <cmath>
#include <fmt/format.h>

namespace Reaper
{
AudioBackend create_audio_backend(ReaperRoot& root, const AudioConfig& config)
{
    REAPER_PROFILE_SCOPE("Audio", MP_BLUE);
    log_info(root, "audio: creating backend");

    AudioBackend backend = {};
    backend.frame_index = 0;
    backend.enable_output = false;

#if defined(REAPER_USE_ALSA)
    snd_pcm_format_t alsa_format = SND_PCM_FORMAT_UNKNOWN;

    if (config.bit_depth == 32)
        alsa_format = SND_PCM_FORMAT_S32_LE;

    Assert(alsa_format != SND_PCM_FORMAT_UNKNOWN);

    /* Open PCM device for playback. */
    int rc;
    rc = snd_pcm_open(&backend.pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    Assert(rc >= 0, fmt::format("unable to open pcm device: {}", snd_strerror(rc)));

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(backend.pcm_handle, hw_params);

    snd_pcm_hw_params_set_access(backend.pcm_handle, hw_params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED); // Interleaved channel data (ex: LRLRLR for stereo)
    snd_pcm_hw_params_set_format(backend.pcm_handle, hw_params, alsa_format);
    snd_pcm_hw_params_set_channels(backend.pcm_handle, hw_params, config.channel_count);

    int dir; // FIXME ?

    u32 adjusted_sample_rate = config.sample_rate;
    snd_pcm_hw_params_set_rate_near(backend.pcm_handle, hw_params, &adjusted_sample_rate, &dir);
    // Assert(adjusted_sample_rate == config.sample_rate); FIXME

    backend.period_size = 128; // FIXME
    snd_pcm_hw_params_set_period_size_near(backend.pcm_handle, hw_params, &backend.period_size, &dir);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(backend.pcm_handle, hw_params);
    Assert(rc >= 0, fmt::format("unable to set hw parameters: {}", snd_strerror(rc)));

    // snd_pcm_hw_params_get_period_size(hw_params, &config.period_size, &dir);
#else
    static_cast<void>(config);
#endif

    // play_something(backend, config);

    log_debug(root, "audio: backend ready");

    return backend;
}

void destroy_audio_backend(ReaperRoot& root, AudioBackend& backend)
{
    REAPER_PROFILE_SCOPE("Audio", MP_BLUE);
    log_info(root, "audio: destroying backend");

#if defined(REAPER_USE_ALSA)
    snd_pcm_close(backend.pcm_handle);
#endif

    backend.audio_buffer.clear();
}

namespace
{
    i32 f32_to_i32(f32 value) { return (i32)(value * (float)0x7fffffff); }

    float note(float semitone_offset, float base_freq = 440.f) { return base_freq * powf(2.f, semitone_offset / 12.f); }

    float wave(float time_s, float frequency)
    {
        // return std::sin(time_s * frequency * 2.f * M_PI);
        return std::sin(time_s * frequency * 2.f * M_PI) > 0.f ? 1.0f : -1.0f;
    }
} // namespace

void play_something(AudioBackend& backend, const AudioConfig& config)
{
    u32 sample_count = 5 * config.sample_rate;
    u32 frame_size = config.channel_count * config.bit_depth / 8; /* 4 bytes/sample, 2 channels */

    u32 size = sample_count * frame_size;
    u8* buffer = (u8*)malloc(size);

    for (u32 i = 0; i < sample_count; i++)
    {
        float time_s = static_cast<float>(i) / static_cast<float>(config.sample_rate);
        float pitch_shift = 1.f + std::sin(time_s * 2.f * M_PI) * 0.04f;

        float sound = 0.33 * wave(time_s, pitch_shift * note(0.f)) + 0.33 * wave(time_s, pitch_shift * note(5.f))
                      + 0.33 * wave(time_s, pitch_shift * note(12.f));

        float gain_db = -6.0f;
        float gain_linear = powf(2.f, gain_db / 3.f);

        i32 wave_l = f32_to_i32(sound * gain_linear);
        i32 wave_r = wave_l;

        i32* buffer_out_l = (i32*)(reinterpret_cast<i32*>(buffer) + i * frame_size);
        i32* buffer_out_r = (i32*)(reinterpret_cast<i32*>(buffer) + i * frame_size + 4);

        *buffer_out_l = wave_l;
        *buffer_out_r = wave_r;
    }

#if defined(REAPER_USE_ALSA)
    i32 rc = snd_pcm_writei(backend.pcm_handle, buffer, sample_count);

    Assert(rc != -EPIPE, "underrun occurred");
    Assert(rc >= 0, fmt::format("snd_pcm_writei error: {}", snd_strerror(rc)));
    Assert(rc == (i32)sample_count, fmt::format("short write, wrote {} frames", rc));

    snd_pcm_drain(backend.pcm_handle);
#else
    static_cast<void>(backend);
#endif

    free(buffer);
}

void print_audio_backend_diagnostics()
{
#if defined(REAPER_USE_ALSA)
    printf("ALSA library version: %s\n", SND_LIB_VERSION_STR);

    printf("\nPCM stream types:\n");
    for (u32 val = 0; val <= SND_PCM_STREAM_LAST; val++)
        printf("  %s\n", snd_pcm_stream_name((snd_pcm_stream_t)val));

    printf("\nPCM access types:\n");
    for (u32 val = 0; val <= SND_PCM_ACCESS_LAST; val++)
        printf("  %s\n", snd_pcm_access_name((snd_pcm_access_t)val));

    printf("\nPCM formats:\n");
    for (u32 val = 0; val <= SND_PCM_FORMAT_LAST; val++)
        if (snd_pcm_format_name((snd_pcm_format_t)val) != NULL)
            printf("  %s (%s)\n",
                   snd_pcm_format_name((snd_pcm_format_t)val),
                   snd_pcm_format_description((snd_pcm_format_t)val));

    printf("\nPCM subformats:\n");
    for (u32 val = 0; val <= SND_PCM_SUBFORMAT_LAST; val++)
        printf("  %s (%s)\n",
               snd_pcm_subformat_name((snd_pcm_subformat_t)val),
               snd_pcm_subformat_description((snd_pcm_subformat_t)val));

    printf("\nPCM states:\n");
    for (u32 val = 0; val <= SND_PCM_STATE_LAST; val++)
        printf("  %s\n", snd_pcm_state_name((snd_pcm_state_t)val));
#endif
}

void audio_execute_frame(ReaperRoot& root, AudioBackend& backend)
{
    if (!backend.enable_output)
        return;

#if defined(REAPER_USE_ALSA)
    REAPER_PROFILE_SCOPE("Audio", MP_BLUE);

    // if (backend.frame_index > 0)
    // {
    //     i32 rc = snd_pcm_start(backend.pcm_handle);
    //     Assert(rc >= 0, fmt::format("snd_pcm_start error: {}", snd_strerror(rc)));
    // }

    // snd_pcm_sframes_t avail, commitres;
    snd_pcm_state_t state = snd_pcm_state(backend.pcm_handle);
    Assert(state != SND_PCM_STATE_XRUN);

    int frame_bytes = 8; // FIXME

    u32       total_frames = backend.audio_buffer.size() / frame_bytes;
    u32       available_frame_count = total_frames - backend.frame_index;
    u32       current_buffer_offset = backend.frame_index * frame_bytes;
    const u8* buffer_ptr = backend.audio_buffer.data() + current_buffer_offset;

    u64 len = available_frame_count;

    while (len > 0)
    {
        i64 r = snd_pcm_writei(backend.pcm_handle, buffer_ptr, len);

        if (r == -EAGAIN)
            break;

        snd_pcm_state_t astate = snd_pcm_state(backend.pcm_handle);
        if (astate == SND_PCM_STATE_XRUN)
        {
            log_warning(root, "audio: buffer underrun, trying to recover stream");
            snd_pcm_prepare(backend.pcm_handle);
        }
        else
        {
            Assert(r != -EPIPE, "underrun occurred");
            Assert(r >= 0, fmt::format("snd_pcm_writei error: {}", snd_strerror(r)));

            i64 frames_written = r;

            len -= frames_written;

            log_debug(root, "audio: wrote {}/{} audio frames", frames_written, available_frame_count);

            backend.frame_index += frames_written;
        }
    }
#else
    static_cast<void>(root);
#endif
}
} // namespace Reaper
