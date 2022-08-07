#include "lib/base.hlsl"
#include "lib/constants.hlsl"
#include "lib/conversion.hlsl"

#include "share/sound.hlsl"

//------------------------------------------------------------------------------
// Input

VK_PUSH_CONSTANT_HELPER(SoundPushConstants) consts;

VK_BINDING(0, 0) ConstantBuffer<AudioPassParams> params;
VK_BINDING(1, 0) StructuredBuffer<OscillatorInstance> instance_params;

//------------------------------------------------------------------------------
// Output

VK_BINDING(2, 0) RWByteAddressBuffer Output;

//------------------------------------------------------------------------------

// pan -1 left 0 center +1 right
float2 stereo_pan(float2 s, float pan)
{
    // FIXME I don't know how to code a pan
    return float2(
        s.x * saturate(-pan + 1.0),
        s.y * saturate(pan + 1.0)
    );
}

float osc_sin(float time_secs, float frequency)
{
    return sin(time_secs * TAU * frequency);
}

float2 sample_oscillator(OscillatorInstance osc, float time_secs)
{
    const float smp_mono = osc_sin(time_secs, osc.frequency);
    const float2 smp_stereo = smp_mono.xx;

    return stereo_pan(smp_stereo, osc.pan);
}

uint2 encode_sample_stereo_integer(float2 s, uint bits)
{
    int2 s_snorm = int2(float_to_snorm(s.x, bits),
                        float_to_snorm(s.y, bits));

    return asuint(s_snorm);
}

float2 apply_gain(float2 input, float gain_db)
{
    float gain = pow(2.f, gain_db / 3.f);
    return input * gain;
}

[numthreads(FrameCountPerGroup, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    const uint sample_index = dtid.x;
    const float time_secs = (float)(consts.start_sample + sample_index) * SampleRateInv; // FIXME care with precision

    float2 mix = 0.f;
    for (uint i = 0; i < OscillatorCount; i++)
    {
        const OscillatorInstance instance = instance_params[i];
        const float2 oscillator_output = sample_oscillator(instance, time_secs);

        mix += oscillator_output; // FIXME naive mix
    }

    const float2 output = apply_gain(mix, -18.f);
    const uint2 encoded_output = encode_sample_stereo_integer(output, 32);

    Output.Store2(sample_index * SampleSizeInBytes, encoded_output);
}
