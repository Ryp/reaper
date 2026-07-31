// Port of the axis half of src/input/GenericController.{h,cpp}.
//
// Only the axes are ported: the buttons feed the physics sim and the freeze-
// culling toggle, neither of which exists in v1. The Linux evdev controller
// backend is not ported either — the keyboard mapping in game_loop.zig is the
// only producer, exactly as it is in the C++ when ENABLE_LINUX_CONTROLLER is
// off.

const std = @import("std");

pub const Axis = enum(u32) {
    lt = 0,
    rt,
    lsx,
    lsy,
    rsx,
    rsy,
    dpad_x,
    dpad_y,

    pub const count = @typeInfo(Axis).@"enum".fields.len;
};

pub const State = struct {
    axes: [Axis.count]f32,

    /// The neutral state from create_generic_controller_state(): sticks
    /// centred, but triggers fully released reads as -1, not 0.
    pub const neutral: State = blk: {
        var state = State{ .axes = @splat(0.0) };
        state.axes[@intFromEnum(Axis.lt)] = -1.0;
        state.axes[@intFromEnum(Axis.rt)] = -1.0;
        break :blk state;
    };

    pub fn get(self: State, axis: Axis) f32 {
        return self.axes[@intFromEnum(axis)];
    }

    pub fn set(self: *State, axis: Axis, value: f32) void {
        self.axes[@intFromEnum(axis)] = value;
    }
};

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

const testing = std.testing;

test "the neutral state releases both triggers to -1" {
    // The trigger widgets map [-1, 1] onto [empty, full], so a zero-initialised
    // state would draw both triggers half pressed.
    const state = State.neutral;

    try testing.expectEqual(@as(f32, -1.0), state.get(.lt));
    try testing.expectEqual(@as(f32, -1.0), state.get(.rt));
    try testing.expectEqual(@as(f32, 0.0), state.get(.lsx));
    try testing.expectEqual(@as(f32, 0.0), state.get(.rsy));
}

test "the axis order matches GenericAxis::Type" {
    // The C++ indexes `axes[]` with this enum; the order is the layout.
    try testing.expectEqual(@as(u32, 0), @intFromEnum(Axis.lt));
    try testing.expectEqual(@as(u32, 1), @intFromEnum(Axis.rt));
    try testing.expectEqual(@as(u32, 2), @intFromEnum(Axis.lsx));
    try testing.expectEqual(@as(u32, 3), @intFromEnum(Axis.lsy));
    try testing.expectEqual(@as(u32, 4), @intFromEnum(Axis.rsx));
    try testing.expectEqual(@as(u32, 5), @intFromEnum(Axis.rsy));
    try testing.expectEqual(@as(usize, 8), Axis.count);
}
