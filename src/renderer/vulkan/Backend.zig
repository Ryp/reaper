// Port of src/renderer/vulkan/Backend.h + Backend.cpp
//
// Owns the top-level Vulkan objects that outlive any individual frame:
// instance, device, queues, descriptor pool, VMA allocator, and swapchain.
//
// The three vulkan-zig dispatch tables (BaseDispatch / InstanceDispatch /
// DeviceDispatch) replace the C++ manual function-pointer loading.
// SDL3 replaces IWindow and the platform-surface helpers.

const std = @import("std");
const vk = @import("vulkan");
const build_options = @import("build_options");
const log = std.log.scoped(.vulkan);

const PhysicalDevice = @import("PhysicalDevice.zig");
const Swapchain = @import("Swapchain.zig");

// --------------------------------------------------------------------------
// SDL3 – only the Vulkan-specific surface we need.
// Include paths come from the SDL3 artifact linked in build.zig.
// --------------------------------------------------------------------------
pub const sdl = @cImport({
    @cInclude("SDL3/SDL.h");
    @cInclude("SDL3/SDL_vulkan.h");
});

// --------------------------------------------------------------------------
// VMA – header-only C library compiled via vma_impl.c.
// VK_NO_PROTOTYPES keeps static Vulkan function declarations out of the
// header; we supply function pointers through VmaVulkanFunctions instead.
// --------------------------------------------------------------------------
const vma = @cImport({
    @cDefine("VK_NO_PROTOTYPES", "1");
    @cDefine("VMA_STATIC_VULKAN_FUNCTIONS", "0");
    @cDefine("VMA_DYNAMIC_VULKAN_FUNCTIONS", "1");
    @cInclude("vk_mem_alloc.h");
});

// --------------------------------------------------------------------------
// Vulkan dispatch tables – pre-built by vulkan-zig from vk.xml at build time.
// All extensions are included in the generated vk.zig; nothing to enumerate
// here anymore.
// --------------------------------------------------------------------------
pub const BaseDispatch = vk.BaseWrapper;
pub const InstanceDispatch = vk.InstanceWrapper;
pub const DeviceDispatch = vk.DeviceWrapper;

// --------------------------------------------------------------------------
// VulkanBackend  (mirrors VulkanBackend in Backend.h)
// --------------------------------------------------------------------------

pub const Options = struct {
    freeze_meshlet_culling: bool = false,
    enable_debug_tile_lighting: bool = true,
    enable_msaa_visibility: bool = false,
};

pub const VulkanBackend = struct {
    allocator: std.mem.Allocator,

    // Dispatch tables replace the C++ global function pointers.
    vkb: BaseDispatch,
    vki: InstanceDispatch,
    vkd: DeviceDispatch,

    instance: vk.Instance,
    physical_device: PhysicalDevice.PhysicalDeviceInfo,
    device: vk.Device,

    // These can alias the same handle when graphics == present family.
    graphics_queue: vk.Queue,
    present_queue: vk.Queue,

    global_descriptor_pool: vk.DescriptorPool,

    vma_instance: vma.VmaAllocator,

    present_info: Swapchain.PresentationInfo,
    render_extent: vk.Extent2D,

    // Signalled when a swapchain image is available; not a timeline semaphore
    // (Vulkan spec does not allow timeline sems for present operations).
    semaphore_swapchain_image_available: vk.Semaphore,

    debug_messenger: ?vk.DebugUtilsMessengerEXT,

    new_swapchain_extent: vk.Extent2D,
    frame_index: u64,

    options: Options,

    // --------------------------------------------------------------------------
    // Init / deinit
    // --------------------------------------------------------------------------

    /// Mirrors create_vulkan_renderer_backend().
    /// `window` must have been created with SDL_WINDOW_VULKAN.
    pub fn init(
        allocator: std.mem.Allocator,
        window: *sdl.SDL_Window,
        window_width: u32,
        window_height: u32,
    ) !VulkanBackend {
        log.info("vulkan: creating backend", .{});

        // ----------------------------------------------------------------
        // Load Vulkan library via SDL; get the bootstrap proc-addr pointer.
        // ----------------------------------------------------------------
        if (!sdl.SDL_Vulkan_LoadLibrary(null)) {
            log.err("SDL_Vulkan_LoadLibrary failed: {s}", .{sdl.SDL_GetError()});
            return error.VulkanLoadFailed;
        }
        errdefer sdl.SDL_Vulkan_UnloadLibrary();

        const raw_proc_addr = sdl.SDL_Vulkan_GetVkGetInstanceProcAddr() orelse
            return error.NoVkGetInstanceProcAddr;

        const get_proc_addr: vk.PfnGetInstanceProcAddr = @ptrCast(raw_proc_addr);

        const vkb = BaseDispatch.load(get_proc_addr);

        // ----------------------------------------------------------------
        // Instance extensions
        // ----------------------------------------------------------------
        var sdl_ext_count: u32 = 0;
        const sdl_exts_ptr = sdl.SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
        if (sdl_exts_ptr == null and sdl_ext_count > 0) return error.SDLGetExtensionsFailed;

        var instance_extensions = try std.ArrayList([*:0]const u8).initCapacity(allocator, sdl_ext_count + 4);
        defer instance_extensions.deinit(allocator);

        if (sdl_exts_ptr) |p| {
            for (p[0..sdl_ext_count]) |ext| try instance_extensions.append(allocator, ext);
        }

        try instance_extensions.append(allocator, "VK_KHR_get_surface_capabilities2");
        try instance_extensions.append(allocator, "VK_EXT_swapchain_colorspace");

        if (build_options.enable_validation) {
            try instance_extensions.append(allocator, "VK_EXT_debug_utils");
        }

        try checkInstanceExtensions(vkb, instance_extensions.items, allocator);

        // ----------------------------------------------------------------
        // Instance layers
        // ----------------------------------------------------------------
        var instance_layers = try std.ArrayList([*:0]const u8).initCapacity(allocator, 1);
        defer instance_layers.deinit(allocator);

        if (build_options.enable_validation) {
            try instance_layers.append(allocator, "VK_LAYER_KHRONOS_validation");
        }

        try checkInstanceLayers(vkb, instance_layers.items, allocator);

        log.info("vulkan: using {} instance extensions, {} layers", .{
            instance_extensions.items.len, instance_layers.items.len,
        });

        // ----------------------------------------------------------------
        // Create VkInstance
        // ----------------------------------------------------------------
        const app_ver = vk.makeApiVersion(
            0,
            build_options.app_version_major,
            build_options.app_version_minor,
            build_options.app_version_patch,
        );

        const app_info = vk.ApplicationInfo{
            .s_type = .application_info,
            .p_next = null,
            .p_application_name = "Reaper",
            .application_version = @bitCast(app_ver),
            .p_engine_name = "Reaper",
            .engine_version = @bitCast(app_ver),
            .api_version = @bitCast(vk.makeApiVersion(
                0,
                build_options.vulkan_api_major,
                build_options.vulkan_api_minor,
                build_options.vulkan_api_patch,
            )),
        };

        const instance_create_info = vk.InstanceCreateInfo{
            .s_type = .instance_create_info,
            .p_next = null,
            .flags = .{},
            .p_application_info = &app_info,
            .enabled_layer_count = @intCast(instance_layers.items.len),
            .pp_enabled_layer_names = instance_layers.items.ptr,
            .enabled_extension_count = @intCast(instance_extensions.items.len),
            .pp_enabled_extension_names = instance_extensions.items.ptr,
        };

        const instance = try vkb.createInstance(&instance_create_info, null);
        const vki = InstanceDispatch.load(instance, get_proc_addr);
        errdefer vki.destroyInstance(instance, null);

        log.info("vulkan: instance created (API 1.4)", .{});

        // ----------------------------------------------------------------
        // Debug messenger
        // ----------------------------------------------------------------
        const debug_messenger: ?vk.DebugUtilsMessengerEXT = if (build_options.enable_validation)
            try createDebugMessenger(vki, instance)
        else
            null;
        errdefer if (debug_messenger) |m| vki.destroyDebugUtilsMessengerEXT(instance, m, null);

        // ----------------------------------------------------------------
        // Presentation surface (SDL owns the lifetime)
        // ----------------------------------------------------------------
        const surface = try createSurface(vki, instance, window);
        errdefer vki.destroySurfaceKHR(instance, surface, null);

        // ----------------------------------------------------------------
        // Device extensions we require
        // ----------------------------------------------------------------
        const device_extensions = [_][*:0]const u8{
            "VK_KHR_swapchain",
            "VK_KHR_swapchain_mutable_format",
            "VK_EXT_calibrated_timestamps",
            "VK_EXT_primitive_topology_list_restart",
            "VK_EXT_sample_locations",
        };

        // ----------------------------------------------------------------
        // Physical device selection
        // ----------------------------------------------------------------
        var phys_device_count: u32 = 0;
        _ = try vki.enumeratePhysicalDevices(instance, &phys_device_count, null);
        if (phys_device_count == 0) return error.NoVulkanDevices;

        const phys_devices = try allocator.alloc(vk.PhysicalDevice, phys_device_count);
        defer allocator.free(phys_devices);
        _ = try vki.enumeratePhysicalDevices(instance, &phys_device_count, phys_devices.ptr);

        log.debug("vulkan: enumerating {} physical devices", .{phys_device_count});

        var physical_device_info: PhysicalDevice.PhysicalDeviceInfo = undefined;
        var found = false;

        for (phys_devices) |pd| {
            const info = try PhysicalDevice.createPhysicalDeviceInfo(vki, pd, surface, allocator);
            const accepted = PhysicalDevice.checkPhysicalDevice(vki, &info, &device_extensions, allocator) catch |err| {
                log.debug("vulkan: device rejected: {}", .{err});
                continue;
            };
            if (accepted) {
                physical_device_info = info;
                found = true;
                break;
            }
        }

        if (!found) return error.NoSuitablePhysicalDevice;
        PhysicalDevice.logPhysicalDevice(&physical_device_info);

        // ----------------------------------------------------------------
        // Logical device
        // ----------------------------------------------------------------
        const device = try createDevice(vki, &physical_device_info, &device_extensions);
        const vkd = DeviceDispatch.load(device, vki.dispatch.vkGetDeviceProcAddr.?);
        errdefer vkd.destroyDevice(device, null);

        const graphics_queue = vkd.getDeviceQueue(device, physical_device_info.graphics_queue_family_index, 0);
        const present_queue = vkd.getDeviceQueue(device, physical_device_info.present_queue_family_index, 0);

        // ----------------------------------------------------------------
        // Global descriptor pool
        // ----------------------------------------------------------------
        const global_descriptor_pool = try createGlobalDescriptorPool(vkd, device);
        errdefer vkd.destroyDescriptorPool(device, global_descriptor_pool, null);

        // ----------------------------------------------------------------
        // VMA allocator
        // ----------------------------------------------------------------
        const vma_instance = try createVmaAllocator(vki, &physical_device_info, instance, device, get_proc_addr);
        errdefer vma.vmaDestroyAllocator(vma_instance);

        // ----------------------------------------------------------------
        // Swapchain
        // ----------------------------------------------------------------
        var present_info = Swapchain.PresentationInfo{ .surface = surface };

        const swapchain_desc = Swapchain.SwapchainDescriptor{
            .preferred_image_count = 3,
            .preferred_format = .{ .format = .undefined, .color_space = .srgb_nonlinear_khr },
            .preferred_extent = .{ .width = window_width, .height = window_height },
        };

        try Swapchain.configureVulkanWmSwapchain(
            vki,
            physical_device_info.handle,
            swapchain_desc,
            &present_info,
            allocator,
        );
        try Swapchain.createVulkanWmSwapchain(vkd, device, &present_info, allocator);
        errdefer Swapchain.destroyVulkanWmSwapchain(vkd, device, &present_info, allocator);

        const render_extent = setRenderResolution(&present_info);

        // ----------------------------------------------------------------
        // image-available semaphore
        // ----------------------------------------------------------------
        const sem_info = vk.SemaphoreCreateInfo{
            .s_type = .semaphore_create_info,
            .p_next = null,
            .flags = .{},
        };
        const semaphore_swapchain_image_available = try vkd.createSemaphore(device, &sem_info, null);
        errdefer vkd.destroySemaphore(device, semaphore_swapchain_image_available, null);

        log.info("vulkan: ready", .{});

        return VulkanBackend{
            .allocator = allocator,
            .vkb = vkb,
            .vki = vki,
            .vkd = vkd,
            .instance = instance,
            .physical_device = physical_device_info,
            .device = device,
            .graphics_queue = graphics_queue,
            .present_queue = present_queue,
            .global_descriptor_pool = global_descriptor_pool,
            .vma_instance = vma_instance,
            .present_info = present_info,
            .render_extent = render_extent,
            .semaphore_swapchain_image_available = semaphore_swapchain_image_available,
            .debug_messenger = debug_messenger,
            .new_swapchain_extent = .{ .width = 0, .height = 0 },
            .frame_index = 0,
            .options = .{},
        };
    }

    /// Mirrors destroy_vulkan_renderer_backend() – reverse of init().
    pub fn deinit(self: *VulkanBackend) void {
        log.info("vulkan: destroying backend", .{});

        _ = self.vkd.deviceWaitIdle(self.device) catch {};

        self.vkd.destroySemaphore(self.device, self.semaphore_swapchain_image_available, null);

        Swapchain.destroyVulkanWmSwapchain(self.vkd, self.device, &self.present_info, self.allocator);

        log.debug("vulkan: destroying vma allocator", .{});
        vma.vmaDestroyAllocator(self.vma_instance);

        log.debug("vulkan: destroying global descriptor pool", .{});
        self.vkd.destroyDescriptorPool(self.device, self.global_descriptor_pool, null);

        log.debug("vulkan: destroying logical device", .{});
        self.vkd.destroyDevice(self.device, null);

        log.debug("vulkan: destroying surface", .{});
        self.vki.destroySurfaceKHR(self.instance, self.present_info.surface, null);

        if (self.debug_messenger) |m| {
            log.debug("vulkan: detaching debug messenger", .{});
            self.vki.destroyDebugUtilsMessengerEXT(self.instance, m, null);
        }

        self.vki.destroyInstance(self.instance, null);

        sdl.SDL_Vulkan_UnloadLibrary();
        log.info("vulkan: backend destroyed", .{});
    }

    /// Mirrors set_backend_render_resolution().
    pub fn updateRenderExtent(self: *VulkanBackend) void {
        self.render_extent = setRenderResolution(&self.present_info);
    }
};

// --------------------------------------------------------------------------
// Private helpers
// --------------------------------------------------------------------------

fn setRenderResolution(present_info: *const Swapchain.PresentationInfo) vk.Extent2D {
    // The render extent tracks the surface extent; callers can apply
    // a scaling factor here in the future.
    return present_info.surface_extent;
}

fn checkInstanceExtensions(
    vkb: BaseDispatch,
    required: []const [*:0]const u8,
    allocator: std.mem.Allocator,
) !void {
    if (required.len == 0) return;

    var count: u32 = 0;
    _ = try vkb.enumerateInstanceExtensionProperties(null, &count, null);

    const props = try allocator.alloc(vk.ExtensionProperties, count);
    defer allocator.free(props);
    _ = try vkb.enumerateInstanceExtensionProperties(null, &count, props.ptr);

    for (required) |req| {
        const req_name = std.mem.span(req);
        var found = false;
        for (props) |p| {
            if (std.mem.eql(u8, std.mem.sliceTo(&p.extension_name, 0), req_name)) {
                found = true;
                break;
            }
        }
        if (!found) {
            log.err("vulkan: instance extension '{s}' not supported", .{req_name});
            return error.MissingInstanceExtension;
        }
    }
}

fn checkInstanceLayers(
    vkb: BaseDispatch,
    required: []const [*:0]const u8,
    allocator: std.mem.Allocator,
) !void {
    if (required.len == 0) return;

    var count: u32 = 0;
    _ = try vkb.enumerateInstanceLayerProperties(&count, null);

    const props = try allocator.alloc(vk.LayerProperties, count);
    defer allocator.free(props);
    _ = try vkb.enumerateInstanceLayerProperties(&count, props.ptr);

    for (required) |req| {
        const req_name = std.mem.span(req);
        var found = false;
        for (props) |p| {
            if (std.mem.eql(u8, std.mem.sliceTo(&p.layer_name, 0), req_name)) {
                found = true;
                break;
            }
        }
        if (!found) {
            log.err("vulkan: instance layer '{s}' not available", .{req_name});
            return error.MissingInstanceLayer;
        }
    }
}

fn createDebugMessenger(vki: InstanceDispatch, instance: vk.Instance) !vk.DebugUtilsMessengerEXT {
    const callback = struct {
        fn debugCallback(
            severity: vk.DebugUtilsMessageSeverityFlagsEXT,
            _: vk.DebugUtilsMessageTypeFlagsEXT,
            data: ?*const vk.DebugUtilsMessengerCallbackDataEXT,
            _: ?*anyopaque,
        ) callconv(vk.vulkan_call_conv) vk.Bool32 {
            const msg: []const u8 = if (data) |d|
                std.mem.span(d.p_message orelse "(no message)")
            else
                "(no message)";
            if (severity.error_bit_ext) {
                log.err("[Vulkan] {s}", .{msg});
            } else if (severity.warning_bit_ext) {
                log.warn("[Vulkan] {s}", .{msg});
            } else if (severity.info_bit_ext) {
                log.info("[Vulkan] {s}", .{msg});
            } else {
                log.debug("[Vulkan] {s}", .{msg});
            }
            return .false;
        }
    }.debugCallback;

    const create_info = vk.DebugUtilsMessengerCreateInfoEXT{
        .s_type = .debug_utils_messenger_create_info_ext,
        .p_next = null,
        .flags = .{},
        .message_severity = .{
            .verbose_bit_ext = true,
            .info_bit_ext = true,
            .warning_bit_ext = true,
            .error_bit_ext = true,
        },
        .message_type = .{
            .general_bit_ext = true,
            .validation_bit_ext = true,
            .performance_bit_ext = true,
        },
        .pfn_user_callback = callback,
        .p_user_data = null,
    };

    const messenger = try vki.createDebugUtilsMessengerEXT(instance, &create_info, null);
    log.info("vulkan: debug messenger attached", .{});
    return messenger;
}

fn createSurface(vki: InstanceDispatch, instance: vk.Instance, window: *sdl.SDL_Window) !vk.SurfaceKHR {
    // SDL expects a raw VkInstance pointer. Convert from vulkan-zig's enum(usize).
    const c_instance: sdl.VkInstance = @ptrFromInt(@intFromEnum(instance));

    var c_surface: sdl.VkSurfaceKHR = undefined;
    if (!sdl.SDL_Vulkan_CreateSurface(window, c_instance, null, &c_surface)) {
        log.err("SDL_Vulkan_CreateSurface failed: {s}", .{sdl.SDL_GetError()});
        return error.SurfaceCreationFailed;
    }
    _ = vki; // surface is already valid; vki is unused here but kept for symmetry

    // VkSurfaceKHR on 64-bit is a handle-as-pointer (8 bytes).  vulkan-zig
    // models non-dispatchable handles as enum(u64).
    const surface: vk.SurfaceKHR = @enumFromInt(@intFromPtr(c_surface));
    log.info("vulkan: surface created", .{});
    return surface;
}

fn createDevice(
    vki: InstanceDispatch,
    physical_device: *const PhysicalDevice.PhysicalDeviceInfo,
    extensions: []const [*:0]const u8,
) !vk.Device {
    // Build queue create-info list.  When graphics == present we need only
    // one entry; otherwise two.
    const queue_priority = [_]f32{1.0};
    var queue_infos_buf: [2]vk.DeviceQueueCreateInfo = undefined;
    var queue_count: u32 = 1;

    queue_infos_buf[0] = .{
        .s_type = .device_queue_create_info,
        .p_next = null,
        .flags = .{},
        .queue_family_index = physical_device.graphics_queue_family_index,
        .queue_count = 1,
        .p_queue_priorities = &queue_priority,
    };

    if (physical_device.graphics_queue_family_index != physical_device.present_queue_family_index) {
        queue_infos_buf[1] = .{
            .s_type = .device_queue_create_info,
            .p_next = null,
            .flags = .{},
            .queue_family_index = physical_device.present_queue_family_index,
            .queue_count = 1,
            .p_queue_priorities = &queue_priority,
        };
        queue_count = 2;
    }

    // Build the pNext feature chain (bottom → top so head pointer is set last).
    var primitive_restart = vk.PhysicalDevicePrimitiveTopologyListRestartFeaturesEXT{
        .s_type = .physical_device_primitive_topology_list_restart_features_ext,
        .p_next = null,
        .primitive_topology_list_restart = .true,
        .primitive_topology_patch_list_restart = .false,
    };

    var shader_atomic = vk.PhysicalDeviceShaderAtomicFloatFeaturesEXT{
        .s_type = .physical_device_shader_atomic_float_features_ext,
        .p_next = @ptrCast(&primitive_restart),
        // Let the driver enable what it supports; we just need the struct in chain.
        .shader_buffer_float_32_atomics = .false,
        .shader_buffer_float_32_atomic_add = .false,
        .shader_buffer_float_64_atomics = .false,
        .shader_buffer_float_64_atomic_add = .false,
        .shader_shared_float_32_atomics = .false,
        .shader_shared_float_32_atomic_add = .false,
        .shader_shared_float_64_atomics = .false,
        .shader_shared_float_64_atomic_add = .false,
        .shader_image_float_32_atomics = .false,
        .shader_image_float_32_atomic_add = .false,
        .sparse_image_float_32_atomics = .false,
        .sparse_image_float_32_atomic_add = .false,
    };

    var features_1_4 = vk.PhysicalDeviceVulkan14Features{
        .s_type = .physical_device_vulkan_1_4_features,
        .p_next = @ptrCast(&shader_atomic),
        .index_type_uint_8 = .true,
        .maintenance_5 = .true,
    };

    var features_1_3 = vk.PhysicalDeviceVulkan13Features{
        .s_type = .physical_device_vulkan_1_3_features,
        .p_next = @ptrCast(&features_1_4),
        .synchronization_2 = .true,
        .dynamic_rendering = .true,
        .subgroup_size_control = .true,
    };

    var features_1_2 = vk.PhysicalDeviceVulkan12Features{
        .s_type = .physical_device_vulkan_1_2_features,
        .p_next = @ptrCast(&features_1_3),
        .draw_indirect_count = .true,
        .imageless_framebuffer = .true,
        .separate_depth_stencil_layouts = .true,
        .descriptor_indexing = .true,
        .runtime_descriptor_array = .true,
        .descriptor_binding_partially_bound = .true,
        .timeline_semaphore = .true,
        .shader_sampled_image_array_non_uniform_indexing = .true,
    };

    // PhysicalDeviceFeatures2 is the pNext chain head; pEnabledFeatures is null.
    var device_features2 = vk.PhysicalDeviceFeatures2{
        .s_type = .physical_device_features_2,
        .p_next = @ptrCast(&features_1_2),
        .features = .{
            .sampler_anisotropy = .true,
            .multi_draw_indirect = .true,
            .draw_indirect_first_instance = .true,
            .fragment_stores_and_atomics = .true,
            .fill_mode_non_solid = .true,
            .geometry_shader = .true,
        },
    };

    log.info("vulkan: using {} device extensions", .{extensions.len});
    for (extensions) |e| log.debug("- {s}", .{std.mem.span(e)});

    const device_create_info = vk.DeviceCreateInfo{
        .s_type = .device_create_info,
        .p_next = @ptrCast(&device_features2),
        .flags = .{},
        .queue_create_info_count = queue_count,
        .p_queue_create_infos = &queue_infos_buf,
        .enabled_layer_count = 0,
        .pp_enabled_layer_names = null,
        .enabled_extension_count = @intCast(extensions.len),
        .pp_enabled_extension_names = extensions.ptr,
        .p_enabled_features = null, // features are in the pNext chain above
    };

    const device = try vki.createDevice(physical_device.handle, &device_create_info, null);
    log.info("vulkan: logical device created", .{});
    return device;
}

fn createGlobalDescriptorPool(vkd: DeviceDispatch, device: vk.Device) !vk.DescriptorPool {
    const max_sets: u32 = 100;
    const pool_sizes = [_]vk.DescriptorPoolSize{
        .{ .type = .sampler, .descriptor_count = 16 },
        .{ .type = .sampled_image, .descriptor_count = 256 },
        .{ .type = .uniform_buffer, .descriptor_count = 32 },
        .{ .type = .storage_buffer, .descriptor_count = 32 },
        .{ .type = .storage_image, .descriptor_count = 32 },
        .{ .type = .combined_image_sampler, .descriptor_count = 16 },
    };

    const pool_info = vk.DescriptorPoolCreateInfo{
        .s_type = .descriptor_pool_create_info,
        .p_next = null,
        .flags = .{},
        .max_sets = max_sets,
        .pool_size_count = pool_sizes.len,
        .p_pool_sizes = &pool_sizes,
    };

    const pool = try vkd.createDescriptorPool(device, &pool_info, null);
    log.debug("vulkan: global descriptor pool created (max_sets={})", .{max_sets});
    return pool;
}

fn createVmaAllocator(
    vki: InstanceDispatch,
    physical_device: *const PhysicalDevice.PhysicalDeviceInfo,
    instance: vk.Instance,
    device: vk.Device,
    get_proc_addr: vk.PfnGetInstanceProcAddr,
) !vma.VmaAllocator {
    // Provide the two bootstrap pointers; VMA fetches everything else via
    // vkGetDeviceProcAddr at startup (VMA_DYNAMIC_VULKAN_FUNCTIONS=1).
    var vma_funcs = std.mem.zeroes(vma.VmaVulkanFunctions);
    vma_funcs.vkGetInstanceProcAddr = @ptrCast(get_proc_addr);
    vma_funcs.vkGetDeviceProcAddr = @ptrCast(vki.dispatch.vkGetDeviceProcAddr.?);

    var create_info = std.mem.zeroes(vma.VmaAllocatorCreateInfo);
    // Convert vulkan-zig opaque handles to C pointer types.
    create_info.physicalDevice = @ptrFromInt(@intFromEnum(physical_device.handle));
    create_info.device = @ptrFromInt(@intFromEnum(device));
    create_info.instance = @ptrFromInt(@intFromEnum(instance));
    create_info.pVulkanFunctions = &vma_funcs;
    create_info.vulkanApiVersion = @bitCast(vk.makeApiVersion(
        0,
        build_options.vulkan_api_major,
        build_options.vulkan_api_minor,
        build_options.vulkan_api_patch,
    ));

    var vma_instance: vma.VmaAllocator = undefined;
    const result = vma.vmaCreateAllocator(&create_info, &vma_instance);
    if (result != 0) { // 0 == VK_SUCCESS
        log.err("vmaCreateAllocator failed: {}", .{result});
        return error.VmaCreationFailed;
    }

    log.debug("vulkan: VMA allocator created", .{});
    return vma_instance;
}
