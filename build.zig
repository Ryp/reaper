const std = @import("std");
const builtin = @import("builtin");

// Zig's self-hosted ELF linker cannot relocate the .sframe sections that recent
// glibc/GCC toolchains emit into crt1.o (R_X86_64_PC64), and the bundled LLD
// segfaults on them. Pinning the ABI explicitly makes the target non-native, so
// Zig builds and links its own glibc start files instead of the host's. The glibc
// version has to be pinned too, otherwise it is inherited from the host and Zig
// refuses to synthesize versions newer than the ones it ships stubs for.
const default_target: std.Target.Query = switch (builtin.os.tag) {
    .linux => .{
        .abi = .gnu,
        .glibc_version = .{ .major = 2, .minor = 38, .patch = 0 },
    },
    else => .{},
};

// Single source of truth for the Vulkan API version required by this project.
const vulkan_api_major: u32 = 1;
const vulkan_api_minor: u32 = 4;
const vulkan_api_patch: u32 = 0;

// Single source of truth for the app/engine version — read from build.zig.zon.
const zon = @import("build.zig.zon");
const app_semver = std.SemanticVersion.parse(zon.version) catch unreachable;

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{});
    const target = b.standardTargetOptions(.{ .default_target = default_target });

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
    const enable_validation = b.option(bool, "validation", "Enable Vulkan validation layers") orelse (optimize == .Debug);

    // The frame graph schedules barriers automatically, so synchronization
    // validation is the check that actually matters from M3 onwards.
    const enable_sync_validation = b.option(
        bool,
        "sync-validation",
        "Enable Vulkan synchronization validation",
    ) orelse enable_validation;

    const enable_tracy = b.option(bool, "tracy", "Enable Tracy support") orelse false;
    const tracy_callstack = b.option(bool, "tracy-callstack", "Include callstack information with Tracy data. Does nothing if -Dtracy is not provided") orelse enable_tracy;
    const tracy_allocation = b.option(bool, "tracy-allocation", "Include allocation information with Tracy data. Does nothing if -Dtracy is not provided") orelse enable_tracy;
    const tracy_callstack_depth: u32 = b.option(u32, "tracy-callstack-depth", "Declare callstack depth for Tracy data. Does nothing if -Dtracy_callstack is not provided") orelse 10;

    const no_bin = b.option(bool, "no-bin", "skip emitting binary") orelse false;

    exe.use_lld = b.option(bool, "lld", "Force the LLD linker on or off");

    // Zig 0.16's self-hosted x86_64 backend, which Debug builds default to,
    // passes a trailing `float` argument to a C function as garbage once more
    // than 8 integer/pointer arguments precede it. Minimal repro: an extern fn
    // taking 8 usize then f32 receives 0.0 instead of the value; with 7 it is
    // fine, and -fllvm is correct in both cases.
    //
    // meshopt_buildMeshlets has exactly that shape (10 args then cone_weight),
    // so the clusterizer would silently build meshlets with the wrong cone
    // weight — or trip its own assert. Nothing here needs the self-hosted
    // backend, so LLVM is used unless explicitly overridden.
    const use_llvm = b.option(bool, "llvm", "Use the LLVM backend") orelse true;
    exe.use_llvm = use_llvm;

    const exe_options = b.addOptions();

    exe_options.addOption(bool, "enable_vulkan", enable_vulkan);
    exe_options.addOption(bool, "enable_validation", enable_validation);
    exe_options.addOption(bool, "enable_sync_validation", enable_sync_validation);

    exe_options.addOption(u32, "vulkan_api_major", vulkan_api_major);
    exe_options.addOption(u32, "vulkan_api_minor", vulkan_api_minor);
    exe_options.addOption(u32, "vulkan_api_patch", vulkan_api_patch);

    exe_options.addOption(u32, "app_version_major", @intCast(app_semver.major));
    exe_options.addOption(u32, "app_version_minor", @intCast(app_semver.minor));
    exe_options.addOption(u32, "app_version_patch", @intCast(app_semver.patch));

    exe_options.addOption(bool, "enable_tracy", enable_tracy);
    exe_options.addOption(bool, "enable_tracy_callstack", tracy_callstack);
    exe_options.addOption(bool, "enable_tracy_allocation", tracy_allocation);
    exe_options.addOption(u32, "tracy_callstack_depth", tracy_callstack_depth);

    exe.root_module.addOptions("build_options", exe_options);

    // vulkan-zig is pure Zig with no runtime dependency on a device, so the
    // test artifact gets it too — the frame graph is typed in terms of Vulkan
    // enums but is entirely testable on the CPU.
    const vulkan_registry = b.dependency("vulkan_headers", .{});
    const amd_vma_dep = b.dependency("amd_vma", .{});
    const vulkan_module = b.dependency("vulkan_zig", .{
        .registry = vulkan_registry.path("registry/vk.xml"),
    }).module("vulkan-zig");

    if (enable_vulkan) {
        const registry = vulkan_registry;
        const vulkan = vulkan_module;

        exe.root_module.addImport("vulkan", vulkan);

        exe.root_module.link_libc = true;
        exe.root_module.link_libcpp = true;

        // VMA: expose Vulkan headers and the VMA header for @cImport in Zig source.
        const amd_vma = amd_vma_dep;
        exe.root_module.addIncludePath(registry.path("include"));
        exe.root_module.addIncludePath(amd_vma.path("include"));

        // VMA: compile the single implementation translation unit.
        // VMA is a C++14 header-only library, so we compile a .cpp TU.
        const vma_version_flag = std.fmt.allocPrint(
            b.allocator,
            "-DVMA_VULKAN_VERSION={d}",
            .{vulkan_api_major * 1_000_000 + vulkan_api_minor * 1_000 + vulkan_api_patch},
        ) catch @panic("OOM");

        exe.root_module.addCSourceFile(.{
            .file = b.path("src/renderer/vulkan/vma_impl.cpp"),
            .flags = &.{
                "-std=c++17",
                "-DVMA_IMPLEMENTATION",
                "-DVMA_STATIC_VULKAN_FUNCTIONS=0",
                "-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1",
                vma_version_flag,
                // Suppress warnings from third-party code.
                "-w",
            },
        });
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

    // meshoptimizer stays C++ and is referenced by path during coexistence, so
    // both builds compile the exact same sources — the meshlet layout has to
    // agree between them. It becomes a zon package in the C++ removal phase.
    addMeshOptimizer(b, exe);

    // lodepng, same deal: referenced by path so both builds decode identically.
    addLodePng(b, exe);

    // cgltf, likewise — the mesh data it hands back feeds the meshlet build, so
    // both builds have to read the exact same bytes out of the .bin.
    addCgltf(b, exe);

    // Dear ImGui, built from the same vendored submodule as the C++ target.
    addImGui(b, exe);

    // Shaders are compiled unconditionally so that `zig build test` can check
    // the registry without a Vulkan device.
    const shaders = addShaders(b);
    shaders.attachTo(exe);

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
    //
    // Deliberately independent of the executable: the test root only pulls in
    // GPU-free code, so `zig build test` stays runnable in CI without a Vulkan
    // device and without building SDL or VMA.
    const tests = b.addTest(.{
        .name = "test",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/test.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });

    shaders.attachTo(tests);
    tests.root_module.addImport("vulkan", vulkan_module);

    // The meshlet build is pure mesh processing, so it belongs in the GPU-free
    // suite — but it does need meshoptimizer linked, and therefore the same
    // LLVM-backend workaround as the executable.
    addMeshOptimizer(b, tests);
    tests.use_llvm = use_llvm;

    // gltf_loader.zig is pure asset parsing, so its tests run in the GPU-free
    // suite too.
    addCgltf(b, tests);

    // The Vulkan-typed modules @cImport the VMA header, so the test artifact
    // needs the include paths and the implementation TU to link. None of it
    // touches a device: the tests here are offset arithmetic and layout checks.
    tests.root_module.addIncludePath(vulkan_registry.path("include"));
    tests.root_module.addIncludePath(amd_vma_dep.path("include"));
    tests.root_module.addCSourceFile(.{
        .file = b.path("src/renderer/vulkan/vma_impl.cpp"),
        .flags = &.{
            "-std=c++17",
            "-DVMA_IMPLEMENTATION",
            "-DVMA_STATIC_VULKAN_FUNCTIONS=0",
            "-DVMA_DYNAMIC_VULKAN_FUNCTIONS=1",
            "-DVMA_VULKAN_VERSION=1003000",
            "-w",
        },
    });

    const tests_cmd = b.addRunArtifact(tests);

    const tests_step = b.step("test", "Run tests");
    tests_step.dependOn(&tests_cmd.step);
}

// --------------------------------------------------------------------------
// Shaders
//
// Mirrors the REAPER_SHADER_SRCS loop in src/renderer/CMakeLists.txt: for each
// entry point, glslangValidator produces unoptimized SPIR-V, spirv-opt
// legalizes and optimizes it, and spirv-val gates the result.
//
// Where CMake writes to build/shader/ and the C++ ShaderModules walks that
// directory at runtime, the .spv files here are embedded into the binary. The
// registry keys stay byte-identical to the C++ `code_map` keys (the shader
// path relative to the shader root, with the .hlsl extension replaced by .spv)
// so renderpass code can be ported without touching any shader name.
// --------------------------------------------------------------------------

const shader_root = "src/renderer/shader";

/// The Vulkan version glslang targets. NOTE: glslang generates bad code when
/// targeting vulkan1.2 (KhronosGroup/glslang#2411), so this deliberately does
/// not track vulkan_api_* above — same as the CMake build.
const shader_target_env = "vulkan1.1";
const shader_entry_point = "main";

/// Verbatim copy of REAPER_SHADER_SRCS, in the same order.
const shader_sources = [_][]const u8{
    "copy_to_depth.frag.hlsl",
    "copy_to_depth_from_hzb.frag.hlsl",
    "debug_geometry/build_cmds.comp.hlsl",
    "debug_geometry/draw.frag.hlsl",
    "debug_geometry/draw.vert.hlsl",
    "reduce_exposure.comp.hlsl",
    "reduce_exposure_tail.comp.hlsl",
    "forward.frag.hlsl",
    "forward.vert.hlsl",
    "fullscreen_triangle.vert.hlsl",
    "gbuffer/gbuffer_write_opaque.frag.hlsl",
    "gbuffer/gbuffer_write_opaque.vert.hlsl",
    "gui_write.frag.hlsl",
    "histogram/reduce_histogram.comp.hlsl",
    "hzb_reduce.comp.hlsl",
    "matrix_row_major_codegen_bug.comp.hlsl",
    "meshlet/cull_meshlet.comp.hlsl",
    "meshlet/cull_triangle_batch.comp.hlsl",
    "meshlet/prepare_fine_culling_indirect.comp.hlsl",
    "shadow/render_shadow.vert.hlsl",
    "sound/oscillator.comp.hlsl",
    "swapchain_write.frag.hlsl",
    "tone_mapping_bake_lut.comp.hlsl",
    "tiled_lighting/classify_volume.comp.hlsl",
    "tiled_lighting/rasterize_light_volume.frag.hlsl",
    "tiled_lighting/rasterize_light_volume.vert.hlsl",
    "tiled_lighting/tile_depth_downsample.comp.hlsl",
    "tiled_lighting/tiled_lighting.comp.hlsl",
    "tiled_lighting/tiled_lighting_debug.comp.hlsl",
    "vis_buffer/resolve_depth_legacy.frag.hlsl",
    "vis_buffer/vis_buffer_raster.frag.hlsl",
    "vis_buffer/vis_buffer_raster.vert.hlsl",
    "vis_buffer/fill_gbuffer.comp.hlsl",
    "vis_buffer/fill_gbuffer_msaa.comp.hlsl",
    "vis_buffer/fill_gbuffer_msaa_with_depth_resolve.comp.hlsl",
};

/// Shaders that exist only on the Zig side. Kept separate so shader_sources
/// above stays a verbatim copy of REAPER_SHADER_SRCS and the CMake build is
/// left alone.
const extra_shader_sources = [_][]const u8{
    "debug_gradient.comp.hlsl",
};

pub const Shaders = struct {
    /// Import as "shaders"; keys are the C++ `code_map` keys.
    module: *std.Build.Module,
    /// Depend on this to make spirv-val gate a build; the module import alone
    /// only pulls in spirv-opt's output.
    validate_step: *std.Build.Step,

    pub fn attachTo(shaders: Shaders, compile: *std.Build.Step.Compile) void {
        compile.root_module.addImport("shaders", shaders.module);
        compile.step.dependOn(shaders.validate_step);
    }
};

/// Compiles every shader and returns a module exposing them by registry key.
fn addShaders(b: *std.Build) Shaders {
    // glslang has no dependency scanner we can hook into, so every shader is
    // conservatively rebuilt when any shared header changes.
    const shared_inputs = collectSharedShaderInputs(b);

    const shaders_step = b.step("shaders", "Compile all shaders to SPIR-V");

    var registry_source: std.ArrayList(u8) = .empty;
    registry_source.appendSlice(b.allocator,
        \\// Generated by build.zig — do not edit.
        \\
        \\const std = @import("std");
        \\
        \\fn embed(comptime name: []const u8) []const u32 {
        \\    const aligned = struct {
        \\        const data align(@alignOf(u32)) = @embedFile(name).*;
        \\    };
        \\    return std.mem.bytesAsSlice(u32, &aligned.data);
        \\}
        \\
        \\pub const map = std.StaticStringMap([]const u32).initComptime(.{
        \\
    ) catch @panic("OOM");

    const shaders_module = b.createModule(.{});

    for (shader_sources ++ extra_shader_sources) |source| {
        // "meshlet/cull_meshlet.comp.hlsl" -> "meshlet/cull_meshlet.comp.spv",
        // which is exactly the key C++ builds from the build/shader/ layout.
        const key = std.mem.concat(b.allocator, u8, &.{
            source[0 .. source.len - ".hlsl".len],
            ".spv",
        }) catch @panic("OOM");

        const basename = std.fs.path.basename(key);

        // glslangValidator --target-env vulkan1.1 -D_GLSLANG -g -e main -V -D <in> -I<dir> -o <out>
        const glslang = b.addSystemCommand(&.{
            "glslangValidator",
            "--target-env",
            shader_target_env,
            "-D_GLSLANG",
            "-g",
            "-e",
            shader_entry_point,
            "-V",
            "-D",
        });
        glslang.addFileArg(b.path(b.pathJoin(&.{ shader_root, source })));
        glslang.addPrefixedDirectoryArg("-I", b.path(shader_root));
        glslang.addArg("-o");
        const unoptimized = glslang.addOutputFileArg(
            std.mem.concat(b.allocator, u8, &.{ basename, ".unoptimized" }) catch @panic("OOM"),
        );

        for (shared_inputs) |input| {
            glslang.addFileInput(input);
        }

        // spirv-opt <in> --legalize-hlsl -Os -o <out>
        const spirv_opt = b.addSystemCommand(&.{"spirv-opt"});
        spirv_opt.addFileArg(unoptimized);
        spirv_opt.addArgs(&.{ "--legalize-hlsl", "-Os", "-o" });
        const optimized = spirv_opt.addOutputFileArg(basename);

        // spirv-val <out> — a gate, not a transform, so it produces no file.
        const spirv_val = b.addSystemCommand(&.{"spirv-val"});
        spirv_val.addFileArg(optimized);

        shaders_step.dependOn(&spirv_val.step);

        shaders_module.addAnonymousImport(key, .{ .root_source_file = optimized });

        registry_source.print(b.allocator, "    .{{ \"{s}\", embed(\"{s}\") }},\n", .{ key, key }) catch @panic("OOM");
    }

    registry_source.appendSlice(b.allocator, "});\n") catch @panic("OOM");

    const write_files = b.addWriteFiles();
    shaders_module.root_source_file = write_files.add("shader_registry.zig", registry_source.items);

    return .{ .module = shaders_module, .validate_step = shaders_step };
}

/// Every file a shader might `#include`: the whole lib/ tree plus the
/// CPU-shared struct definitions.
fn collectSharedShaderInputs(b: *std.Build) []const std.Build.LazyPath {
    var inputs: std.ArrayList(std.Build.LazyPath) = .empty;

    var dir = std.Io.Dir.cwd().openDir(b.graph.io, b.pathFromRoot(shader_root), .{ .iterate = true }) catch
        @panic("cannot open the shader directory");
    defer dir.close(b.graph.io);

    var walker = dir.walk(b.allocator) catch @panic("OOM");
    defer walker.deinit();

    while (walker.next(b.graph.io) catch @panic("shader directory walk failed")) |entry| {
        if (entry.kind != .file) continue;

        const is_lib = std.mem.startsWith(u8, entry.path, "lib/") or std.mem.startsWith(u8, entry.path, "lib\\");
        const is_shared = std.mem.endsWith(u8, entry.path, ".share.hlsl");
        if (!is_lib and !is_shared) continue;

        inputs.append(b.allocator, b.path(b.pathJoin(&.{ shader_root, b.dupePath(entry.path) }))) catch @panic("OOM");
    }

    return inputs.items;
}

// --------------------------------------------------------------------------
// meshoptimizer
//
// Only the clusterizer is actually used (meshopt_buildMeshlets and friends),
// but the library is small and its translation units are independent, so all of
// them are compiled rather than tracking which ones the linker happens to need.
// --------------------------------------------------------------------------

const meshoptimizer_root = "external/meshoptimizer/src";

const meshoptimizer_sources = [_][]const u8{
    "allocator.cpp",
    "clusterizer.cpp",
    "indexcodec.cpp",
    "indexgenerator.cpp",
    "overdrawanalyzer.cpp",
    "overdrawoptimizer.cpp",
    "simplifier.cpp",
    "spatialorder.cpp",
    "stripifier.cpp",
    "vcacheanalyzer.cpp",
    "vcacheoptimizer.cpp",
    "vertexcodec.cpp",
    "vertexfilter.cpp",
    "vfetchanalyzer.cpp",
    "vfetchoptimizer.cpp",
};

fn addMeshOptimizer(b: *std.Build, compile: *std.Build.Step.Compile) void {
    compile.root_module.addIncludePath(b.path(meshoptimizer_root));

    for (meshoptimizer_sources) |source| {
        compile.root_module.addCSourceFile(.{
            .file = b.path(b.pathJoin(&.{ meshoptimizer_root, source })),
            .flags = &.{
                "-std=c++11",
                // Suppress warnings from third-party code.
                "-w",
            },
        });
    }

    compile.root_module.link_libc = true;
    compile.root_module.link_libcpp = true;
}

// --------------------------------------------------------------------------
// cgltf
// --------------------------------------------------------------------------

const cgltf_root = "external/cgltf";

fn addCgltf(b: *std.Build, compile: *std.Build.Step.Compile) void {
    compile.root_module.addIncludePath(b.path(cgltf_root));

    compile.root_module.addCSourceFile(.{
        .file = b.path("src/mesh/cgltf_impl.c"),
        .flags = &.{
            "-std=c99",
            // Suppress warnings from third-party code.
            "-w",
        },
    });

    compile.root_module.link_libc = true;
}

// --------------------------------------------------------------------------
// Dear ImGui
// --------------------------------------------------------------------------

const imgui_root = "external/imgui";

/// Verbatim copy of the source list in cmake/external/imgui.cmake.
const imgui_sources = [_][]const u8{
    "imgui.cpp",
    "imgui_demo.cpp",
    "imgui_draw.cpp",
    "imgui_widgets.cpp",
    "imgui_tables.cpp",
    "backends/imgui_impl_vulkan.cpp",
};

/// Standard install locations, used when pkg-config is unavailable. A search
/// path that does not exist is simply ignored by the linker, so adding all of
/// them costs nothing.
const vulkan_lib_dir_fallbacks = [_][]const u8{
    "/usr/lib",
    "/usr/lib/x86_64-linux-gnu",
    "/usr/lib64",
    "/usr/local/lib",
};

fn vulkanLibDirs(b: *std.Build) []const []const u8 {
    if (b.option([]const u8, "vulkan-lib-dir", "Directory containing libvulkan, when it is somewhere unusual")) |dir| {
        return b.dupeStrings(&.{dir});
    }

    // Set by the Vulkan SDK's own setup scripts and by the CI action, where
    // the loader is not in any system directory.
    if (b.graph.environ_map.get("VULKAN_SDK")) |sdk| {
        if (sdk.len > 0) return b.dupeStrings(&.{b.pathJoin(&.{ sdk, "lib" })});
    }

    var exit_code: u8 = undefined;
    if (b.runAllowFail(
        &.{ "pkg-config", "--variable=libdir", "vulkan" },
        &exit_code,
        .ignore,
    )) |stdout| {
        const trimmed = std.mem.trim(u8, stdout, " \r\n");
        if (trimmed.len > 0) return b.dupeStrings(&.{trimmed});
    } else |_| {}

    return &vulkan_lib_dir_fallbacks;
}

fn addImGui(b: *std.Build, compile: *std.Build.Step.Compile) void {
    compile.root_module.addIncludePath(b.path(imgui_root));
    compile.root_module.addIncludePath(b.path("src/renderer"));

    for (imgui_sources) |source| {
        compile.root_module.addCSourceFile(.{
            .file = b.path(b.pathJoin(&.{ imgui_root, source })),
            .flags = &.{
                "-std=c++17",
                // Suppress warnings from third-party code.
                "-w",
            },
        });
    }

    // The plain-C surface Zig talks to; see src/renderer/imgui_shim.h.
    compile.root_module.addCSourceFile(.{
        .file = b.path("src/renderer/imgui_shim.cpp"),
        .flags = &.{"-std=c++17"},
    });

    // NOTE: the vendored imgui_impl_vulkan.cpp has both its function-pointer
    // definitions and its IMGUI_VULKAN_FUNC_MAP loader body commented out, so
    // IMGUI_IMPL_VULKAN_NO_PROTOTYPES does not actually compile there — and
    // the map predates the project's dynamic-rendering patch, so it is missing
    // vkCmdBeginRendering anyway. The C++ build resolves those symbols through
    // its own dlopen-based reaper_vulkan_loader; here they come from the system
    // loader instead. Everything else keeps going through the vulkan-zig
    // dispatch tables, which are still loaded via SDL.
    //
    // The default target pins the glibc version (see default_target), which
    // makes it non-native — so Zig does not add the host's library search
    // paths and libvulkan has to be located explicitly.
    for (vulkanLibDirs(b)) |lib_dir| {
        compile.root_module.addLibraryPath(.{ .cwd_relative = lib_dir });
    }
    compile.root_module.linkSystemLibrary("vulkan", .{});

    compile.root_module.link_libc = true;
    compile.root_module.link_libcpp = true;
}

// --------------------------------------------------------------------------
// lodepng
// --------------------------------------------------------------------------

const lodepng_root = "external/lodepng";

fn addLodePng(b: *std.Build, compile: *std.Build.Step.Compile) void {
    compile.root_module.addIncludePath(b.path(lodepng_root));

    // lodepng.h does not wrap its C API in `extern "C"`, so building
    // lodepng.cpp as C++ mangles the entry points that @cImport declares
    // unmangled. lodepng's own docs say the file is valid C once the C++
    // wrapper is disabled, and Zig picks the language from the extension — so
    // it is copied to a .c name for the build.
    const lodepng_c = b.addWriteFiles().addCopyFile(
        b.path(b.pathJoin(&.{ lodepng_root, "lodepng.cpp" })),
        "lodepng.c",
    );

    compile.root_module.addCSourceFile(.{
        .file = lodepng_c,
        .flags = &.{
            "-std=c99",
            "-DLODEPNG_NO_COMPILE_CPP",
            // Suppress warnings from third-party code.
            "-w",
        },
    });

    compile.root_module.link_libc = true;
    compile.root_module.link_libcpp = true;
}
