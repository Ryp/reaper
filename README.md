# Reaper

Reaper is a small C++ test engine, currently being ported to Zig. Both builds
are live and render the same scene; see `ZIG_PORT_PLAN.md` for what the port
covers and where the two still differ.

## Build (CMake, C++)

```sh
git submodule update --init
cmake -S . -B build
cmake --build build
```

## Build (Zig)

Needs Zig 0.16 — the version is pinned as `minimum_zig_version` in
`build.zig.zon`, so [anyzig](https://github.com/marler8997/anyzig) picks it up
automatically. The submodules are needed here too: the Zig build compiles
cgltf, imgui, lodepng and meshoptimizer straight out of `external/`.

```sh
git submodule update --init
zig build
```

| Command | What it does |
| --- | --- |
| `zig build` | Builds `zig-out/bin/reaper`. |
| `zig build run` | Builds and runs it. Arguments go after `--`. |
| `zig build shaders` | Compiles the HLSL tree to SPIR-V through glslang + spirv-opt + spirv-val. Run implicitly by `zig build`; the `.spv` blobs are embedded in the binary rather than loaded from disk. |
| `zig build test` | Runs the GPU-free test suite. Needs no Vulkan device. |
| `zig fmt --check build.zig src` | Formatting gate. |

Useful options: `-Dvalidation` / `-Dsync-validation` (both default to on in
Debug), `-Dtracy` for profiling, and `-Dvulkan-lib-dir=` if libvulkan lives
somewhere `pkg-config` and the standard paths do not cover.

The binary must be run from the repository root — it reads `res/` by relative
path, same as the C++ one.

```sh
zig build run -- --width 1280 --height 720
```

`--frame-count N` exits after N presented frames and `--screenshot <path>`
writes the last one out as a binary PPM, which is what makes the port's
milestone gates checkable without a human at the window.
