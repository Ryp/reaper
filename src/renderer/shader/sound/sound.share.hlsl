////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SOUND_SHARE_INCLUDED
#define SOUND_SHARE_INCLUDED

#include "shared_types.hlsl"

static const hlsl_uint SampleRate = 44100;
static const hlsl_float SampleRateInv = 1.f / (hlsl_float)SampleRate;
static const hlsl_uint FrameCountPerGroup = 64;
static const hlsl_uint BitsPerChannel = 32;
static const hlsl_uint SampleSizeInBytes = 8;

static const hlsl_uint OscillatorCount = 4;

struct SoundPushConstants
{
    hlsl_uint start_sample;
    hlsl_float _pad0;
    hlsl_float _pad1;
    hlsl_float _pad2;
};

struct AudioPassParams
{
    hlsl_float4 _pad;
};

struct OscillatorInstance
{
    hlsl_float frequency;
    hlsl_float pan;
    hlsl_float _pad0;
    hlsl_float _pad1;
};

#endif
