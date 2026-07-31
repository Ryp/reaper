// Second test root (`zig build test`).
//
// Only GPU-free modules belong here: the suite has to stay runnable in CI on a
// machine with no Vulkan device.

comptime {
    _ = @import("renderer/vulkan/shader_modules.zig");
}
