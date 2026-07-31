// Port of src/renderer/vulkan/ComputeHelper.h

const std = @import("std");

pub fn divRoundUp(batch_size: u32, group_size: u32) u32 {
    return (batch_size + group_size - 1) / group_size;
}

test "divRoundUp rounds up rather than truncating" {
    try std.testing.expectEqual(@as(u32, 4), divRoundUp(1024, 256));
    try std.testing.expectEqual(@as(u32, 5), divRoundUp(1025, 256));
    try std.testing.expectEqual(@as(u32, 1), divRoundUp(1, 256));
    try std.testing.expectEqual(@as(u32, 0), divRoundUp(0, 256));

    // The dispatch sizes the ported passes actually use.
    try std.testing.expectEqual(@as(u32, 160), divRoundUp(1280, 8));
    try std.testing.expectEqual(@as(u32, 90), divRoundUp(720, 8));
}
