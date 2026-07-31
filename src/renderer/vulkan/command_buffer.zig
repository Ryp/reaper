// Port of src/renderer/vulkan/CommandBuffer.h

const vk = @import("vulkan");

pub const CommandBuffer = struct {
    handle: vk.CommandBuffer,

    // The C++ struct also carries a tracy::VkCtx* under REAPER_USE_TRACY.
    // GPU profiling is not wired up yet, so there is nothing to mirror here.
};
