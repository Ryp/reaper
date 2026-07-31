// The one and only VMA @cImport.
//
// VK_NO_PROTOTYPES keeps the static Vulkan declarations out of the header;
// function pointers are supplied through VmaVulkanFunctions instead. The
// defines have to match the ones vma_impl.cpp is compiled with.

pub const c = @cImport({
    @cDefine("VK_NO_PROTOTYPES", "1");
    @cDefine("VMA_STATIC_VULKAN_FUNCTIONS", "0");
    @cDefine("VMA_DYNAMIC_VULKAN_FUNCTIONS", "1");
    @cInclude("vk_mem_alloc.h");
});
