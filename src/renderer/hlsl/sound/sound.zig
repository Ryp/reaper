// Mirror of src/renderer/shader/sound/sound.share.hlsl
//
// Layout is asserted at comptime; see hlsl/types.zig for why.

const hlsl = @import("../types.zig");

pub const SampleRate: hlsl.Uint = 44100;
pub const SampleRateInv: hlsl.Float = 1.0 / @as(hlsl.Float, @floatFromInt(SampleRate));
pub const FrameCountPerGroup: hlsl.Uint = 64;
pub const BitsPerChannel: hlsl.Uint = 32;
pub const SampleSizeInBytes: hlsl.Uint = 8;
pub const OscillatorCount: hlsl.Uint = 4;

pub const SoundPushConstants = extern struct {
    start_sample: hlsl.Uint = 0,
    _pad0: hlsl.Float = 0,
    _pad1: hlsl.Float = 0,
    _pad2: hlsl.Float = 0,
};

pub const OscillatorInstance = extern struct {
    frequency: hlsl.Float = 0,
    pan: hlsl.Float = 0,
    _pad0: hlsl.Float = 0,
    _pad1: hlsl.Float = 0,
};

comptime {
    hlsl.assertLayout(SoundPushConstants, 16);
    hlsl.assertLayout(OscillatorInstance, 16);
}
