# HDR

State of HDR output in this engine, what it needs from the platform, and which
parts of the toolchain do not work once it is on. Written against the Zig
build; the C++ build shares the swapchain logic but has none of the fixes.

## Using it

```bash
zig build run -- --hdr
```

Off by default. With `--hdr` the format chooser takes
`VK_COLOR_SPACE_HDR10_ST2084_EXT` and the composite shader encodes PQ; without
it, HDR colour spaces are filtered out of the candidate list entirely.

`--swapchain-format <name>` forces a specific format/colour-space pair through
the preferred-format short-circuit, for comparing two of them on the same
scene. See `parseForcedFormat` in `src/main.zig` for the table.

## What the platform has to provide

Verified on sway 1.12 / wlroots 0.20.2 / mesa 26.1.6 / AMD RADV.

1. **The compositor's Vulkan renderer.** `WLR_RENDERER=vulkan`. This is not
   optional and it is not obvious: wlroots' GLES2 backend sets
   `features.output_color_transform = false`, so sway never creates the
   `wp_color_manager_v1` global, and Mesa's Wayland WSI early-returns after
   adding sRGB and PASS_THROUGH. That early return is the difference between a
   surface offering **18 formats and 90**.
2. **HDR enabled on the output.** `output DP-1 hdr on` in the sway config, and
   *no* `color_profile` line for that output — sway rejects the combination.
   `swaymsg -t get_outputs` distinguishes the two states: `features.hdr` is
   whether it *can*, `hdr` is whether it *is*.

With both in place the surface offers six colour spaces:
`SRGB_NONLINEAR`, `PASS_THROUGH`, `BT709_LINEAR`, `BT709_NONLINEAR`,
`BT2020_LINEAR`, `HDR10_ST2084`.

**scRGB will not appear on Linux.** `EXTENDED_SRGB_LINEAR_EXT` needs
`extended_target_volume`, which sway does not advertise in 1.12 or master. The
shader's `TRANSFER_FUNC_WINDOWS_SCRGB` path is Windows-only; Linux goes through
PQ.

`VK_COLOR_SPACE_PASS_THROUGH_EXT` is not a way to ask what the display wants —
it is the opposite. The Wayland WSI spec defines it as attaching *no* image
description to the surface, so the compositor falls back to assuming sRGB. The
format chooser filters those entries out and loses nothing by it.

## Why it is opt-in rather than detected

The compositor advertises PQ and BT.2020 as soon as it can **convert** them,
not when the output is in HDR mode. Take them on an SDR output and the shader
tone-maps to PQ purely so the compositor can tone-map back down — two passes,
worse than sending sRGB. Since the ranking puts `is_hdr` first, HDR formats had
to be filtered out rather than merely ranked below SDR.

Auto-detection is not currently possible:

- **SDL cannot answer.** `SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN` is derived from
  `HDR_headroom > 1.0` (`SDL_video.c:890`) and SDL's Wayland backend never sets
  `HDR_headroom`. It reads false regardless of the compositor's actual state.
  The Rendering window shows it, labelled as SDL's answer rather than the
  display's, precisely so it is not mistaken for ground truth.
- **Vulkan cannot answer.** A colour space says which transfer function the
  swapchain expects; nothing in Vulkan reports the display's peak luminance.
  `VK_EXT_hdr_metadata` is write-only by design.

The real answer means binding `wp_color_manager_v1` (live at v2 on a working
setup) and calling `get_surface_feedback` → `get_preferred` →
`get_information`, which returns `target_luminance`, `target_max_cll` and
`target_max_fall`. That is a Wayland client alongside the Vulkan one, and is
not written.

## Format selection

`betterFormat` and the filter in `chooseSwapchainFormat`
(`src/renderer/vulkan/Swapchain.zig`) carry two rules the C++ comparator does
not, because until the compositor started offering HDR colour spaces every
format was paired with `SRGB_NONLINEAR` and both questions were moot:

- **At least 10 bits per channel under HDR.** PQ distributes its code points
  across 0.005–10000 nits and bands visibly at 8 bits; HDR10 is defined as
  10-bit for that reason. Scoped to HDR deliberately — applying it to SDR would
  pull the choice to `r16g16b16a16_unorm` and lose the free sRGB view
  conversion.
- **No `_SRGB` format under a non-sRGB transfer function.** An `_SRGB` view
  applies the OETF in hardware, so pairing one with PQ double-encodes.

The second is a *filter*, not a ranking rule, and that distinction matters. As
a ranking rule it compares across colour spaces, which let `bt709_linear`
outrank sRGB and silently changed the SDR pick. It is a property of one
candidate, not a preference between two. A test pins the SDR choice.

Without these, the HDR10 list — which begins with `b8g8r8a8_srgb` — wins on
first-wins tie-break, giving an 8-bit double-encoded PQ swapchain. With them,
the pick is `a2r10g10b10_unorm_pack32`.

## Correctness check

Decoding the PQ output back through the ST.2084 EOTF and comparing against the
same scene rendered SDR:

| | ratio to `SDR_linear × tonemap_max_nits` |
|---|---|
| p10 | 0.918 |
| median | **0.985** |
| p90 | 0.994 |

So the HDR path reproduces the SDR path's luminance scaled by
`tonemap_max_nits`, to about 1.5%. Nothing double-encodes and the absolute
scale is right. The softness at p10 is 8-bit quantisation of the sRGB
reference in shadows, where PQ allocates far more code points.

All five PQ-capable formats produce identical nits — expected, since the shader
output is the same and only the storage differs.

## Known gaps

**Luminance is hardcoded.** `PresentationInfo.tonemap_max_nits = 400.0` and the
`sdr_*_nits` values next to it are placeholders. Under PQ these stop being
arbitrary and become *absolute* — 400 is simply wrong on a 1000-nit panel, and
the measured peak sitting at 294 nits is the tone-mapper faithfully hitting a
bad number. Fixing it means either the `wp_color_manager_v1` query above or
parsing the EDID HDR static metadata block.

**Exposure is in arbitrary units.** The `manual_exposure_key` FIXME in
`swapchain_write.frag.hlsl` says the HDR range should preferably be in nits.
That matters more under PQ than under SDR: the SDR path normalises, the PQ path
divides by a fixed `PQ_MAX_NITS`, so an exposure error shows up directly as
wrong absolute brightness.

**`vkSetHdrMetadataEXT` is never called.** Mesa would forward mastering display
primaries and min/max luminance to the compositor. It validates them and
silently ignores anything inconsistent, so passing garbage is not fatal, but
passing nothing means the compositor tone-maps without knowing our intent.

## Tooling that stops working under HDR

### Screenshots are not HDR-capable, on either side

`--screenshot` writes an 8-bit PNG. `PixelLayout.decode` in
`src/renderer/vulkan/screenshot.zig` is typed `fn (texel: []const u8) [3]u8`,
so every format — including the 10-bit and fp16 ones it correctly understands —
collapses to 8 bits before reaching the encoder. The file is therefore both
lossy *and* untagged, and since the contents are PQ code values rather than
display colours it reads washed-out in any ordinary viewer even when the render
is numerically correct.

This also means **banding differences between formats are not measurable
through the screenshot path** — 8-bit, 10-bit and fp16 sources all come out
identical. The argument for 10-bit rests on PQ's code-value spacing, not on a
measurement taken here.

Fixing it is two changes: widen `decode` to `[3]u16` and encode 16-bit, then
tag with a `cICP` chunk of `[9, 16, 0, 1]` — BT.2020 primaries, ST.2084 PQ,
identity matrix, full range. lodepng cannot write `cICP`; it is a 4-byte
ancillary chunk that can be appended by hand with a CRC32, or that path can
move to libpng, which has `png_set_cICP` and `png_set_cLLI` exported.

### No HDR screenshot tool exists for Wayland

Checked August 2026. This is not a gap in one tool, it is protocol-level:
**`ext-image-copy-capture-v1` carries no colour metadata at all** — no
primaries, no transfer function, no luminance. A capture client receives
PQ-encoded BT.2020 code values with nothing saying so.

Bit depth is *not* the blocker. On a working HDR setup the compositor offers
exactly one shm format for capture, `XRGB2101010` — genuine 10-bit.

- **grim** (1.5.0) negotiates that 10-bit buffer correctly and
  `get_pixman_format` maps all four 2101010 variants. It then discards them
  twice: `render()` hardcodes `PIXMAN_a8r8g8b8` as the composite target, and
  `write_to_png_stream` asserts 8-bit and sets `bit_depth = 8`. Output is an
  untagged 8-bit PNG — no `cICP`, `mDCV`, `cLLI`, `iCCP`, `sRGB`, `cHRM` or
  `gAMA`. Tracked upstream as [~emersion/grim
  #61](https://todo.sr.ht/~emersion/grim/61), which names pixman as the
  blocker. (grim is on SourceHut, not GitLab.)
- **wayshot** fails outright: it matches `Xbgr2101010` and sway offers
  `Xrgb2101010`.
- **wl-screenrec** tries 8-bit first and the compositor offers `XR24` on
  dmabuf, so it silently discards the HDR.
- **Spectacle** needs KWin.

The only correct option today for arbitrary desktop content is to toggle HDR
off, capture, and toggle back. For this engine's own output the in-app readback
above is the better path anyway — we control the swapchain format and know the
transfer function a priori, so there is nothing to negotiate.

### RenderDoc and HDR are mutually exclusive

RenderDoc's Vulkan layer has no `VK_KHR_wayland_surface`. Loading it — whether
by launching from qrenderdoc or via `--renderdoc` — makes `SDL_CreateWindow`
fail on a Wayland session:

```
Installed Vulkan doesn't implement the VK_KHR_wayland_surface extension
```

`SDL_VIDEODRIVER=x11` works and is how the RenderDoc integration was verified,
but an X11 surface offers no HDR colour spaces — only sRGB. So captures are
SDR-only.

RenderDoc does have an opt-in Wayland build:

```
option(ENABLE_UNSUPPORTED_EXPERIMENTAL_POSSIBLY_BROKEN_WAYLAND
       "Enable EXPERIMENTAL, POSSIBLY BROKEN, UNSUPPORTED wayland windowing support" OFF)
```

Default off, and Arch's `renderdoc` package does not set it — the installed
library carries the "support is not compiled in" error strings and does not
link `libwayland`. Building it locally with `-DENABLE_QRENDERDOC=OFF` and
pointing `LD_LIBRARY_PATH` at the result would test it without replacing the
system package, since `renderdoc.zig` resolves by soname. Whether the capture
layer then handles an `HDR10_ST2084` swapchain is a separate question and
untested.

### The desktop is too bright, and that is not this engine

With HDR on, wlroots maps SDR content to the ITU-R BT.2408 reference white of
**203 nits**, hardcoded in `render/color.c`, with no sway option to change it.
That is roughly 2× a normal SDR desktop. Tracked as
[swaywm/sway#9158](https://github.com/swaywm/sway/issues/9158), open since May
2026 with no work started. Realistic workaround is a keybind:

```
bindsym $mod+Shift+h output DP-1 hdr toggle
```

Note this is unrelated to sway PR #9170 / wlroots MR 5384 "Add brightness
support", which despite the name is panel backlight control via DRM properties.
