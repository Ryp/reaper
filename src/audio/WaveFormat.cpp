////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "WaveFormat.h"

namespace Reaper::Audio
{
namespace
{
    struct wav_header
    {
        u8  magic[4];
        u32 chunk_size;
        u8  format_magic[4];
        u8  subchunk1_id[4];
        u32 subchunk1_size;
        u16 audio_format;
        u16 channel_count;
        u32 sample_rate;
        u32 byte_rate;
        u16 block_align;
        u16 bits_per_sample;
        u8  subchunk2_id[4];
        u32 subchunk2_size;
    };
    void write_magic(u8* dst, const char* magic)
    {
        for (u32 i = 0; i < 4; i++)
            dst[i] = magic[i];
    }
} // namespace

void write_wav(std::ostream& out, const u8* raw_audio, u32 audio_size, u32 bit_depth, u32 sample_rate)
{
    const u32 channel_count = 2;
    const u32 frame_size = channel_count * bit_depth / 8;

    wav_header header = {};
    write_magic(header.magic, "RIFF");
    header.chunk_size = sizeof(header) - 8 + audio_size;
    write_magic(header.format_magic, "WAVE");
    write_magic(header.subchunk1_id, "fmt ");
    header.subchunk1_size = 16;
    header.audio_format = 1;
    header.channel_count = 2;
    header.sample_rate = sample_rate;
    header.byte_rate = sample_rate * frame_size;
    header.block_align = frame_size;
    header.bits_per_sample = bit_depth;
    write_magic(header.subchunk2_id, "data");
    header.subchunk2_size = audio_size;

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(raw_audio), audio_size);
}
} // namespace Reaper::Audio
