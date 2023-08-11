////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "SamplerResources.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/Debug.h"
#include "renderer/vulkan/renderpass/ShadowConstants.h"

#include <common/Log.h>
#include <common/ReaperRoot.h>

#include <cfloat>
#include <core/Assert.h>

namespace Reaper
{
namespace
{
    VkSampler create_sampler(VulkanBackend& backend, const char* debug_name, const VkSamplerCreateInfo& create_info)
    {
        VkSampler sampler;
        Assert(vkCreateSampler(backend.device, &create_info, nullptr, &sampler) == VK_SUCCESS);

        VulkanSetDebugName(backend.device, sampler, debug_name);

        return sampler;
    }
} // namespace

SamplerResources create_sampler_resources(ReaperRoot& /*root*/, VulkanBackend& backend)
{
    SamplerResources resources;

    resources.linear_clamp = create_sampler(backend, "Linear Clamp Sampler",
                                            VkSamplerCreateInfo{
                                                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                                .pNext = nullptr,
                                                .flags = VK_FLAGS_NONE,
                                                .magFilter = VK_FILTER_LINEAR,
                                                .minFilter = VK_FILTER_LINEAR,
                                                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                                .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                .mipLodBias = 0.f,
                                                .anisotropyEnable = VK_FALSE,
                                                .maxAnisotropy = 0,
                                                .compareEnable = VK_FALSE,
                                                .compareOp = VK_COMPARE_OP_ALWAYS,
                                                .minLod = 0.f,
                                                .maxLod = FLT_MAX,
                                                .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                                                .unnormalizedCoordinates = VK_FALSE,
                                            });

    resources.linear_black_border = create_sampler(backend, "Linear Black Border Sampler",
                                                   VkSamplerCreateInfo{
                                                       .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                                       .pNext = nullptr,
                                                       .flags = VK_FLAGS_NONE,
                                                       .magFilter = VK_FILTER_LINEAR,
                                                       .minFilter = VK_FILTER_LINEAR,
                                                       .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                                       .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                       .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                       .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                       .mipLodBias = 0.f,
                                                       .anisotropyEnable = VK_FALSE,
                                                       .maxAnisotropy = 0,
                                                       .compareEnable = VK_FALSE,
                                                       .compareOp = VK_COMPARE_OP_ALWAYS,
                                                       .minLod = 0.f,
                                                       .maxLod = FLT_MAX,
                                                       .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                                                       .unnormalizedCoordinates = VK_FALSE,
                                                   });

    resources.shadow_map_sampler = create_sampler(
        backend, "Shadow Map Sampler",
        VkSamplerCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FLAGS_NONE,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .mipLodBias = 0.f,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 0,
            .compareEnable = VK_TRUE,
            .compareOp = ShadowUseReverseZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS,
            .minLod = 0.f,
            .maxLod = FLT_MAX,
            .borderColor = ShadowUseReverseZ ? VK_BORDER_COLOR_INT_OPAQUE_BLACK : VK_BORDER_COLOR_INT_OPAQUE_WHITE,
            .unnormalizedCoordinates = VK_FALSE,
        });

    resources.diffuse_map_sampler = create_sampler(backend, "Diffuse Map Sampler",
                                                   VkSamplerCreateInfo{
                                                       .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                                       .pNext = nullptr,
                                                       .flags = VK_FLAGS_NONE,
                                                       .magFilter = VK_FILTER_LINEAR,
                                                       .minFilter = VK_FILTER_LINEAR,
                                                       .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                                       .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                       .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                       .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                       .mipLodBias = 0.f,
                                                       .anisotropyEnable = VK_TRUE,
                                                       .maxAnisotropy = 8,
                                                       .compareEnable = VK_FALSE,
                                                       .compareOp = VK_COMPARE_OP_ALWAYS,
                                                       .minLod = 0.f,
                                                       .maxLod = FLT_MAX,
                                                       .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                                                       .unnormalizedCoordinates = VK_FALSE,
                                                   });

    return resources;
} // namespace Reaper

void destroy_sampler_resources(VulkanBackend& backend, SamplerResources& resources)
{
    vkDestroySampler(backend.device, resources.linear_clamp, nullptr);
    vkDestroySampler(backend.device, resources.linear_black_border, nullptr);
    vkDestroySampler(backend.device, resources.shadow_map_sampler, nullptr);
    vkDestroySampler(backend.device, resources.diffuse_map_sampler, nullptr);
}
} // namespace Reaper
