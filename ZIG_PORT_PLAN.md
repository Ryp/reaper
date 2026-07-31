# Zig Port of Reaper — v1 Plan (handoff copy)

> Approved 2026-07-31. Canonical plan also at `~/.claude/plans/i-want-to-plan-binary-moth.md`; this repo copy is the handoff for future sessions. Project memory: `~/.claude/projects/-home-ryp-dev-reaper/memory/`.

## CURRENT STATUS (2026-07-31, M0 implemented)

- Milestones tracked in the Claude Code task list: **M0 done (pending one manual ESC check)**, M1–M8 pending.
- Local branch `zig-reference` (= dangling commit `4ddf766`) pins the old Zig port; origin/master was force-pushed after an intentional rebase — do NOT restore those commits, use `git show zig-reference:<path>` for reference only.
- Tooling: RTK plugin installed (user scope). `zig-docs` MCP already in `.mcp.json`. Graphify was installed then **fully removed at user request — do not reinstall**.
- Project `CLAUDE.md` does not exist currently; create it with build docs as the port progresses.

### M0 result

`anyzig build` and `anyzig build test` are green; `anyzig fmt --check build.zig src` is clean. A 120-frame run on
an AMD RX 9070 XT (RADV) is **validation clean** and reads back a solid frame of exactly the expected colour
(linear `(0.1, 0.2, 0.4)` → sRGB `(89, 124, 170)` across 100% of pixels). Resize and clean teardown verified.

Files added: `src/game_loop.zig`, `src/renderer/window/{sdl,window}.zig`,
`src/renderer/vulkan/{barrier,backend_resources,command_buffer,execute_frame,frame_sync,screenshot}.zig`.

### Deviations from the plan taken in M0 (all deliberate)

1. **vulkan-zig re-pinned** `30c3a9d5` → `b496a6a5` (the `zig-0.16-compat` branch tip). The planned pin uses
   `std.StringArrayHashMap`, removed in Zig 0.16; `master` now requires 0.17.0-dev.
2. **Default build target pins `abi=gnu` + glibc 2.38** (`default_target` in build.zig). Recent glibc/GCC emit
   `.sframe` sections into `crt1.o` that Zig's self-hosted ELF linker cannot relocate (`R_X86_64_PC64`), and the
   bundled LLD segfaults on them. A non-native target makes Zig build its own start files. `-Dlld` forces the
   linker either way. Revisit when Zig grows SFrame support.
3. **`PresentationInfo.queue_swapchain_transition` (bool) → `images_pending_initial_transition` ([]bool).** The
   C++ pattern barriers *every* swapchain image out of UNDEFINED on the first frame, which validation rejects
   (you may not transition an unacquired image). Per-image flags give the same effect, legally.
4. **M0 clears with an empty dynamic-rendering pass, not `vkCmdClearColorImage`.** The transfer clear would need
   `VK_IMAGE_USAGE_TRANSFER_DST_BIT` on the swapchain, which C++ never requests. Going through an attachment
   keeps the swapchain config and the barrier layouts identical to C++ and is the shape M1 needs anyway.
5. **Screenshot tooling pulled forward from M7.** `--frame-count N` + `--screenshot <path>` write the last frame
   as binary PPM (no encoder needed), with `.transfer_src_bit` added to the swapchain usage when the surface
   supports it. The copy is recorded into that frame's own command buffer. This makes every later milestone gate
   machine-checkable instead of eyeball-only; swap PPM for PNG once lodepng is compiled in.
6. `--width` / `--height` / `--fullscreen` flags added; `zig build test` no longer depends on the install step so
   CI can run it without a GPU or an SDL/VMA build.

ESC-to-quit confirmed manually by the user (it cannot be automated here: `xdotool` does not reach the window under
Wayland and `/dev/uinput` needs root). The M0 gate is fully closed.

### M1 result

**Done.** `zig build shaders` compiles all 35 entry points through the CMake chain
(`glslangValidator --target-env vulkan1.1 -D_GLSLANG -g -e main -V -D … -I<shader_dir>` → `spirv-opt
--legalize-hlsl -Os` → `spirv-val` gate) and embeds them; `shader_sources` in build.zig is a verbatim copy of
`REAPER_SHADER_SRCS`. build.zig generates `shader_registry.zig` (a comptime `StaticStringMap`) so the shader list
lives in exactly one place. `src/renderer/vulkan/shader_modules.zig` exposes the C++ `get_spirv_shader_module`
contract — `get(comptime name) []const u32`, keyed exactly like the C++ `code_map`
(`"meshlet/cull_meshlet.comp.spv"`), with an unknown name being a `@compileError`. Every `lib/**` file and every
`*.share.hlsl` is registered as a `Run.addFileInput` so any shared-header edit rebuilds all shaders.

**shader-diff verified:** all 35 outputs are **byte-identical** to running CMake's exact command line with the
system SDK (glslang 16.3.0, SPIRV-Tools v2026.2). Note the checked-in `build/shader/` tree is *stale* — 19 of 35
differ against it, but re-running the reference command reproduces the Zig output exactly, so the divergence is
old CMake output, not the Zig chain. A `zig build shader-diff` step is therefore unnecessary; re-run the
comparison ad hoc if the toolchain moves.

`zig build test` covers the registry (SPIR-V magic number on all 35) and needs no GPU: the shader module is
attached to the test artifact too, and the test step no longer depends on the install step.

**Pipeline + draw done.** `pipeline.zig` ports Pipeline.h/cpp field-for-field (all the `default_*` state
constructors, `GraphicsPipelineProperties`, graphics/compute creation, dynamic-rendering helpers);
`render_pass_helpers.zig` ports RenderPassHelpers.h. `renderpass/swapchain_pass.zig` is the M1 subset of
SwapchainPass.cpp: same vertex shader, same default pipeline state, same dynamic viewport/scissor, same
3-vertex draw, but paired with `gui_write.frag` — an existing shader that needs no descriptors and no push
constants — so nothing had to be added to the HLSL tree. Its pipeline lives outside the (not yet ported)
pipeline factory and rebuilds on `reconfigure()` when the swapchain view format changes.

*Gate — machine-verified, stricter than the planned eyeball:* the frame is exactly two colours. The fragment
shader's `float4(0.5, 0.2, 0.9, …)` reads back as sRGB `(188, 124, 243)` (exact), it covers exactly 1.00% of
the frame (the shader's UV box is 0.1 × 0.1), and its bounding box maps to UV `[0.800, 0.900]` on both axes to
the pixel. That confirms the triangle covers the full screen, UV interpolation is right, screen orientation is
right, and the SPIR-V is real. Validation clean; `zig build test` and `zig fmt --check` green.

Note: the fullscreen triangle survives the default `cullMode = BACK` / `frontFace = COUNTER_CLOCKWISE` state,
which a naive signed-area reading of the Vulkan spec suggests it should not. It was ported faithfully from C++
and empirically renders — do not "fix" the cull state on paper.

**Descriptor sets** (`descriptor_set.zig`, `DescriptorWriteHelper`) and `pipeline_factory.zig` were not needed
for this gate and move to M3, where the frame graph first needs them.

### M2 result

`src/math/linalg.zig` clones the glm subset the renderer uses, with glm's conventions intact (column-major
`m.c[col][row]`, `Mat4x3` = 4 columns of 3 rows = HLSL `float3x4`). `perspectiveRhZo`, `lookAtRh` and
`inverseMat4` are transliterated operation-for-operation rather than rewritten, so the floating point results
track glm's. 13 golden tests cover ZO depth mapping (near→0, far→1), right-handedness, general-matrix inverse
round-trips, quaternion composition, and the mat4x3↔mat4 conversions.

`src/renderer/hlsl/types.zig` ports Types.inl + shared_types.hlsl, and all **21** `.share.hlsl` files have
mirrors under `src/renderer/hlsl/` following the shader tree. The three C++ layout tests
(`test/hlsl/{float_vector,float_matrix,struct}.cpp`) are ported directly.

*Layout checking.* `assertLayout(T, size)` requires fields to sit back to back and the total to match — no
implicit padding anywhere. `assertLayoutPadded(T, declared, size)` is for the four structs whose trailing
padding is real (they end in a `uint2`, which forces 8-byte alignment); spelling out both numbers keeps the gap
visible. This catches misordered fields, wrong field types, *and* dropped `_pad` members — the last one matters
because removing a trailing pad usually leaves `@sizeOf` unchanged, so only the field sum notices. All three
failure modes were verified by deliberately breaking a mirror and confirming the compile error.

*Cross-validated against C++.* The asserts only prove the Zig mirrors are self-consistent, so both sides were
compiled and diffed: a namespaced C++ probe including every `.share.hlsl` (they can't share a translation unit —
three of them define `MinWaveLaneCount`) against an equivalent Zig probe. **All 40 sizes are identical**, which
is what actually rules out a transcription error. Worth repeating if a `.share.hlsl` ever changes:
`g++ -std=c++20 -I src -I src/renderer/shader -I external/glm -DGLM_FORCE_DEPTH_ZERO_TO_ONE`.

*Gate:* `zig build test` green — 17 tests, no GPU needed.

### M3 result

Ports FrameGraph, FrameGraphBuilder, the barrier scheduler, FrameGraphPass (the RAII barrier scope becomes
explicit begin/end paired with `defer`), Image, Buffer, DescriptorSet and FrameGraphResources, plus the GPU
resource property/view types. `PixelFormat` is skipped as planned — roughly 620 of Image.cpp's 800 lines are its
conversion tables, and `vk.Format` replaces all of it.

*Gate part 1 — graph tests, no GPU.* The C++ graph test builds a scene and asserts nothing, so it only ever
caught crashes. The same scene is declared here and checked: the pass nothing depends on is pruned along with the
texture it wrote, the schedule keeps declaration order, every used texture leaves UNDEFINED exactly once, and the
barrier events partition cleanly per pass. Cycle detection and transitive closure get direct tests. 23 tests
green.

*Gate part 2 — demo, validation and sync-validation clean.* `renderpass/debug_gradient.zig` declares three
passes with no scene behind them: compute writes a UV gradient into texture A, `vkCmdCopyImage` moves it to B,
and B is blitted onto the swapchain. The readback is pixel-exact — corners `(0,0,0)`, `(255,0,0)`, `(0,255,0)`,
`(255,255,0)`, centre `(128,128,0)`, blue always 0, exactly 65536 distinct colours (the full 8-bit R×G space) —
so the gradient really did survive all three passes and the automatic barriers between them.

Sync validation is now on by default in Debug (`-Dsync-validation`), chained into the instance as
`VkValidationFeaturesEXT`. It immediately earned its keep: the first run reported a READ_AFTER_WRITE hazard
because `vkCmdBlitImage` executes in the `BLIT` stage while the barrier only made the image available to `COPY`.
Worth remembering — a transfer access is not one stage.

Deviations, both fixing latent C++ bugs rather than porting them:
* Image views are only created for textures whose usage has a view-capable bit. C++ creates them
  unconditionally, which works there only because every texture it declares happens to be sampled or storage; a
  transfer-only texture makes `vkCreateImageView` fail outright.
* The `_b` copies of the texture arrays in FrameGraphResources are left out. They exist only to be swappable
  (confirmed by the author), and since the destroy pass runs *before* the swap and only touches the non-`_b`
  set, the swap moves empty arrays around and achieves nothing else.

One shader was added, `debug_gradient.comp.hlsl`, in a separate `extra_shader_sources` list so `shader_sources`
stays a verbatim copy of `REAPER_SHADER_SRCS` and CMake is untouched. It uses the engine's own
`[[spv::format_rgba8]]` convention; without it `RWTexture2D<float4>` compiles to `Rgba32f` and validation warns
that every store is undefined.

**Superseded:** the M1 fullscreen triangle. The swapchain is now a blit target rather than an attachment, so
`swapchain_pass.zig` is unused until M4a wires up the real composite.

### M4a progress — asset loading and camera done, GPU passes remaining

**Done.** `src/mesh/{mesh,obj_loader}.zig` and `src/renderer/camera.zig`.

*The plan was wrong about n-gons.* It called for a fan; tinyobj actually runs its built-in **ear clipping**, and
`track_chunk_simple.obj` has faces with up to **11 vertices**, so a fan would cover the notch on any concave
one. The ear clipping is ported as-is including its quirks: projection axes come from the first corner with a
non-degenerate cross product, and the loop gives up after a bounded number of unproductive iterations. Quads use
the shorter-diagonal split, and there is no vertex dedup — every face corner becomes a fresh vertex, index buffer
`0, 1, 2, ...`.

*Verified against tinyobj itself,* not by inspection: a C++ probe linking tinyobjloader and an equivalent Zig
probe hash the flattened positions, normals and UVs for all 8 meshes under `res/model`. **All hashes match** —
including the 300k-vertex dragon, the quad-bearing asteroid and box, and the 11-gon track chunk. Rebuild with
`g++ -std=c++17 -I external/tinyobjloader` if the loader ever changes.

Camera keeps both quirks, and they are load-bearing for framing: the *horizontal* half-FOV is what reaches
`glm::perspective`'s `fovy` slot (the vertical one is derived separately as `atan(tan(h) * aspect)`), and the
viewport Y flip negates the whole of column 1 rather than `[1][1]` alone. Both have direct tests so a future
"cleanup" cannot silently change the shot.

`zig build test` is at 37 tests.

**MeshCache + meshlet build done.** meshoptimizer is compiled from `external/meshoptimizer` by path, the same
sources CMake uses. The clusterizer half lives in `src/mesh/meshlet_builder.zig` rather than the Vulkan layer,
because it needs no device and belongs in the GPU-free suite; `mesh_cache.zig` keeps the buffers and the bump
allocator. Parameters are the planned 192 / 64 / 0.5.

> ### Zig 0.16 codegen bug — Debug builds now force the LLVM backend
>
> Zig 0.16's **self-hosted x86_64 backend**, which Debug builds default to, passes a trailing `float` argument
> to a C function as garbage once **more than 8 integer/pointer arguments** precede it. Minimal repro: an
> `extern fn` taking 8 `usize` then `f32` receives `0.0`; with 7 it is correct. `-fllvm` is correct in both
> cases, and `-fno-llvm` reproduces it.
>
> `meshopt_buildMeshlets` is exactly that shape — 10 arguments then `cone_weight` — so it tripped the
> clusterizer's own assert. Without the assert it would have silently clustered with the wrong cone weight.
> `build.zig` now sets `use_llvm = true` on both artifacts (`-Dllvm=false` to override). **Anything else passing
> floats to C is exposed to this**, so keep the workaround until Zig fixes the backend.

*Verification.* Meshlet counts, vertex counts, index offsets and the compacted index data match the C++ path
**exactly** for every mesh in `res/model` (992 / 1089 / 1563 / 809 / 22 meshlets etc.).

*Bounds sphere centre — investigated and closed as harmless.* The centres differ from the C++ build by ~1e-4 on
a unit-radius mesh. Chased to the bottom on a fixed 6-vertex / 4-triangle input, dumping all 48 bytes of
`meshopt_Bounds` from both sides: **exactly one byte of 48 differs**, `center[2]` = `0x3e370340` (C++) vs
`0x3e370341` (Zig) — a single ULP. Struct size, radius, cone axis, cone cutoff, cone apex and every other byte
are identical, which rules out the `sret` return-by-value theory and any layout problem.

The 1 ULP comes from **Zig's bundled clang**, not from the port: system `g++` and system `clang++` agree with
each other at `-O0/-Os/-O2`, with and without `-march=native`, `-mfma`, `-mavx2` and `-ffp-contract=off`, and all
of them disagree with Zig's clang at every Zig optimization level. On a whole mesh that single ULP is amplified
by the iterative bounding-sphere fit, which is why a symmetric mesh whose true centre is the origin can come out
with the residual's sign flipped (icosahedron centre `y`: `-0.000097` vs `+0.000097` — 1 ULP cannot flip a sign
at that magnitude, the iteration does).

Impact: none worth acting on. Meshlet membership, counts, offsets and compacted index data are byte-identical;
only the culling sphere centre moves by ~0.01% of its radius, and the radius itself matches. Do not expect
bit-identical bounds between the two builds.

**PrepareBuckets + scene graph done.** `src/renderer/prepare_buckets.zig`. Scene nodes come from an arena rather
than individual `new`/`delete` — the C++ has a FIXME asking for a pool allocator and an arena is that pool.
`prepareScene` takes the LOD-0 `MeshAlloc` array directly rather than the whole `MeshCache`, since that is all it
reads, which keeps it free of any Vulkan dependency and testable. The light projection stays duplicated from
Camera exactly as it is in C++: the two copies feed different values into the FOV slot, so merging them would
move the shadow frusta.

`zig build test` is at 44 tests.

**PipelineFactory, SamplerResources and the ToneMapping LUT bake done.** The factory keeps the C++ shape —
register a creator, get a tracker index, build lazily on first update — with a function pointer rather than a
closure so the registry stays a plain array.

> **The frame graph pruned the tone map pass, and the first "clean" run proved nothing.** Nothing consumes the
> LUT yet, so the DAG closure never reached the bake and it was dropped entirely — exactly the behaviour the M3
> pruning test checks for. `createFrameGraphRecord` now takes `has_side_effects` so the demo can force it alive;
> it goes back to `false` once SwapchainPass reads the LUT. Worth remembering when adding any pass ahead of its
> consumer: a validation-clean run is not evidence the pass ran.

Note `linear_clamp` and `linear_black_border` are byte-identical in the C++ despite the names — the second uses
`CLAMP_TO_EDGE`, not `CLAMP_TO_BORDER`. Both are kept so the names stay available to the passes that reference
them; changing the address mode would change what they sample outside `[0,1]`.

Validation and sync validation stay clean with the compute dispatch into a 1D storage image. **The LUT contents
are not verified** — that needs either a frame-graph texture readback or the swapchain pass consuming it.

`zig build test` is at 45 tests.

**Remaining for M4a:** the MeshletCulling passes(770), ForwardPass(411) with a constant default material, and
SwapchainPass(317) as the real composite. The M4a gate does not close on its own — the plan pairs it with M4b for
"lit, shadowed, textured helmet on screen".

## Context

Reaper is a ~30k-LOC C++20 Vulkan 1.4 engine (meshlet culling, visibility buffer, tiled deferred + forward split-screen, frame graph, HDR-aware swapchain) with a Neptune game layer. Goal: port it to Zig; the own C++ code + CMake get deleted **eventually, but NOT in v1** — v1 explicitly keeps both builds working side by side (user requirement). v1 success criterion (user, relaxed): the Zig binary brings up the same default scene — procedural track, SciFiHelmet player ship, 3 shadowed lights, split-screen forward/deferred composite, 3 ImGui windows — and **"we see the ship in the frame"**, judged by eyeball against a C++ screenshot. No determinism work, no C++ patches, no automated diff.

## Locked decisions (user-confirmed; do not re-litigate)

- **Windowing: SDL3** (castholm/SDL zon pkg `v0.4.0+3.4.0`) "for starters" — thin `Window`/event layer so a native-Wayland backend can slot in later without touching the renderer.
- **Zig 0.16 via anyzig**; deps as zon: `vulkan_zig` (Snektron `30c3a9d5`), `vulkan_headers` (v1.4.344), `clap`, `tracy` (v0.13.1, `-Dtracy`), `amd_vma` (v3.3.0), `sdl`. Single exe module, files mirror `src/` tree; no internal module boundaries. Second test root `src/test.zig`.
- **Third-party stays C/C++** compiled by build.zig (`link_libc`+`link_libcpp`): VMA TU, ImGui via zgui, meshoptimizer, cgltf, lodepng. **Rewritten in Zig**: tinyobjloader → `src/mesh/obj_loader.zig`, tinyddsloader → DDS parser. **Dropped**: fmt (`std.fmt`), doctest (`zig test`), glm (hand-rolled math), vulkan_loader/ (vulkan-zig), inih, crashpad/breakpad, AMD RGA, VulkanStringConversion, GPUStackAllocator, PixelFormat enum (use `vk.Format` directly), XCB/XLib windows, alsa experiments, Spline.cpp (verified unused by trackgen).
- **Shaders: keep HLSL + glslang**, replicating CMake's exact chain per entry point (35-entry table mirroring `REAPER_SHADER_SRCS`): `glslangValidator --target-env vulkan1.1 -D_GLSLANG -g -e main -V -D -I<shader_dir>` → `spirv-opt --legalize-hlsl -Os` → `spirv-val` gate. Delivery: **@embedFile registry** — each `.spv` registered as anonymous import keyed exactly like C++ `code_map` (`"meshlet/cull_meshlet.comp.spv"`); `ShaderModules.zig` = comptime `StaticStringMap` behind the same `get(name) []const u32` contract (kills the `build/shader` CWD dependency). Optional `zig build shader-diff` byte-compares against CMake's `build/shader/**.spv` during coexistence (same system SDK ⇒ byte-identical). `addFileInput` all `lib/**` + `*.share.hlsl` for conservative rebuilds.
- **ImGui: `zgui` (zig-gamedev) bindings** — actively maintained, pins stock ImGui 1.92.1-docking, DrawList API exposed. Retires the Ryp imgui fork: dynamic rendering + external fn loading are official `imgui_impl_vulkan` features now (`InitInfo.UseDynamicRendering`, `ImGui_ImplVulkan_LoadFunctions` fed from vulkan-zig); the fork's linear-style-colors commit reproduces as a ~10-line init loop converting `style.Colors` sRGB→linear. Accepted delta: 1.92 widget styling ≠ 1.88 fork — fine under the relaxed bar. Reaper uses NO platform backend (io fed manually): use zgui core + its vendored `imgui_impl_vulkan`; if zgui only ships platform+render combos, compile vendored `backends/imgui_impl_vulkan.cpp` directly + declare the ~7 externs. Fallback binding: `floooh/dcimgui`. Init parity: `InitInfo{queue_family=0, MinImageCount=ImageCount=3, MSAA=1}`, color format `R8G8B8A8_SRGB`, one-shot font upload cmd buffer at renderer start.
- **Math: hand-rolled `src/math/linalg.zig`** (~650 LOC), glm storage/indexing clone: column-major `m.c[col][row]`, `Vec3=@Vector(3,f32)`, `Mat4x3` = 4 cols × 3 rows. Must replicate `perspectiveRH_ZO` (GLM_FORCE_DEPTH_ZERO_TO_ONE), `lookAtRh`, quat angleAxis/toMat4, general mat4 inverse. Port Camera.cpp's quirk faithfully (horizontal half-fov passed to fovy slot + Y-flip of `projection[1]`). zmath rejected (row-major convention = silent parity killer). **v1-only design (user): a rework is expected later — keep the module small, isolated behind one import, quirks documented.**
- **CPU↔GPU shared structs: hand-written `extern struct` mirrors** of the 20 `.share.hlsl` files under `src/renderer/hlsl/` (~900 LOC), field-level `align` reproducing `hlsl_*` shims (float3 = 12B/align4; float3_pad = 16B; float4x4 = 4 padded columns, col-major) + comptime `@sizeOf`/`@offsetOf` asserts + ported `test/hlsl/*.cpp` layout tests. No codegen. Padding fields default `= 0`.
- **Frame graph ports 1:1** (declare→build→allocate volatile→update descriptors→schedule→record→submit2→present; same record order as TestGraphics.cpp:419-536). Handles = non-exhaustive `enum(u32)` + `packed struct(u32)`; graph rebuilt per frame (no generational handles). `compute_schedule` verbatim but `std.sort.block` (stable) for barrier events. RAII barrier scope → explicit call + `defer`. Port `renderer/test/graph.cpp` to `zig test` (GPU-free gate).
- **Allocators**: one `ArenaAllocator` in BackendResources, `reset(.retain_capacity)` per frame — all frame-lifetime data arena-backed; persistent objects on GPA. DescriptorWriteHelper = exact-capacity arena slices + `appendAssumeCapacity` (pointer stability).
- **Error semantics**: init paths = error unions + errdefer chains; per-frame invariants = `std.debug.assert` (stricter than C++'s log-and-continue — intentional); deliberately tolerant paths (acquire OUT_OF_DATE/SUBOPTIMAL, present, event status, timeline timeout retry) handled explicitly, never `try`-propagated. `execute_frame` returns `!void`; game loop logs and continues.
- **maintenance5 module-less pipelines** kept (VkShaderModuleCreateInfo chained into stage p_next). PipelineFactory registry + per-frame dirty update kept. SwapchainPass pipeline stays outside the factory (spec-constant rebuild on swapchain reconfigure).
- **StorageBufferAllocator**: port verbatim + add `minStorageBufferOffsetAlignment` align-forward (fixes known FIXME).
- **Fix the 4 known defects from the old Zig code**: remove the AMD vendor_id 0x1002 rejection (C++ has no such filter — the comment claiming so is false), stop swallowing errors in `checkComputeStoresToDepth`, drop the all-false `ShaderAtomicFloatFeaturesEXT` chain, wire `resizeVulkanWmSwapchain` into the resize path (`new_swapchain_extent` consumed at top of execute frame).
- **Track gen**: port Track.cpp 1:1 (~350 LOC); `std.Random.DefaultPrng` with fixed default seed + `--seed N` flag; keep C++ draw order/retry structure. Tracks will differ from C++ runs — fine per relaxed bar. Spline.cpp NOT needed (dead constants only).
- **Physics stub** (`neptune/sim_stub.zig` ~60 LOC): `sim_update` no-op, player at spawn `translate(1.1, 0.8, 0)`; carries the 14 `vars` defaults from PhysicsSim.cpp:38-52 + 4 suspensions (`length_ratio_last=0.8`) so the Physics window renders correctly. Bullet returns in v2.
- **GBufferPass: skipped for v1** (dead code in the default frame). Post-v1 backlog.
- **v1 excludes**: ALSA audio (GPU audio pass stubbed/skipped with comment), gamepad, crash handlers, RenderDoc, free cam, test scene path.

## Target file layout (Zig)

```
build.zig / build.zig.zon
src/
  main.zig  test.zig  tracy.zig
  game_loop.zig                        # GameLoop.cpp v1 subset
  math/linalg.zig
  mesh/{mesh.zig, obj_loader.zig}
  neptune/{trackgen.zig, sim_stub.zig}
  renderer/
    camera.zig  prepare_buckets.zig  execute_frame.zig
    graph/{frame_graph.zig, builder.zig, debug.zig}
    hlsl/{types.zig, + one file per .share.hlsl (20)}
    vulkan/
      Backend.zig PhysicalDevice.zig Swapchain.zig       # from zig-reference + defect fixes
      backend_resources.zig framegraph_resources.zig
      buffer.zig image.zig storage_buffer.zig barrier.zig command_buffer.zig frame_sync.zig debug.zig
      shader_modules.zig shader_registry.zig pipeline.zig pipeline_factory.zig descriptor_set.zig sampler_resources.zig
      dds.zig texture_loading_png.zig material_resources.zig mesh_cache.zig
      vma_impl.cpp                                     # zgui dep provides ImGui bindings + impl_vulkan
      renderpass/{constants.zig, frame_graph_pass.zig, test_graphics.zig, tiled_lighting_common.zig,
        meshlet_culling.zig, shadow_map.zig, vis_buffer.zig, hzb.zig, tiled_raster.zig, tiled_lighting.zig,
        forward.zig, lighting.zig, histogram.zig, exposure.zig, tone_mapping.zig, debug_geometry.zig,
        gui.zig, swapchain_pass.zig}
  renderer/shader/**                   # HLSL tree stays as-is (compiled by build.zig)
```

C++ naming maps to namespaced fns: `create_forward_pass_record` → `forward.createRecord`; each pass file = `Resources` + `Record` + the create/record/updateDescriptors/recordCommandBuffer quadruplet.

## Milestones (each ends with pixels or a green test gate)

**M0 — Skeleton: window + device + clear.** build.zig + zon deps; SDL3 window + event pump (ESC quit, resize, mouse state); Backend/PhysicalDevice/Swapchain from zig-reference **with the 4 defect fixes**; vma_impl.cpp; FrameSync (timeline semaphore); acquire (10-retry) → `vkCmdClearColorImage` → present loop.
*Gate:* solid clear color, ESC quits, resize works, validation clean.
*Ports:* Backend(647) PhysicalDevice(191) Swapchain(564) PresentationSurface(119) DebugMessageCallback(175) FrameSync(47) Semaphore(29) CommandBuffer(27).

**M1 — Shader pipeline + fullscreen triangle.** 35-shader build chain + @embedFile registry + ShaderModules.zig; Pipeline(graphics subset)/DescriptorSet(minimal)/PipelineFactory; draw `fullscreen_triangle.vert` + trivial frag to swapchain.
*Gate:* UV gradient from real SPIR-V; optional `shader-diff` green vs CMake output.

**M2 — Math + HLSL interop (CPU-only).** linalg.zig + golden tests (inverse/perspective/lookAt); hlsl/types.zig + 20 mirror files + comptime asserts; port test/hlsl layout tests.
*Gate:* `zig build test` green.

**M3 — Frame graph + resource plumbing.** graph port + ported graph.cpp test; Image(802)/Buffer(147)/barrier(112)/storage_buffer(114); FrameGraphResources(238) (events array, a/b swap); BackendResources skeleton + frame arena; execute-frame shape; demo: compute gradient pass → swapchain composite through the graph.
*Gate:* graph unit test green (no GPU); validation + sync-validation clean on demo.

**M4 — Forward path + asset pipeline (biggest; split a/b).**
*a:* MeshCache + meshoptimizer meshlet build (192/64/0.5); obj_loader.zig (no-dedup flattening, quad shorter-diagonal split, fan for ngons); PrepareBuckets(276); Camera(60); MeshletCulling passes (770 — forward consumes culled buffers, not skippable); ForwardPass(411) w/ constant default material; SamplerResources; ToneMapping LUT bake(138); SwapchainPass(317) real composite (forward bound to both halves temporarily).
*b:* MaterialResources(202) + PNG loader (lodepng TU, albedo sRGB, LinearTiling quirk) + dds.zig (DX10/BC7 subset) + cgltf glue (strided copy, 4-texture material table); ShadowMap(229) + 3 hardcoded lights.
*Gate:* lit, shadowed, textured helmet + a track chunk on screen via forward.

**M5 — Real scene.** trackgen.zig (seeded DefaultPrng, 100 skinned chunks); SceneGraph (node pool, parent walk); player node + helmet child (scale 0.4, rot −π/2 Y) + chase cam `inverse(lookAt((-2,0.8,0),(1,0.4,0),+Y))` + 3 lights (exact values GameLoop.cpp:535-570); per-frame scene_meshes rebuild; physics stub; game_loop.zig v1 subset.
*Gate:* full scene through forward path; ship visible over track.

**M6 — Vis-buffer + tiled deferred (right half).** VisibilityBufferPass(922) + fill-gbuffer compute; HZBPass(194); TiledRasterPass(722); TiledLightingCommon(115)/Pass(386); LightingPass(46).
*Gate:* true split-screen — left forward, right deferred, visually matching each other.

**M7 — ImGui + remaining frame.** zgui dep + vendored imgui_impl_vulkan wiring + sRGB→linear style-color init loop; GuiPass(146); the 3 windows (Rendering [~20-LOC comptime vk-format-name helper replaces VulkanStringConversion], Controller Axes [neutral: axes 0, LT=RT=−1], Physics [stub vars]); mouse→io wiring (pos polled per frame, wheel ×0.5); Histogram(183) + Exposure(356); DebugGeometry(525) (0 cmds default); audio pass skipped w/ comment. Optional: `--screenshot-frame N` swapchain readback → PNG via lodepng (add `.transfer_src_bit` to swapchain usage when supported).
*Gate:* **the v1 image** — split-screen + 3 ImGui windows; ship in frame. Eyeball vs a C++ run screenshot.

**M8 — v1 signoff: both builds working side by side (NO C++ removal — user requirement).**
1. Side-by-side run: C++ (cmake) and Zig, both from repo root sharing `res/`; screenshot each; eyeball check. "Ship in frame" = v1 done.
2. Non-destructive housekeeping only: Zig CI job alongside the untouched CMake matrix (`mlugg/setup-zig` from `minimum_zig_version`, Vulkan SDK pin 1.4.304.1 w/ Glslang+SPIRV-Tools, `zig build test`, `zig fmt --check build.zig src`), fix `.paths` in build.zig.zon, document `zig build` / `zig build run` / `zig build shaders` in README/CLAUDE.md.

**Post-v1 — C++ removal (separate, user-triggered phase):** delete own C++ + CMake + cmake/, deregister submodules (glm, fmt, doctest, tinyobjloader, tinyddsloader, bullet3, crashpad/breakpad/depot_tools, inih, amd-rga, tracy, amd-vma, imgui; meshoptimizer/cgltf/lodepng after zon-tarball conversion at exact pinned commits; **keep `res/`**); full CI rewrite; itch.io linux deploy from `zig-out` (Windows channel pauses until a Windows milestone).

**Post-v1 backlog (v2+):** GBufferPass port (dormant), Bullet physics, ALSA + GPU-audio readback, gamepad, Windows build via SDL3, DXC migration, native Wayland backend option, math-layer rework (user-flagged), imgui.ini persistence decisions.

## During coexistence

CMake stays untouched for all of v1; artifacts disjoint (`build/` vs `zig-out/`+`.zig-cache/` — ensure .gitignore covers both). Both binaries run from repo root sharing `res/`. build.zig references `external/meshoptimizer`, `external/cgltf`, `external/lodepng` by path during coexistence (identical sources both sides — meshlet consistency), zon-ified only in the post-v1 removal phase. ImGui is NOT shared: C++ keeps its fork, Zig uses zgui's stock 1.92 (accepted visual delta).

## Verification

- Per milestone: the visual gate + Debug-build validation layers (sync validation on for M3+).
- `zig build test`: graph culling/schedule tests, math goldens, interop layout asserts, obj/dds parsers against real `res/` fixtures, trackgen smoke test.
- Final v1: run both binaries from repo root, screenshot each (Zig side optionally `--screenshot-frame`), eyeball: split-screen present, track + ship + shadows + 3 ImGui windows. "Ship in frame" = pass.

## Key references for implementation

- `src/renderer/vulkan/renderpass/TestGraphics.cpp` — frame definition; 1:1 port anchor (record order :419-536, backend_execute_frame :122, debug UI :88-120)
- `src/renderer/graph/FrameGraph.cpp` + `FrameGraphBuilder.cpp` — culling + compute_schedule to transliterate
- `src/renderer/shader/shared_types.hlsl` + `src/renderer/hlsl/Types.{h,inl}` — layout contract for interop structs
- `src/renderer/PrepareBuckets.cpp`, `src/renderer/Camera.cpp` — exact glm semantics the math layer must clone
- `src/renderer/vulkan/BackendResources.cpp:56-79` — init ordering blueprint (order matters)
- `src/GameLoop.cpp` — scene setup :300-572, ImGui windows :139-257 + :752-790, event loop :594-861
- `src/neptune/trackgen/Track.cpp` — RNG at :48/:161, port 1:1
- `src/renderer/vulkan/MeshCache.cpp:127-234` — meshlet build params
- `src/mesh/ModelLoader.cpp:26-101` — no-dedup OBJ contract
- `src/renderer/CMakeLists.txt` — shader list + exact glslang/spirv-opt flags
- `git show zig-reference:build.zig` (+ Backend/PhysicalDevice/Swapchain.zig, main.zig, build.zig.zon) — reference Zig code, reuse content freely
- Old-attempt gotchas: memory notes at `~/.claude/projects/-home-ryp-dev-reaper/memory/MEMORY.md` (vulkan-zig 0.16 API quirks, SDL3 typed fn ptrs, VMA bootstrap ptrs)

## Risks (ranked)

1. **HLSL interop layout bugs** → silent GPU garbage. Mitigate: M2 comptime asserts + ported layout tests before any pass code.
2. **Math divergence** (perspectiveRH_ZO/lookAt/quirks) → wrong-looking frame. Mitigate: golden tests; port Camera quirks verbatim.
3. **Zig 0.16 std churn / vulkan-zig drift.** Mitigate: anyzig pin, zig-docs MCP, pinned dep commits; bump only between milestones.
4. **Frame-graph barrier scheduling bugs.** Mitigate: ported graph tests, sync validation, GraphDebug print-diff vs C++ on a toy graph.
5. **ImGui via zgui deltas** (stock 1.92 vs the C++ 1.88 fork). Mitigate: relaxed bar absorbs styling drift; dcimgui as fallback binding.
6. **SDL3-vs-C++ event/extent differences** (resize storms, extent negotiation). Mitigate: thin window layer; relaxed parity bar absorbs cosmetic differences.
7. **Effort size**: ~14-15k LOC Zig total. Milestone gates keep it always-runnable.
