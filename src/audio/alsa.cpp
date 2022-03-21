////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "alsa.h"

#include "WaveFormat.h"

#include <cmath>

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

namespace Reaper::Audio
{
void test_alsa()
{
    int val;

    printf("ALSA library version: %s\n", SND_LIB_VERSION_STR);

    printf("\nPCM stream types:\n");
    for (val = 0; val <= SND_PCM_STREAM_LAST; val++)
        printf("  %s\n", snd_pcm_stream_name((snd_pcm_stream_t)val));

    printf("\nPCM access types:\n");
    for (val = 0; val <= SND_PCM_ACCESS_LAST; val++)
        printf("  %s\n", snd_pcm_access_name((snd_pcm_access_t)val));

    printf("\nPCM formats:\n");
    for (val = 0; val <= SND_PCM_FORMAT_LAST; val++)
        if (snd_pcm_format_name((snd_pcm_format_t)val) != NULL)
            printf("  %s (%s)\n",
                   snd_pcm_format_name((snd_pcm_format_t)val),
                   snd_pcm_format_description((snd_pcm_format_t)val));

    printf("\nPCM subformats:\n");
    for (val = 0; val <= SND_PCM_SUBFORMAT_LAST; val++)
        printf("  %s (%s)\n",
               snd_pcm_subformat_name((snd_pcm_subformat_t)val),
               snd_pcm_subformat_description((snd_pcm_subformat_t)val));

    printf("\nPCM states:\n");
    for (val = 0; val <= SND_PCM_STATE_LAST; val++)
        printf("  %s\n", snd_pcm_state_name((snd_pcm_state_t)val));
}

/*
 *
 * This example opens the default PCM device, sets
 * some parameters, and then displays the value
 * of most of the hardware parameters. It does not
 * perform any sound playback or recording.
 *
 * */

/* Use the newer ALSA API */

/* All of the ALSA library API is defined
 *  * in this header */

void test_alsa2()
{
    int                  rc;
    snd_pcm_t*           handle;
    snd_pcm_hw_params_t* params;
    unsigned int         val, val2;
    int                  dir;
    snd_pcm_uframes_t    frames;

    /* Open PCM device for playback. */
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0)
    {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(handle, params);

    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S32_LE);

    /* Two channels (stereo) */
    snd_pcm_hw_params_set_channels(handle, params, 2);

    /* 44100 bits/second sampling rate (CD quality) */
    val = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0)
    {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }

    /* Display information about the PCM interface */

    printf("PCM handle name = '%s'\n", snd_pcm_name(handle));

    printf("PCM state = %s\n", snd_pcm_state_name(snd_pcm_state(handle)));

    snd_pcm_hw_params_get_access(params, (snd_pcm_access_t*)&val);
    printf("access type = %s\n", snd_pcm_access_name((snd_pcm_access_t)val));

    snd_pcm_hw_params_get_format(params, (snd_pcm_format_t*)&val);
    printf("format = '%s' (%s)\n",
           snd_pcm_format_name((snd_pcm_format_t)val),
           snd_pcm_format_description((snd_pcm_format_t)val));

    snd_pcm_hw_params_get_subformat(params, (snd_pcm_subformat_t*)&val);
    printf("subformat = '%s' (%s)\n",
           snd_pcm_subformat_name((snd_pcm_subformat_t)val),
           snd_pcm_subformat_description((snd_pcm_subformat_t)val));

    snd_pcm_hw_params_get_channels(params, &val);
    printf("channels = %d\n", val);

    snd_pcm_hw_params_get_rate(params, &val, &dir);
    printf("rate = %d bps\n", val);

    snd_pcm_hw_params_get_period_time(params, &val, &dir);
    printf("period time = %d us\n", val);

    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    printf("period size = %d frames\n", (int)frames);

    snd_pcm_hw_params_get_buffer_time(params, &val, &dir);
    printf("buffer time = %d us\n", val);

    snd_pcm_hw_params_get_buffer_size(params, (snd_pcm_uframes_t*)&val);
    printf("buffer size = %d frames\n", val);

    snd_pcm_hw_params_get_periods(params, &val, &dir);
    printf("periods per buffer = %d frames\n", val);

    snd_pcm_hw_params_get_rate_numden(params, &val, &val2);
    printf("exact rate = %d/%d bps\n", val, val2);

    val = snd_pcm_hw_params_get_sbits(params);
    printf("significant bits = %d\n", val);

    //    snd_pcm_hw_params_get_tick_time(params, &val, &dir);
    //    printf("tick time = %d us\n", val);

    val = snd_pcm_hw_params_is_batch(params);
    printf("is batch = %d\n", val);

    val = snd_pcm_hw_params_is_block_transfer(params);
    printf("is block transfer = %d\n", val);

    val = snd_pcm_hw_params_is_double(params);
    printf("is double = %d\n", val);

    val = snd_pcm_hw_params_is_half_duplex(params);
    printf("is half duplex = %d\n", val);

    val = snd_pcm_hw_params_is_joint_duplex(params);
    printf("is joint duplex = %d\n", val);

    val = snd_pcm_hw_params_can_overrange(params);
    printf("can overrange = %d\n", val);

    val = snd_pcm_hw_params_can_mmap_sample_resolution(params);
    printf("can mmap = %d\n", val);

    val = snd_pcm_hw_params_can_pause(params);
    printf("can pause = %d\n", val);

    val = snd_pcm_hw_params_can_resume(params);
    printf("can resume = %d\n", val);

    val = snd_pcm_hw_params_can_sync_start(params);
    printf("can sync start = %d\n", val);

    snd_pcm_close(handle);
}

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

void test_mmap()
{
    size_t pagesize = getpagesize();

    printf("System page size: %zu bytes\n", pagesize);

    int file_fd = open("/home/ryp/dev/reaper/misc/mmap.txt", O_RDONLY);
    if (file_fd == -1)
    {
        perror("Could not open");
        return;
    }

    char* region = (char*)mmap((void*)(pagesize * (1 << 20)), // Map from the start of the 2^20th page
                               pagesize,                      // for one page length
                               PROT_READ,
                               MAP_ANON | MAP_PRIVATE, // to a private block of hardware memory
                               file_fd,
                               0);

    if (region == MAP_FAILED)
    {
        perror("Could not mmap");
        return;
    }

    // strcpy(region, "Hello, poftut.com");

    printf("Contents of region: %s\n", region);

    int unmap_result = munmap(region, 1 << 10);
    if (unmap_result != 0)
    {
        perror("Could not munmap");
        return;
    }
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

void test_alsa3()
{
    int                  rc;
    snd_pcm_t*           handle;
    snd_pcm_hw_params_t* params;
    unsigned int         sample_rate;
    int                  dir;
    snd_pcm_uframes_t    frames;
    u8*                  buffer;

    /* Open PCM device for playback. */
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0)
    {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(handle, params);

    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S32_LE);

    /* Two channels (stereo) */
    snd_pcm_hw_params_set_channels(handle, params, 2);

    /* 44100 bits/second sampling rate (CD quality) */
    sample_rate = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, &dir);

    /* Set period size to 32 frames. */
    frames = 128;
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0)
    {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    u32 sample_count = 5 * sample_rate;
    u32 frame_size = 2 * 4; /* 4 bytes/sample, 2 channels */

    u32 size = sample_count * frame_size;
    buffer = (u8*)malloc(size);

    for (u32 i = 0; i < sample_count; i++)
    {
        float time_s = static_cast<float>(i) / static_cast<float>(sample_rate);
        float pitch_shift = 1.f + std::sin(time_s * 2.f * M_PI) * 0.04f;

        float sound = 0.33 * wave(time_s, pitch_shift * note(0.f)) + 0.33 * wave(time_s, pitch_shift * note(5.f))
                      + 0.33 * wave(time_s, pitch_shift * note(12.f));

        float gain_db = -6.0f;
        float gain = powf(2.f, gain_db / 3.f);

        i32 wave_l = f32_to_i32(sound * gain);
        i32 wave_r = wave_l;

        i32* buffer_out_l = (i32*)(buffer + i * frame_size);
        i32* buffer_out_r = (i32*)(buffer + i * frame_size + 4);

        *buffer_out_l = wave_l;
        *buffer_out_r = wave_r;
    }

    std::ofstream output_file("output.wav", std::ios::binary | std::ios::out);
    Assert(output_file.is_open());

    write_wav(output_file, buffer, size, 32, 44100);

    output_file.close();

    {
        rc = snd_pcm_writei(handle, buffer, sample_count);

        if (rc == -EPIPE)
        {
            /* EPIPE means underrun */
            fprintf(stderr, "underrun occurred\n");
            snd_pcm_prepare(handle);
        }
        else if (rc < 0)
        {
            fprintf(stderr, "error from writei: %s\n", snd_strerror(rc));
        }
        else if (rc != (int)frames)
        {
            fprintf(stderr, "short write, write %d frames\n", rc);
        }
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);

    free(buffer);
}
} // namespace Reaper::Audio
