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
    VkSampler create_shadow_map_sampler(VulkanBackend& backend, bool reverse_z)
    {
        VkSamplerCreateInfo shadowMapSamplerCreateInfo = {};
        shadowMapSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        shadowMapSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        shadowMapSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        shadowMapSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowMapSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowMapSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowMapSamplerCreateInfo.anisotropyEnable = VK_FALSE;
        shadowMapSamplerCreateInfo.maxAnisotropy = 16;
        shadowMapSamplerCreateInfo.borderColor =
            reverse_z ? VK_BORDER_COLOR_INT_OPAQUE_BLACK : VK_BORDER_COLOR_INT_OPAQUE_WHITE;
        shadowMapSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
        shadowMapSamplerCreateInfo.compareEnable = VK_TRUE;
        shadowMapSamplerCreateInfo.compareOp = reverse_z ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
        shadowMapSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        shadowMapSamplerCreateInfo.mipLodBias = 0.f;
        shadowMapSamplerCreateInfo.minLod = 0.f;
        shadowMapSamplerCreateInfo.maxLod = FLT_MAX;

        VkSampler sampler;
        Assert(vkCreateSampler(backend.device, &shadowMapSamplerCreateInfo, nullptr, &sampler) == VK_SUCCESS);

        return sampler;
    }

    VkSampler create_diffuse_map_sampler(VulkanBackend& backend)
    {
        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.anisotropyEnable = VK_TRUE;
        samplerCreateInfo.anisotropyEnable = VK_FALSE; // FIXME enable at higher level
        samplerCreateInfo.maxAnisotropy = 8;
        samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.mipLodBias = 0.f;
        samplerCreateInfo.minLod = 0.f;
        samplerCreateInfo.maxLod = FLT_MAX;

        VkSampler sampler = VK_NULL_HANDLE;
        Assert(vkCreateSampler(backend.device, &samplerCreateInfo, nullptr, &sampler) == VK_SUCCESS);

        return sampler;
    }
} // namespace

SamplerResources create_sampler_resources(ReaperRoot& root, VulkanBackend& backend)
{
    SamplerResources resources;

    VkSamplerCreateInfo linearClampSamplerCreateInfo = {};
    linearClampSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    linearClampSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    linearClampSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    linearClampSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    linearClampSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    linearClampSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    linearClampSamplerCreateInfo.anisotropyEnable = VK_FALSE;
    linearClampSamplerCreateInfo.maxAnisotropy = 16;
    linearClampSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    linearClampSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    linearClampSamplerCreateInfo.compareEnable = VK_FALSE;
    linearClampSamplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    linearClampSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    linearClampSamplerCreateInfo.mipLodBias = 0.f;
    linearClampSamplerCreateInfo.minLod = 0.f;
    linearClampSamplerCreateInfo.maxLod = FLT_MAX;

    Assert(vkCreateSampler(backend.device, &linearClampSamplerCreateInfo, nullptr, &resources.linearClampSampler)
           == VK_SUCCESS);

    VkSamplerCreateInfo linearBlackBorderSamplerCreateInfo = {};
    linearBlackBorderSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    linearBlackBorderSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    linearBlackBorderSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    linearBlackBorderSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    linearBlackBorderSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    linearBlackBorderSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    linearBlackBorderSamplerCreateInfo.anisotropyEnable = VK_FALSE;
    linearBlackBorderSamplerCreateInfo.maxAnisotropy = 16;
    linearBlackBorderSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    linearBlackBorderSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    linearBlackBorderSamplerCreateInfo.compareEnable = VK_FALSE;
    linearBlackBorderSamplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    linearBlackBorderSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    linearBlackBorderSamplerCreateInfo.mipLodBias = 0.f;
    linearBlackBorderSamplerCreateInfo.minLod = 0.f;
    linearBlackBorderSamplerCreateInfo.maxLod = FLT_MAX;

    Assert(vkCreateSampler(backend.device, &linearBlackBorderSamplerCreateInfo, nullptr,
                           &resources.linearBlackBorderSampler)
           == VK_SUCCESS);

    log_debug(root, "vulkan: created sampler with handle: {}", static_cast<void*>(resources.linearBlackBorderSampler));

    VulkanSetDebugName(backend.device, resources.linearBlackBorderSampler, "LinearBlackBorderSampler");

    resources.shadowMapSampler = create_shadow_map_sampler(backend, ShadowUseReverseZ);
    resources.diffuseMapSampler = create_diffuse_map_sampler(backend);

    return resources;
}

void destroy_sampler_resources(VulkanBackend& backend, SamplerResources& resources)
{
    vkDestroySampler(backend.device, resources.linearClampSampler, nullptr);
    vkDestroySampler(backend.device, resources.linearBlackBorderSampler, nullptr);
    vkDestroySampler(backend.device, resources.shadowMapSampler, nullptr);
    vkDestroySampler(backend.device, resources.diffuseMapSampler, nullptr);
}
} // namespace Reaper
