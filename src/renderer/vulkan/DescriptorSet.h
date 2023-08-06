////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vulkan_loader/Vulkan.h>

#include <nonstd/span.hpp>
#include <vector>

namespace Reaper
{
VkDescriptorImageInfo  create_descriptor_image_info(VkImageView image_view, VkImageLayout layout);
VkDescriptorImageInfo  create_descriptor_image_info(VkSampler sampler);
VkDescriptorBufferInfo create_descriptor_buffer_info(VkBuffer handle, u64 offset_bytes, u64 size_bytes);

VkWriteDescriptorSet create_image_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                   VkDescriptorType                          descriptor_type,
                                                   nonstd::span<const VkDescriptorImageInfo> image_infos);

VkWriteDescriptorSet create_image_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                   VkDescriptorType             descriptor_type,
                                                   const VkDescriptorImageInfo* image_info);

VkWriteDescriptorSet create_buffer_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                    VkDescriptorType                           descriptor_type,
                                                    nonstd::span<const VkDescriptorBufferInfo> buffer_infos);

VkWriteDescriptorSet create_buffer_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                    VkDescriptorType              descriptor_type,
                                                    const VkDescriptorBufferInfo* buffer_info);

VkWriteDescriptorSet create_texel_buffer_view_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                               VkDescriptorType                 descriptor_type,
                                                               nonstd::span<const VkBufferView> texel_buffer_views);

VkWriteDescriptorSet create_texel_buffer_view_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                               VkDescriptorType    descriptor_type,
                                                               const VkBufferView* texel_buffer_view);

// Vulkan needs a few structures to be chained with pointers to fill descriptor sets.
// This is quite tedious, so this helper's job is to hold the memory for these.
// FIXME std::vector shouldn't be used here, we need to guarantee that no realloc happens.
struct DescriptorWriteHelper
{
    std::vector<VkDescriptorImageInfo>  images;
    std::vector<VkDescriptorBufferInfo> buffers;
    std::vector<VkBufferView>           texel_buffers;

    u64 image_capacity;
    u64 buffer_capacity;
    u64 texel_buffer_capacity;

    std::vector<VkWriteDescriptorSet> writes;

    ~DescriptorWriteHelper();
};

DescriptorWriteHelper create_descriptor_write_helper(u32 image_descriptor_count, u32 buffer_descriptor_count,
                                                     u32 texel_buffer_descriptor_count = 0);

void append_write(DescriptorWriteHelper& write_context, VkDescriptorSet descriptor_set, u32 binding,
                  VkDescriptorType type, VkImageView image_view, VkImageLayout layout);
void append_write(DescriptorWriteHelper& write_context, VkDescriptorSet descriptor_set, u32 binding, VkSampler sampler);
void append_write(DescriptorWriteHelper& write_context, VkDescriptorSet descriptor_set, u32 binding,
                  VkDescriptorType type, VkBuffer buffer, u64 offset_bytes, u64 size_bytes);
void append_write(DescriptorWriteHelper& write_context, VkDescriptorSet descriptor_set, u32 binding,
                  VkDescriptorType type, VkBuffer buffer);
void append_write(DescriptorWriteHelper& write_context, VkDescriptorSet descriptor_set, u32 binding,
                  VkDescriptorType type, VkBufferView texel_buffer_view);

void flush_descriptor_write_helper(DescriptorWriteHelper& write_helper, VkDevice device);
} // namespace Reaper
