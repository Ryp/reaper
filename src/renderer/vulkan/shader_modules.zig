// Port of src/renderer/vulkan/ShaderModules.h + ShaderModules.cpp
//
// Same contract as the C++ side — look a shader up by the path it has under
// build/shader/, get its SPIR-V words back — but the lookup is a comptime map
// over embedded blobs instead of a runtime walk of a directory. That removes
// the C++ version's dependency on the process being started from the repo root
// with a populated build/shader/ next to it.

const std = @import("std");

const registry = @import("shaders");

/// Returns the SPIR-V words for `name`, e.g. "meshlet/cull_meshlet.comp.spv".
/// The name is a compile-time constant everywhere it is used, so a missing
/// entry is a programming error rather than a runtime condition — exactly like
/// the C++ Assert().
pub fn get(comptime name: []const u8) []const u32 {
    comptime {
        if (registry.map.get(name) == null) {
            @compileError("unknown shader '" ++ name ++ "'; is it listed in shader_sources in build.zig?");
        }
    }
    return registry.map.get(name).?;
}

/// Runtime lookup, for the rare caller that picks a shader dynamically.
pub fn find(name: []const u8) ?[]const u32 {
    return registry.map.get(name);
}

pub fn count() usize {
    return registry.map.kvs.len;
}

test "every registered shader is valid SPIR-V" {
    // 0x07230203 is the SPIR-V magic number. Catching a truncated or
    // byte-swapped blob here is much cheaper than debugging it at device level.
    try std.testing.expect(registry.map.kvs.len > 0);

    for (registry.map.keys(), registry.map.values()) |name, code| {
        errdefer std.debug.print("offending shader: {s}\n", .{name});

        try std.testing.expect(code.len >= 5);
        try std.testing.expectEqual(@as(u32, 0x0723_0203), code[0]);
    }
}
