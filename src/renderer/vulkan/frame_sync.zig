// Port of src/renderer/vulkan/FrameSync.h + FrameSync.cpp
//
// A single timeline semaphore paces the CPU against the GPU: frame N submits
// with a signal value of N, and the next frame waits on N before recording.

const std = @import("std");
const vk = @import("vulkan");

pub const FrameSyncResources = struct {
    timeline_semaphore: vk.Semaphore,
};

pub fn create(vkd: anytype, device: vk.Device) !FrameSyncResources {
    const semaphore_type_create_info = vk.SemaphoreTypeCreateInfo{
        .s_type = .semaphore_type_create_info,
        .p_next = null,
        .semaphore_type = .timeline,
        .initial_value = 0,
    };

    const timeline_semaphore_create_info = vk.SemaphoreCreateInfo{
        .s_type = .semaphore_create_info,
        .p_next = @ptrCast(&semaphore_type_create_info),
        .flags = .{},
    };

    return .{
        .timeline_semaphore = try vkd.createSemaphore(device, &timeline_semaphore_create_info, null),
    };
}

pub fn destroy(vkd: anytype, device: vk.Device, resources: FrameSyncResources) void {
    vkd.destroySemaphore(device, resources.timeline_semaphore, null);
}
