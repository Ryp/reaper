// Port of src/renderer/vulkan/Swapchain.h + Swapchain.cpp
//
// Manages the WM swapchain lifecycle: configuration, creation, image-view
// creation, per-image semaphore allocation, resize, and teardown.
// Also contains the colour-space / transfer-function classification logic
// that lives in the C++ Swapchain.cpp.

const std = @import("std");
const vk = @import("vulkan");
const log = std.log.scoped(.vulkan);

// --------------------------------------------------------------------------
// Colour-space and transfer-function enums  (renderer/ColorSpace.h + TransferFunction.h)
// --------------------------------------------------------------------------

pub const ColorSpace = enum {
    Unknown,
    sRGB,
    Rec709,
    DisplayP3,
    Rec2020,
};

pub const TransferFunction = enum {
    Unknown,
    Linear,
    sRGB,
    Rec709,
    PQ,
    // FIXME Add HLG but assert
    scRGB_Windows,
};

// --------------------------------------------------------------------------
// SwapchainFormat
// --------------------------------------------------------------------------

pub const SwapchainFormat = struct {
    vk_format: vk.Format,
    vk_color_space: vk.ColorSpaceKHR,
    /// May differ from vk_format when VK_KHR_swapchain_mutable_format is used.
    vk_view_format: vk.Format,

    color_space: ColorSpace,
    transfer_function: TransferFunction = .Unknown,
    is_hdr: bool = false,
};

// --------------------------------------------------------------------------
// SwapchainDescriptor (input parameters)
// --------------------------------------------------------------------------

pub const SwapchainDescriptor = struct {
    preferred_image_count: u32,
    preferred_format: vk.SurfaceFormatKHR,
    preferred_extent: vk.Extent2D,
};

// --------------------------------------------------------------------------
// PresentationInfo  (mirrors the C++ struct of the same name)
// --------------------------------------------------------------------------

pub const PresentationInfo = struct {
    surface: vk.SurfaceKHR,

    // HDR / tone-mapping hint fields kept for parity with C++; unused by the
    // backend itself today.
    exposure_compensation_stops: f32 = 0.0,
    tonemap_min_nits: f32 = 0.1,
    tonemap_max_nits: f32 = 400.0,
    sdr_ui_max_brightness_nits: f32 = 200.0,
    sdr_peak_brightness_nits: f32 = 400.0,

    // C++ tracks this with a single `queue_swapchain_transition` bool and, on
    // the first frame after (re)creation, barriers every swapchain image out
    // of UNDEFINED at once. Validation rejects that: you may not transition an
    // image you have not acquired. One flag per image gives the same result —
    // each image leaves UNDEFINED exactly once — while only ever touching the
    // image the frame actually owns.
    images_pending_initial_transition: []bool = &.{},

    surface_caps: vk.SurfaceCapabilitiesKHR = undefined,
    swapchain_format: SwapchainFormat = undefined,
    image_count: u32 = 0,
    present_mode: vk.PresentModeKHR = .fifo_khr,
    surface_extent: vk.Extent2D = .{ .width = 0, .height = 0 },
    swapchain_usage_flags: vk.ImageUsageFlags = .{},
    surface_transform: vk.SurfaceTransformFlagsKHR = .{},

    swapchain: vk.SwapchainKHR = .null_handle,

    images: []vk.Image = &.{},
    image_views: []vk.ImageView = &.{},
    semaphores_rendering_finished: []vk.Semaphore = &.{},
};

// --------------------------------------------------------------------------
// Public functions
// --------------------------------------------------------------------------

/// Fill PresentationInfo.surface_caps, swapchain_format, image_count,
/// present_mode, surface_extent, swapchain_usage_flags, surface_transform.
/// Does not create the swapchain itself.
pub fn configureVulkanWmSwapchain(
    vki: anytype,
    physical_device: vk.PhysicalDevice,
    desc: SwapchainDescriptor,
    info: *PresentationInfo,
    allocator: std.mem.Allocator,
) !void {
    log.debug("configuring wm swapchain", .{});

    // ---- Surface capabilities (via KHR_get_surface_capabilities2) ----
    const surface_info2 = vk.PhysicalDeviceSurfaceInfo2KHR{
        .s_type = .physical_device_surface_info_2_khr,
        .p_next = null,
        .surface = info.surface,
    };

    var caps2 = vk.SurfaceCapabilities2KHR{
        .s_type = .surface_capabilities_2_khr,
        .p_next = null,
        .surface_capabilities = undefined,
    };
    try vki.getPhysicalDeviceSurfaceCapabilities2KHR(physical_device, &surface_info2, &caps2);

    info.surface_caps = caps2.surface_capabilities;
    const caps = caps2.surface_capabilities;

    // ---- Surface formats ----
    var format_count: u32 = 0;
    _ = try vki.getPhysicalDeviceSurfaceFormats2KHR(physical_device, &surface_info2, &format_count, null);
    if (format_count == 0) return error.NoSurfaceFormats;

    const formats = try allocator.alloc(vk.SurfaceFormat2KHR, format_count);
    defer allocator.free(formats);

    for (formats) |*f| {
        f.* = .{
            .s_type = .surface_format_2_khr,
            .p_next = null,
            .surface_format = undefined,
        };
    }
    _ = try vki.getPhysicalDeviceSurfaceFormats2KHR(physical_device, &surface_info2, &format_count, formats.ptr);

    info.swapchain_format = chooseSwapchainFormat(formats[0..format_count], desc.preferred_format);

    // sRGB swapchains: delegate the EOTF to the image-view format, keep
    // linear in the transfer function field so shaders don't double-apply it.
    if (info.swapchain_format.transfer_function == .sRGB) {
        info.swapchain_format.transfer_function = .Linear;

        // Override view format to the sRGB variant (needs swapchain_mutable_format).
        info.swapchain_format.vk_view_format = srgbViewFormat(info.swapchain_format.vk_format);
    }

    // ---- Image count ----
    {
        var image_count = desc.preferred_image_count;
        if (image_count < caps.min_image_count) image_count = caps.min_image_count;
        if (caps.max_image_count > 0 and image_count > caps.max_image_count)
            image_count = caps.max_image_count;

        log.debug("swapchain image count min={} max={} chosen={}", .{
            caps.min_image_count, caps.max_image_count, image_count,
        });

        if (image_count < 2) return error.SwapchainTooFewImages;
        if (image_count > 5) return error.SwapchainTooManyImages;

        info.image_count = image_count;
    }

    // ---- Present mode ----
    {
        var pm_count: u32 = 0;
        _ = try vki.getPhysicalDeviceSurfacePresentModesKHR(physical_device, info.surface, &pm_count, null);
        if (pm_count == 0) return error.NoPresentModes;

        const modes = try allocator.alloc(vk.PresentModeKHR, pm_count);
        defer allocator.free(modes);
        _ = try vki.getPhysicalDeviceSurfacePresentModesKHR(physical_device, info.surface, &pm_count, modes.ptr);

        info.present_mode = choosePresentMode(modes[0..pm_count]);
    }

    // ---- Extent ----
    info.surface_extent = chooseExtent(caps, desc.preferred_extent);

    // ---- Usage flags ----
    info.swapchain_usage_flags = .{ .color_attachment_bit = true };

    // Opt into transfer-src when the surface allows it so frames can be read
    // back for screenshots. Every driver worth caring about supports this, but
    // it is not guaranteed, hence the check.
    if (caps.supported_usage_flags.transfer_src_bit) {
        info.swapchain_usage_flags.transfer_src_bit = true;
    }

    // ---- Transform ----
    info.surface_transform = chooseTransform(caps);
}

/// Allocate the VkSwapchainKHR, retrieve images, create image views, and
/// create per-image semaphores. Leaves any previous swapchain handle in
/// place as `old_swapchain` so the driver can recycle memory.
pub fn createVulkanWmSwapchain(
    vkd: anytype,
    device: vk.Device,
    info: *PresentationInfo,
    allocator: std.mem.Allocator,
) !void {
    log.debug("creating wm swapchain", .{});

    const view_formats = [_]vk.Format{
        info.swapchain_format.vk_format,
        info.swapchain_format.vk_view_format,
    };

    var format_list = vk.ImageFormatListCreateInfo{
        .s_type = .image_format_list_create_info,
        .p_next = null,
        .view_format_count = view_formats.len,
        .p_view_formats = &view_formats,
    };

    const create_info = vk.SwapchainCreateInfoKHR{
        .s_type = .swapchain_create_info_khr,
        .p_next = @ptrCast(&format_list),
        .flags = .{ .mutable_format_bit_khr = true },
        .surface = info.surface,
        .min_image_count = info.image_count,
        .image_format = info.swapchain_format.vk_format,
        .image_color_space = info.swapchain_format.vk_color_space,
        .image_extent = info.surface_extent,
        .image_array_layers = 1,
        .image_usage = info.swapchain_usage_flags,
        .image_sharing_mode = .exclusive,
        .queue_family_index_count = 0,
        .p_queue_family_indices = null,
        .pre_transform = info.surface_transform,
        .composite_alpha = .{ .opaque_bit_khr = true },
        .present_mode = info.present_mode,
        .clipped = .true,
        .old_swapchain = info.swapchain, // recycle on resize
    };

    info.swapchain = try vkd.createSwapchainKHR(device, &create_info, null);

    log.info("swapchain created format={} colorspace={}", .{
        info.swapchain_format.vk_format,
        info.swapchain_format.vk_color_space,
    });

    // ---- Retrieve images ----
    var actual_image_count: u32 = 0;
    _ = try vkd.getSwapchainImagesKHR(device, info.swapchain, &actual_image_count, null);
    if (actual_image_count == 0) return error.NoSwapchainImages;

    info.images = try allocator.alloc(vk.Image, actual_image_count);
    errdefer allocator.free(info.images);
    _ = try vkd.getSwapchainImagesKHR(device, info.swapchain, &actual_image_count, info.images.ptr);
    info.image_count = actual_image_count;

    // ---- Image views ----
    try createImageViews(vkd, device, info, allocator);

    // ---- Semaphores ----
    try createSemaphores(vkd, device, info, allocator);

    info.images_pending_initial_transition = try allocator.alloc(bool, info.image_count);
    @memset(info.images_pending_initial_transition, true);
}

/// Destroy the image views, semaphores, and swapchain.
pub fn destroyVulkanWmSwapchain(
    vkd: anytype,
    device: vk.Device,
    info: *PresentationInfo,
    allocator: std.mem.Allocator,
) void {
    log.debug("destroying wm swapchain", .{});

    destroySemaphores(vkd, device, info, allocator);
    destroyImageViews(vkd, device, info, allocator);

    vkd.destroySwapchainKHR(device, info.swapchain, null);
    info.swapchain = .null_handle;
}

/// Resize the swapchain to a new extent, recycling the old VkSwapchainKHR.
pub fn resizeVulkanWmSwapchain(
    vki: anytype,
    vkd: anytype,
    device: vk.Device,
    physical_device: vk.PhysicalDevice,
    info: *PresentationInfo,
    extent: vk.Extent2D,
    allocator: std.mem.Allocator,
) !void {
    log.debug("resizing wm swapchain to {}x{}", .{ extent.width, extent.height });

    _ = try vkd.deviceWaitIdle(device);

    destroySemaphores(vkd, device, info, allocator);
    destroyImageViews(vkd, device, info, allocator);

    const desc = SwapchainDescriptor{
        .preferred_image_count = info.image_count,
        .preferred_format = .{
            .format = info.swapchain_format.vk_format,
            .color_space = info.swapchain_format.vk_color_space,
        },
        .preferred_extent = extent,
    };

    try configureVulkanWmSwapchain(vki, physical_device, desc, info, allocator);

    const old = info.swapchain;
    try createVulkanWmSwapchain(vkd, device, info, allocator);

    // createVulkanWmSwapchain stores `old` as oldSwapchain and the driver
    // hands it a new handle, so we can destroy the previous one now.
    if (old != .null_handle) {
        vkd.destroySwapchainKHR(device, old, null);
    }
}

// --------------------------------------------------------------------------
// Private helpers – image views
// --------------------------------------------------------------------------

fn createImageViews(vkd: anytype, device: vk.Device, info: *PresentationInfo, allocator: std.mem.Allocator) !void {
    info.image_views = try allocator.alloc(vk.ImageView, info.image_count);
    errdefer allocator.free(info.image_views);

    for (info.images, 0..) |image, i| {
        const view_info = vk.ImageViewCreateInfo{
            .s_type = .image_view_create_info,
            .p_next = null,
            .flags = .{},
            .image = image,
            .view_type = .@"2d",
            .format = info.swapchain_format.vk_view_format,
            .components = .{
                .r = .identity,
                .g = .identity,
                .b = .identity,
                .a = .identity,
            },
            .subresource_range = .{
                .aspect_mask = .{ .color_bit = true },
                .base_mip_level = 0,
                .level_count = 1,
                .base_array_layer = 0,
                .layer_count = 1,
            },
        };
        info.image_views[i] = try vkd.createImageView(device, &view_info, null);
    }
}

fn destroyImageViews(vkd: anytype, device: vk.Device, info: *PresentationInfo, allocator: std.mem.Allocator) void {
    for (info.image_views) |view| {
        vkd.destroyImageView(device, view, null);
    }
    allocator.free(info.image_views);
    allocator.free(info.images);
    allocator.free(info.images_pending_initial_transition);
    info.image_views = &.{};
    info.images = &.{};
    info.images_pending_initial_transition = &.{};
}

// --------------------------------------------------------------------------
// Private helpers – semaphores
// --------------------------------------------------------------------------

fn createSemaphores(vkd: anytype, device: vk.Device, info: *PresentationInfo, allocator: std.mem.Allocator) !void {
    info.semaphores_rendering_finished = try allocator.alloc(vk.Semaphore, info.image_count);
    errdefer allocator.free(info.semaphores_rendering_finished);

    const sem_info = vk.SemaphoreCreateInfo{
        .s_type = .semaphore_create_info,
        .p_next = null,
        .flags = .{},
    };

    for (info.semaphores_rendering_finished, 0..) |*sem, i| {
        sem.* = try vkd.createSemaphore(device, &sem_info, null);
        log.debug("created rendering-finished semaphore[{}]", .{i});
    }
}

fn destroySemaphores(vkd: anytype, device: vk.Device, info: *PresentationInfo, allocator: std.mem.Allocator) void {
    for (info.semaphores_rendering_finished) |sem| {
        vkd.destroySemaphore(device, sem, null);
    }
    allocator.free(info.semaphores_rendering_finished);
    info.semaphores_rendering_finished = &.{};
}

// --------------------------------------------------------------------------
// Private helpers – format / mode selection
// --------------------------------------------------------------------------

fn getColorSpace(sf: vk.SurfaceFormatKHR) ColorSpace {
    return switch (sf.color_space) {
        .srgb_nonlinear_khr => .sRGB,
        .extended_srgb_linear_ext => .sRGB, // Allows negative values (is that scRGB?)
        .bt709_linear_ext, .bt709_nonlinear_ext => .Rec709,
        .display_p3_linear_ext => .DisplayP3,
        .bt2020_linear_ext, .hdr10_st2084_ext, .hdr10_hlg_ext => .Rec2020,
        .display_native_amd => switch (sf.format) {
            // See https://github.com/GPUOpen-LibrariesAndSDKs/FreesyncPremiumProSample/blob/master/readme.md
            .a2r10g10b10_unorm_pack32 => .sRGB, // FIXME Implies AGSDisplaySettings::Mode::Mode_Freesync2_Gamma22?
            .r16g16b16a16_sfloat => .sRGB, // FIXME Implies AGSDisplaySettings::Mode::Mode_Freesync2_scRGB
            else => .Unknown,
        },
        else => .Unknown,
    };
}

fn getSurfaceFormat(sf: vk.SurfaceFormatKHR) SwapchainFormat {
    var out = SwapchainFormat{
        .vk_format = sf.format,
        .vk_color_space = sf.color_space,
        .vk_view_format = sf.format,
        .color_space = getColorSpace(sf),
    };

    switch (sf.color_space) {
        .srgb_nonlinear_khr => {
            out.transfer_function = .sRGB;
            out.is_hdr = false;
        },
        .bt709_linear_ext => {
            out.transfer_function = .Linear;
            out.is_hdr = false;
        },
        .bt709_nonlinear_ext => {
            out.transfer_function = .Rec709;
            out.is_hdr = false;
        },
        .hdr10_st2084_ext => {
            out.transfer_function = .PQ;
            out.is_hdr = true;
        },
        .extended_srgb_linear_ext => {
            out.transfer_function = .scRGB_Windows;
            out.is_hdr = true;
        },
        else => @panic("Unsupported color space"),
    }

    return out;
}

// Welcome to the fun world of choosing a swapchain format for Vulkan, but especially - HDR formats!
// At the time of writing this there's several caveats:
//
// * Win11 SDR Mode + NVIDIA 551.81 + RGB10A2 + PQ
// In this specific combination, you have an HDR capable screen but windows is set to SDR mode.
// If you choose a RGB10+PQ swapchain, windows WON'T be switched to HDR mode and you'll get horrible banding if your
// display link was set to 8bits per channel (most users). This happens BOTH in borderless fullscreen as well as
// windowed. Note that this doesn't happen with scRGB swapchains, in that case the driver correctly switches windows
// in HDR mode.
// https://forums.developer.nvidia.com/t/bug-driver-switches-display-to-hdr-from-vulkan-pq-swapchain-but-keeps-8-bit-display-link-when-windows-is-in-sdr-mode/287197
//
// * Win11 + AMD
// AMD doesn't report HDR formats when Windows is not in HDR mode itself. Other than that it behaves consistently.
//
// * Linux and HDR
// Support is very limited right now, but efforts are underway. More patience needed.
fn chooseSwapchainFormat(
    formats: []const vk.SurfaceFormat2KHR,
    preferred: vk.SurfaceFormatKHR,
) SwapchainFormat {
    // Short-circuit: exact preferred match.
    for (formats) |f| {
        if (f.surface_format.format == preferred.format and
            f.surface_format.color_space == preferred.color_space)
        {
            return getSurfaceFormat(f.surface_format);
        }
    }

    // Collect valid formats (unknown color space / TF excluded).
    var best: ?SwapchainFormat = null;

    std.debug.assert(formats.len > 0);

    log.debug("choosing {} formats from this list", .{formats.len});

    for (formats) |f| {
        const sf = getSurfaceFormat(f.surface_format);
        if (sf.color_space == .Unknown or sf.transfer_function == .Unknown) continue;

        log.debug("- format = {}, colorspace = {}", .{ sf.vk_format, sf.vk_color_space });

        best = if (best) |b| betterFormat(sf, b) else sf;
    }

    std.debug.assert(best != null);

    if (best) |format| {
        log.debug("selecting swapchain format = {}, colorspace = {}", .{ format.vk_format, format.vk_color_space });
        log.debug("selecting swapchain view format = {}", .{format.vk_view_format});
        return format;
    } else {
        return getSurfaceFormat(formats[0].surface_format);
    }
}

/// Returns whichever of `a` and `b` is preferred (HDR > SDR, PQ > others,
/// avoid 64-bit RT). Mirrors the C++ sort comparator.
fn betterFormat(a: SwapchainFormat, b: SwapchainFormat) SwapchainFormat {
    if (a.is_hdr != b.is_hdr) return if (a.is_hdr) a else b;
    if (a.transfer_function != b.transfer_function) {
        if (a.transfer_function == .PQ) return a;
        if (b.transfer_function == .PQ) return b;
    }
    // Avoid 64-bit RTs.
    if (a.vk_format == .r16g16b16a16_sfloat) return b;
    return a;
}

fn srgbViewFormat(fmt: vk.Format) vk.Format {
    return switch (fmt) {
        .b8g8r8a8_unorm => .b8g8r8a8_srgb,
        .r8g8b8a8_unorm => .r8g8b8a8_srgb,
        .a8b8g8r8_unorm_pack32 => .a8b8g8r8_srgb_pack32,
        else => fmt,
    };
}

fn choosePresentMode(modes: []const vk.PresentModeKHR) vk.PresentModeKHR {
    for (modes) |m| {
        if (m == .mailbox_khr) return m;
    }
    return .fifo_khr;
}

fn chooseExtent(caps: vk.SurfaceCapabilitiesKHR, preferred: vk.Extent2D) vk.Extent2D {
    const special: u32 = 0xFFFF_FFFF;
    if (caps.current_extent.width == special and caps.current_extent.height == special) {
        return .{
            .width = std.math.clamp(preferred.width, caps.min_image_extent.width, caps.max_image_extent.width),
            .height = std.math.clamp(preferred.height, caps.min_image_extent.height, caps.max_image_extent.height),
        };
    }
    return caps.current_extent;
}

fn chooseTransform(caps: vk.SurfaceCapabilitiesKHR) vk.SurfaceTransformFlagsKHR {
    if (caps.supported_transforms.identity_bit_khr) {
        return .{ .identity_bit_khr = true };
    }
    return caps.current_transform;
}
