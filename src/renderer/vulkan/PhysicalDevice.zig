// Port of src/renderer/vulkan/PhysicalDevice.h + PhysicalDevice.cpp
//
// Owns the immutable description of a chosen physical device: versioned
// properties, versioned features, memory layout, queue family indices, and a
// small set of derived "macro features" that are cheaper to query once.

const std = @import("std");
const vk = @import("vulkan");
const build_options = @import("build_options");
const log = std.log.scoped(.vulkan);

// --------------------------------------------------------------------------
// Public types
// --------------------------------------------------------------------------

pub const PhysicalDeviceInfo = struct {
    handle: vk.PhysicalDevice,

    // Filled by fillProperties()
    properties: vk.PhysicalDeviceProperties,
    properties_vk_1_1: vk.PhysicalDeviceVulkan11Properties,
    properties_vk_1_2: vk.PhysicalDeviceVulkan12Properties,
    properties_vk_1_3: vk.PhysicalDeviceVulkan13Properties,
    properties_vk_1_4: vk.PhysicalDeviceVulkan14Properties,
    memory_properties: vk.PhysicalDeviceMemoryProperties,

    // Filled by fillFeatures()
    features: vk.PhysicalDeviceFeatures,
    features_vk_1_2: vk.PhysicalDeviceVulkan12Features,
    features_vk_1_3: vk.PhysicalDeviceVulkan13Features,
    features_vk_1_4: vk.PhysicalDeviceVulkan14Features,
    shader_atomic_features: vk.PhysicalDeviceShaderAtomicFloatFeaturesEXT,
    primitive_restart_features: vk.PhysicalDevicePrimitiveTopologyListRestartFeaturesEXT,

    // Filled by fillQueues()
    // These can alias the same family index.
    graphics_queue_family_index: u32,
    present_queue_family_index: u32,

    macro_features: MacroFeatures,

    pub const MacroFeatures = struct {
        compute_stores_to_depth: bool = false,
    };
};

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

/// Build a complete PhysicalDeviceInfo for `handle`.
/// `vki`     – InstanceDispatch (anytype – caller owns the concrete type).
/// `surface` – the presentation surface used for queue-family support checks.
/// `allocator` – scratch allocator for temporary slices (freed before return).
pub fn createPhysicalDeviceInfo(
    vki: anytype,
    handle: vk.PhysicalDevice,
    surface: vk.SurfaceKHR,
    allocator: std.mem.Allocator,
) !PhysicalDeviceInfo {
    var info: PhysicalDeviceInfo = undefined;
    info.handle = handle;

    try fillProperties(vki, &info);
    fillFeatures(vki, &info);
    try fillQueues(vki, &info, surface, allocator);
    info.macro_features = fillMacroFeatures(vki, handle);

    return info;
}

// --------------------------------------------------------------------------
// Private helpers – properties
// --------------------------------------------------------------------------

fn fillProperties(vki: anytype, info: *PhysicalDeviceInfo) !void {
    // Build the pNext chain of versioned-properties structs so a single
    // vkGetPhysicalDeviceProperties2 call populates everything.
    info.properties_vk_1_1 = std.mem.zeroes(vk.PhysicalDeviceVulkan11Properties);
    info.properties_vk_1_1.s_type = .physical_device_vulkan_1_1_properties;

    info.properties_vk_1_2 = std.mem.zeroes(vk.PhysicalDeviceVulkan12Properties);
    info.properties_vk_1_2.s_type = .physical_device_vulkan_1_2_properties;
    info.properties_vk_1_2.p_next = @ptrCast(&info.properties_vk_1_1);

    info.properties_vk_1_3 = std.mem.zeroes(vk.PhysicalDeviceVulkan13Properties);
    info.properties_vk_1_3.s_type = .physical_device_vulkan_1_3_properties;
    info.properties_vk_1_3.p_next = @ptrCast(&info.properties_vk_1_2);

    info.properties_vk_1_4 = std.mem.zeroes(vk.PhysicalDeviceVulkan14Properties);
    info.properties_vk_1_4.s_type = .physical_device_vulkan_1_4_properties;
    // VkPhysicalDeviceVulkan14Properties has pointer members that must be null
    // when pCopySrcLayouts / pCopyDstLayouts are not used.
    info.properties_vk_1_4.p_copy_src_layouts = null;
    info.properties_vk_1_4.p_copy_dst_layouts = null;
    info.properties_vk_1_4.p_next = @ptrCast(&info.properties_vk_1_3);

    var props2 = vk.PhysicalDeviceProperties2{
        .s_type = .physical_device_properties_2,
        .p_next = @ptrCast(&info.properties_vk_1_4),
        .properties = undefined,
    };
    vki.getPhysicalDeviceProperties2(info.handle, &props2);

    info.properties = props2.properties;
    info.memory_properties = vki.getPhysicalDeviceMemoryProperties(info.handle);
}

// --------------------------------------------------------------------------
// Private helpers – features
// --------------------------------------------------------------------------

fn fillFeatures(vki: anytype, info: *PhysicalDeviceInfo) void {
    info.features_vk_1_2 = std.mem.zeroes(vk.PhysicalDeviceVulkan12Features);
    info.features_vk_1_2.s_type = .physical_device_vulkan_1_2_features;

    info.features_vk_1_3 = std.mem.zeroes(vk.PhysicalDeviceVulkan13Features);
    info.features_vk_1_3.s_type = .physical_device_vulkan_1_3_features;
    info.features_vk_1_3.p_next = @ptrCast(&info.features_vk_1_2);

    info.features_vk_1_4 = std.mem.zeroes(vk.PhysicalDeviceVulkan14Features);
    info.features_vk_1_4.s_type = .physical_device_vulkan_1_4_features;
    info.features_vk_1_4.p_next = @ptrCast(&info.features_vk_1_3);

    info.shader_atomic_features = std.mem.zeroes(vk.PhysicalDeviceShaderAtomicFloatFeaturesEXT);
    info.shader_atomic_features.s_type = .physical_device_shader_atomic_float_features_ext;
    info.shader_atomic_features.p_next = @ptrCast(&info.features_vk_1_4);

    info.primitive_restart_features = std.mem.zeroes(vk.PhysicalDevicePrimitiveTopologyListRestartFeaturesEXT);
    info.primitive_restart_features.s_type = .physical_device_primitive_topology_list_restart_features_ext;
    info.primitive_restart_features.p_next = @ptrCast(&info.shader_atomic_features);

    var features2 = vk.PhysicalDeviceFeatures2{
        .s_type = .physical_device_features_2,
        .p_next = @ptrCast(&info.primitive_restart_features),
        .features = undefined,
    };
    vki.getPhysicalDeviceFeatures2(info.handle, &features2);

    info.features = features2.features;
}

// --------------------------------------------------------------------------
// Private helpers – queue families
// --------------------------------------------------------------------------

fn fillQueues(
    vki: anytype,
    info: *PhysicalDeviceInfo,
    surface: vk.SurfaceKHR,
    allocator: std.mem.Allocator,
) !void {
    info.graphics_queue_family_index = std.math.maxInt(u32);
    info.present_queue_family_index = std.math.maxInt(u32);

    var count: u32 = 0;
    vki.getPhysicalDeviceQueueFamilyProperties(info.handle, &count, null);

    if (count == 0) return error.NoQueueFamilies;

    const queue_props = try allocator.alloc(vk.QueueFamilyProperties, count);
    defer allocator.free(queue_props);
    vki.getPhysicalDeviceQueueFamilyProperties(info.handle, &count, queue_props.ptr);

    // Prefer a single family that handles both graphics and present (most GPUs).
    for (queue_props, 0..) |family, i| {
        const idx: u32 = @intCast(i);

        const has_graphics = family.queue_flags.graphics_bit and family.queue_count > 0;
        const present_support = try vki.getPhysicalDeviceSurfaceSupportKHR(
            info.handle,
            idx,
            surface,
        );

        if (has_graphics) {
            if (info.graphics_queue_family_index == std.math.maxInt(u32)) {
                info.graphics_queue_family_index = idx;
            }

            if (present_support == .true) {
                // Combined family – ideal, return immediately.
                info.graphics_queue_family_index = idx;
                info.present_queue_family_index = idx;
                return;
            }
        }
    }

    // No combined family; look for any family with present support.
    for (queue_props, 0..) |_, i| {
        const idx: u32 = @intCast(i);
        const present_support = try vki.getPhysicalDeviceSurfaceSupportKHR(
            info.handle,
            idx,
            surface,
        );
        if (present_support == .true) {
            info.present_queue_family_index = idx;
            break;
        }
    }
}

// --------------------------------------------------------------------------
// Private helpers – macro features
// --------------------------------------------------------------------------

/// Check whether the device supports writing to a depth image from a compute
/// shader (not universally supported).
fn checkComputeStoresToDepth(vki: anytype, handle: vk.PhysicalDevice) bool {
    const format_info = vk.PhysicalDeviceImageFormatInfo2{
        .s_type = .physical_device_image_format_info_2,
        .p_next = null,
        .format = .d16_unorm,
        .type = .@"2d",
        .tiling = .optimal,
        .usage = .{ .storage_bit = true, .depth_stencil_attachment_bit = true },
        .flags = .{},
    };

    var props = vk.ImageFormatProperties2{
        .s_type = .image_format_properties_2,
        .p_next = null,
        .image_format_properties = undefined,
    };

    const result = vki.getPhysicalDeviceImageFormatProperties2(handle, &format_info, &props) catch |err| switch (err) {
        error.FormatNotSupported => return false,
        else => return false,
    };
    _ = result;
    return true;
}

fn fillMacroFeatures(vki: anytype, handle: vk.PhysicalDevice) PhysicalDeviceInfo.MacroFeatures {
    return .{
        .compute_stores_to_depth = checkComputeStoresToDepth(vki, handle),
    };
}

// --------------------------------------------------------------------------
// Validation
// --------------------------------------------------------------------------

/// Returns false when the device should be rejected, panics on missing
/// required features (mirrors the C++ Assert() pattern).
pub fn checkPhysicalDevice(
    vki: anytype,
    info: *const PhysicalDeviceInfo,
    required_extensions: []const [*:0]const u8,
    allocator: std.mem.Allocator,
) !bool {
    // Skip AMD cards (vendor 0x1002) – same policy as C++ backend.
    if (info.properties.vendor_id == 0x1002) {
        return false;
    }

    // ---- API version ----
    if (info.properties.api_version < @as(u32, @bitCast(vk.makeApiVersion(
        0,
        build_options.vulkan_api_major,
        build_options.vulkan_api_minor,
        build_options.vulkan_api_patch,
    ))))
        return error.UnsupportedVulkanVersion;

    // ---- Subgroup properties (1.1) ----
    const sg = info.properties_vk_1_1.subgroup_supported_operations;
    if (!sg.contains(.{ .basic_bit = true })) return error.MissingSubgroupBasic;
    if (!sg.contains(.{ .ballot_bit = true })) return error.MissingSubgroupBallot;
    if (!sg.contains(.{ .vote_bit = true })) return error.MissingSubgroupVote;
    if (!sg.contains(.{ .shuffle_bit = true })) return error.MissingSubgroupShuffle;

    // ---- Subgroup size (1.3) ----
    if (info.properties_vk_1_3.min_subgroup_size < 8) return error.SubgroupTooSmall;
    if (info.properties_vk_1_3.max_subgroup_size > 64) return error.SubgroupTooLarge;

    // ---- Core features ----
    if (info.features.sampler_anisotropy != .true) return error.MissingFeatureSamplerAnisotropy;
    if (info.features.multi_draw_indirect != .true) return error.MissingFeatureMultiDrawIndirect;
    if (info.features.draw_indirect_first_instance != .true) return error.MissingFeatureDrawIndirectFirstInstance;
    if (info.features.fragment_stores_and_atomics != .true) return error.MissingFeatureFragmentStoresAndAtomics;
    if (info.features.fill_mode_non_solid != .true) return error.MissingFeatureFillModeNonSolid;
    if (info.features.geometry_shader != .true) return error.MissingFeatureGeometryShader;

    // ---- 1.2 features ----
    if (info.features_vk_1_2.draw_indirect_count != .true) return error.MissingFeatureDrawIndirectCount;
    if (info.features_vk_1_2.imageless_framebuffer != .true) return error.MissingFeatureImagelessFramebuffer;
    if (info.features_vk_1_2.separate_depth_stencil_layouts != .true) return error.MissingFeatureSeparateDepthStencilLayouts;
    if (info.features_vk_1_2.descriptor_indexing != .true) return error.MissingFeatureDescriptorIndexing;
    if (info.features_vk_1_2.runtime_descriptor_array != .true) return error.MissingFeatureRuntimeDescriptorArray;
    if (info.features_vk_1_2.descriptor_binding_partially_bound != .true) return error.MissingFeatureDescriptorBindingPartiallyBound;
    if (info.features_vk_1_2.timeline_semaphore != .true) return error.MissingFeatureTimelineSemaphore;
    if (info.features_vk_1_2.shader_sampled_image_array_non_uniform_indexing != .true) return error.MissingFeatureShaderSampledImageNonUniform;

    // ---- 1.3 features ----
    if (info.features_vk_1_3.synchronization_2 != .true) return error.MissingFeatureSynchronization2;
    if (info.features_vk_1_3.dynamic_rendering != .true) return error.MissingFeatureDynamicRendering;
    if (info.features_vk_1_3.subgroup_size_control != .true) return error.MissingFeatureSubgroupSizeControl;

    // ---- 1.4 features ----
    if (info.features_vk_1_4.index_type_uint_8 != .true) return error.MissingFeatureIndexTypeUint8;
    if (info.features_vk_1_4.maintenance_5 != .true) return error.MissingFeatureMaintenance5;

    // ---- EXT features ----
    if (info.primitive_restart_features.primitive_topology_list_restart != .true)
        return error.MissingFeaturePrimitiveTopologyListRestart;

    // ---- Queue families ----
    if (info.graphics_queue_family_index == std.math.maxInt(u32)) return error.NoGraphicsQueue;
    if (info.present_queue_family_index == std.math.maxInt(u32)) return error.NoPresentQueue;

    // ---- Multisample grid (EXT_sample_locations) ----
    var ms_props = vk.MultisamplePropertiesEXT{
        .s_type = .multisample_properties_ext,
        .p_next = null,
        .max_sample_location_grid_size = undefined,
    };
    vki.getPhysicalDeviceMultisamplePropertiesEXT(info.handle, .{ .@"8_bit" = true }, &ms_props);
    if (ms_props.max_sample_location_grid_size.height < 1) return error.BadMultisampleGrid;
    if (ms_props.max_sample_location_grid_size.width < 1) return error.BadMultisampleGrid;

    // ---- Device extension support ----
    try checkDeviceExtensions(vki, info.handle, required_extensions, allocator);

    return true;
}

fn checkDeviceExtensions(
    vki: anytype,
    handle: vk.PhysicalDevice,
    required: []const [*:0]const u8,
    allocator: std.mem.Allocator,
) !void {
    if (required.len == 0) return;

    var count: u32 = 0;
    _ = try vki.enumerateDeviceExtensionProperties(handle, null, &count, null);

    const props = try allocator.alloc(vk.ExtensionProperties, count);
    defer allocator.free(props);
    _ = try vki.enumerateDeviceExtensionProperties(handle, null, &count, props.ptr);

    for (required) |req| {
        const req_name = std.mem.span(req);
        var found = false;
        for (props) |p| {
            const ext_name = std.mem.sliceTo(&p.extension_name, 0);
            if (std.mem.eql(u8, ext_name, req_name)) {
                found = true;
                break;
            }
        }
        if (!found) {
            log.err("vulkan: device extension '{s}' not supported", .{req_name});
            return error.MissingDeviceExtension;
        }
    }
}

// --------------------------------------------------------------------------
// Debug logging
// --------------------------------------------------------------------------

pub fn logPhysicalDevice(info: *const PhysicalDeviceInfo) void {
    log.info("vulkan: selecting device '{s}'", .{std.mem.sliceTo(&info.properties.device_name, 0)});

    const api: vk.Version = @bitCast(info.properties.api_version);
    log.debug("- api version = {}.{}.{}", .{ api.major, api.minor, api.patch });

    const driver: vk.Version = @bitCast(info.properties.driver_version);
    const driver_name = std.mem.sliceTo(&info.properties_vk_1_2.driver_name, 0);
    log.debug("- driver '{s}' version = {}.{}.{}", .{ driver_name, driver.major, driver.minor, driver.patch });

    log.debug("- subgroup min size = {}", .{info.properties_vk_1_3.min_subgroup_size});
    log.debug("- subgroup max size = {}", .{info.properties_vk_1_3.max_subgroup_size});

    log.debug("- memory type count = {}, memory heap count = {}", .{
        info.memory_properties.memory_type_count,
        info.memory_properties.memory_heap_count,
    });
}
