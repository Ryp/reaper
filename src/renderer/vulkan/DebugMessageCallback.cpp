////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "DebugMessageCallback.h"

#include "api/VulkanStringConversion.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include <core/Assert.h>

#include <span>
#include <vector>

namespace Reaper
{
namespace
{
    void print_debug_message(ReaperRoot* root, const VkDebugUtilsMessengerCallbackDataEXT* callback_data)
    {
        const char* id_name = callback_data->pMessageIdName;
        log_info(*root, "vulkan: debug message [id: {} ({:#010x})] {}", id_name ? id_name : "unnamed",
                 callback_data->messageIdNumber, callback_data->pMessage);

        for (auto& queue_label : std::span(callback_data->pQueueLabels, callback_data->queueLabelCount))
        {
            const char* label = queue_label.pLabelName;
            log_info(*root, "- queue label: '{}'", label ? label : "unnamed");
        }

        for (auto& command_buffer_label : std::span(callback_data->pCmdBufLabels, callback_data->cmdBufLabelCount))
        {
            const char* label = command_buffer_label.pLabelName;
            log_info(*root, "- command buffer label: '{}'", label ? label : "unnamed");
        }

        for (auto& object_name_info : std::span(callback_data->pObjects, callback_data->objectCount))
        {
            const char* label = object_name_info.pObjectName;
            log_info(*root, "- object '{}', type = {}, handle = {:#018x}", label ? label : "unnamed",
                     vk_to_string(object_name_info.objectType), object_name_info.objectHandle);
        }
    }

    VkBool32 VKAPI_PTR debug_message_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                              VkDebugUtilsMessageTypeFlagsEXT /*messageTypes*/,
                                              const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                                              void*                                       user_data)
    {
        ReaperRoot* root = static_cast<ReaperRoot*>(user_data);
        Assert(root != nullptr);

        constexpr i32 ID_LoaderMessage = 0x0000000;
        constexpr i32 ID_UNASSIGNED_BestPractices_vkCreateDevice_specialuse_extension_glemulation =
            -0x703c3ecb; // CreateDevice(): Attempting to enable extension VK_EXT_primitive_topology_list_restart, but
                         // this extension is intended to support OpenGL and/or OpenGL ES emulation layers, and
                         // applications ported from those APIs, by adding functionality specific to those APIs and it
                         // is strongly recommended that it be otherwise avoided.
        constexpr i32 ID_UNASSIGNED_BestPractices_vkAllocateMemory_small_allocation =
            -0x23e75295; // vkAllocateMemory(): Allocating a VkDeviceMemory of size 131072. This is a very small
                         // allocation (current threshold is 262144 bytes). You should make large allocations and
                         // sub-allocate from one large VkDeviceMemory.
        constexpr i32 ID_UNASSIGNED_BestPractices_vkBindMemory_small_dedicated_allocation =
            -0x4c2bcb95; // vkBindImageMemory(): Trying to bind VkImage 0xb9b24e0000000113[] to a memory block which is
                         // fully consumed by the image. The required size of the allocation is 131072, but smaller
                         // images like this should be sub-allocated from larger memory blocks. (Current threshold is
                         // 1048576 bytes.)
        constexpr i32 ID_UNASSIGNED_BestPractices_pipeline_stage_flags =
            0x48a09f6c; // You are using VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR when vkCmdResetEvent2 is called
        constexpr i32 ID_UNASSIGNED_BestPractices_CreatePipelines_AvoidPrimitiveRestart =
            0x4d6711e7; // [AMD] Performance warning: Use of primitive restart is not recommended
        constexpr i32 ID_UNASSIGNED_BestPractices_vkImage_DontUseStorageRenderTargets =
            -0x33200141; // [AMD] Performance warning: image 'Lighting' is created as a render target with
                         // VK_IMAGE_USAGE_STORAGE_BIT. Using a VK_IMAGE_USAGE_STORAGE_BIT is not recommended with color
                         // and depth targets
        constexpr i32 ID_UNASSIGNED_BestPractices_CreateDevice_PageableDeviceLocalMemory =
            0x2e99adca; // [NVIDIA] vkCreateDevice() called without pageable device local memory. Use
                        // pageableDeviceLocalMemory from VK_EXT_pageable_device_local_memory when it is available.
        constexpr i32 ID_UNASSIGNED_BestPractices_AllocateMemory_SetPriority =
            0x61f61757; // [NVIDIA] Use VkMemoryPriorityAllocateInfoEXT to provide the operating system information on
                        // the allocations that should stay in video memory and which should be demoted first when video
                        // memory is limited. The highest priority should be given to GPU-written resources like color
                        // attachments, depth attachments, storage images, and buffers written from the GPU.
        constexpr i32 ID_UNASSIGNED_BestPractices_CreatePipelineLayout_SeparateSampler =
            0x362cd642; // [NVIDIA] Consider using combined image samplers instead of separate samplers for marginally
                        // better performance.
        constexpr i32 ID_UNASSIGNED_BestPractices_Zcull_LessGreaterRatio =
            -0xa56a353; // [NVIDIA] Depth attachment VkImage 0xd22318000000014b[Tile Depth Max] is primarily rendered
                        // with depth compare op LESS, but some draws use GREATER. Z-cull is disabled for the least used
                        // direction, which harms depth testing performance. The Z-cull direction can be reset by
                        // clearing the depth attachment, transitioning from VK_IMAGE_LAYOUT_UNDEFINED, using
                        // VK_ATTACHMENT_LOAD_OP_DONT_CARE, or using VK_ATTACHMENT_STORE_OP_DONT_CARE.
        constexpr i32 ID_UNASSIGNED_BestPractices_AllocateMemory_ReuseAllocations =
            0x6e57f7a6; // [NVIDIA] Reuse memory allocations instead of releasing and reallocating. A memory allocation
                        // has just been released, and it could have been reused in place of this allocation.

        std::vector<i32> ignored_ids = {
            ID_LoaderMessage,
            ID_UNASSIGNED_BestPractices_vkCreateDevice_specialuse_extension_glemulation,
            ID_UNASSIGNED_BestPractices_vkAllocateMemory_small_allocation,
            ID_UNASSIGNED_BestPractices_vkBindMemory_small_dedicated_allocation,
            ID_UNASSIGNED_BestPractices_pipeline_stage_flags,
            ID_UNASSIGNED_BestPractices_CreatePipelines_AvoidPrimitiveRestart,
            ID_UNASSIGNED_BestPractices_vkImage_DontUseStorageRenderTargets,
            ID_UNASSIGNED_BestPractices_CreateDevice_PageableDeviceLocalMemory,
            ID_UNASSIGNED_BestPractices_AllocateMemory_SetPriority,
            ID_UNASSIGNED_BestPractices_CreatePipelineLayout_SeparateSampler,
            ID_UNASSIGNED_BestPractices_Zcull_LessGreaterRatio,
            ID_UNASSIGNED_BestPractices_AllocateMemory_ReuseAllocations,
        };

        bool ignore_message = false;
        for (auto ignored_id : ignored_ids)
        {
            if (ignored_id == callback_data->messageIdNumber)
                ignore_message = true;
        }

        if (!ignore_message)
        {
            print_debug_message(root, callback_data);
        }

        const bool ignore_assert = ignore_message || severity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        Assert(ignore_assert, callback_data->pMessage);

        bool exit_on_assert = false;
        if (!ignore_assert && exit_on_assert)
        {
            exit(EXIT_FAILURE);
        }

        return VK_FALSE;
    }
} // namespace

VkDebugUtilsMessengerEXT vulkan_create_debug_callback(VkInstance instance, ReaperRoot& root)
{
    VkDebugUtilsMessengerCreateInfoEXT callbackCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = VK_FLAGS_NONE,
        .messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &debug_message_callback,
        .pUserData = &root,
    };

    VkDebugUtilsMessengerEXT debug_utils_messenger = VK_NULL_HANDLE;

#if REAPER_DEBUG
    Assert(vkCreateDebugUtilsMessengerEXT(instance, &callbackCreateInfo, nullptr, &debug_utils_messenger)
           == VK_SUCCESS);
#endif

    return debug_utils_messenger;
}

void vulkan_destroy_debug_callback(VkInstance instance, VkDebugUtilsMessengerEXT debug_utils_messenger)
{
#if REAPER_DEBUG
    Assert(debug_utils_messenger != VK_NULL_HANDLE);
    vkDestroyDebugUtilsMessengerEXT(instance, debug_utils_messenger, nullptr);
#endif
}

} // namespace Reaper
