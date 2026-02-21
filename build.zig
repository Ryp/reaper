const std = @import("std");

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{});
    const target = b.standardTargetOptions(.{});

    const exe = b.addExecutable(.{
        .name = "reaper",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });

    const clap = b.dependency("clap", .{});
    exe.root_module.addImport("clap", clap.module("clap"));

    const enable_vulkan = true;

    const enable_tracy = b.option(bool, "tracy", "Enable Tracy support") orelse false;
    const tracy_callstack = b.option(bool, "tracy-callstack", "Include callstack information with Tracy data. Does nothing if -Dtracy is not provided") orelse enable_tracy;
    const tracy_allocation = b.option(bool, "tracy-allocation", "Include allocation information with Tracy data. Does nothing if -Dtracy is not provided") orelse enable_tracy;
    const tracy_callstack_depth: u32 = b.option(u32, "tracy-callstack-depth", "Declare callstack depth for Tracy data. Does nothing if -Dtracy_callstack is not provided") orelse 10;

    const no_bin = b.option(bool, "no-bin", "skip emitting binary") orelse false;

    const exe_options = b.addOptions();

    exe_options.addOption(bool, "enable_vulkan", enable_vulkan);

    exe_options.addOption(bool, "enable_tracy", enable_tracy);
    exe_options.addOption(bool, "enable_tracy_callstack", tracy_callstack);
    exe_options.addOption(bool, "enable_tracy_allocation", tracy_allocation);
    exe_options.addOption(u32, "tracy_callstack_depth", tracy_callstack_depth);

    exe.root_module.addOptions("build_options", exe_options);

    if (enable_vulkan) {
        const registry = b.dependency("vulkan_headers", .{});
        const vulkan = b.dependency("vulkan_zig", .{
            .registry = registry.path("registry/vk.xml"),
        }).module("vulkan-zig");

        exe.root_module.addImport("vulkan", vulkan);

        exe.root_module.link_libc = true;
        exe.root_module.linkSystemLibrary("glfw", .{});

        // compile_and_embed_hlsl_shader(b, exe.root_module, "./src/renderer/shaders/triangle.vert.hlsl", .Vertex, "triangle_vs") catch unreachable;
        // compile_and_embed_hlsl_shader(b, exe.root_module, "./src/renderer/shaders/triangle.frag.hlsl", .Fragment, "triangle_fs") catch unreachable;
        // compile_and_embed_hlsl_shader(b, exe.root_module, "./src/renderer/shaders/poly.vert.hlsl", .Vertex, "poly_vs") catch unreachable;
        // compile_and_embed_hlsl_shader(b, exe.root_module, "./src/renderer/shaders/poly.frag.hlsl", .Fragment, "poly_fs") catch unreachable;
    }

    if (enable_tracy) {
        const tracy = b.dependency("tracy", .{});
        const tracy_path = tracy.path("tracy");
        const client_cpp_path = tracy.path("public/TracyClient.cpp");
        const tracy_c_flags: []const []const u8 = &[_][]const u8{ "-DTRACY_ENABLE=1", "-fno-sanitize=undefined" };

        exe.root_module.addIncludePath(tracy_path);
        exe.root_module.addCSourceFile(.{ .file = client_cpp_path, .flags = tracy_c_flags });

        exe.root_module.link_libc = true;
        exe.root_module.link_libcpp = true;
    }

    const sdl_dep = b.dependency("sdl", .{
        .target = target,
        .optimize = optimize,
    });
    const sdl_lib = sdl_dep.artifact("SDL3");

    exe.root_module.linkLibrary(sdl_lib);

    if (no_bin) {
        b.getInstallStep().dependOn(&exe.step);
    } else {
        b.installArtifact(exe);
    }

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the program");
    run_step.dependOn(&run_cmd.step);

    // if (enable_tracy) {
    //     const tracy = b.dependency("tracy", .{});
    //     const tracy_profiler_path = tracy.path("profiler");

    //     const cmake_config = b.addSystemCommand(&.{ "cmake", "-G", "Ninja", "-S", tracy_profiler_path.getPath2(b, null), "-B", "build" });
    //     const cmake_build = b.addSystemCommand(&.{ "cmake", "--build", "build" });

    //     cmake_build.step.dependOn(&cmake_config.step);
    //     run_cmd.step.dependOn(&cmake_build.step);
    // }

    // Test
    const tests = b.addTest(.{
        .name = "test",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/test.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });

    b.installArtifact(tests);

    const tests_cmd = b.addRunArtifact(tests);
    tests_cmd.step.dependOn(b.getInstallStep());

    const tests_step = b.step("test", "Run tests");
    tests_step.dependOn(&tests_cmd.step);
}

// const ShaderStage = enum {
//     Vertex,
//     Fragment,
// };
//
// fn compile_and_embed_hlsl_shader(b: *std.Build, module: *std.Build.Module, hlsl_path: [:0]const u8, shader_stage: ShaderStage, import_name: [:0]const u8) !void {
//     const stage_string = switch (shader_stage) {
//         ShaderStage.Vertex => "-fshader-stage=vert",
//         ShaderStage.Fragment => "-fshader-stage=frag",
//     };
//
//     const cmd = b.addSystemCommand(&.{ "glslc", "--target-env=vulkan1.4", stage_string, "-o" });
//
//     const spv_path = try std.mem.concatWithSentinel(b.allocator, u8, &[_][]const u8{ hlsl_path, ".spv" }, 0);
//     defer b.allocator.free(spv_path);
//
//     const vert_spv = cmd.addOutputFileArg(spv_path);
//     cmd.addFileArg(b.path(hlsl_path));
//     module.addAnonymousImport(import_name, .{ .root_source_file = vert_spv });
// }
