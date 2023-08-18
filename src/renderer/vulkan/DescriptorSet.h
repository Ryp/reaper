////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vulkan_loader/Vulkan.h>

#include <span>
#include <vector>

namespace Reaper
{
VkDescriptorImageInfo  create_descriptor_image_info(VkImageView image_view, VkImageLayout layout);
VkDescriptorImageInfo  create_descriptor_image_info(VkSampler sampler);
VkDescriptorBufferInfo create_descriptor_buffer_info(VkBuffer handle, u64 offset_bytes, u64 size_bytes);

VkWriteDescriptorSet create_image_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                   VkDescriptorType                       descriptor_type,
                                                   std::span<const VkDescriptorImageInfo> image_infos);

VkWriteDescriptorSet create_image_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                   VkDescriptorType             descriptor_type,
                                                   const VkDescriptorImageInfo* image_info);

VkWriteDescriptorSet create_buffer_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                    VkDescriptorType                        descriptor_type,
                                                    std::span<const VkDescriptorBufferInfo> buffer_infos);

VkWriteDescriptorSet create_buffer_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                    VkDescriptorType              descriptor_type,
                                                    const VkDescriptorBufferInfo* buffer_info);

VkWriteDescriptorSet create_texel_buffer_view_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                               VkDescriptorType              descriptor_type,
                                                               std::span<const VkBufferView> texel_buffer_views);

VkWriteDescriptorSet create_texel_buffer_view_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                               VkDescriptorType    descriptor_type,
                                                               const VkBufferView* texel_buffer_view);

// Vulkan needs a few structures to be chained with pointers to fill descriptor sets.
// This is quite tedious, so this helper's job is to hold the memory for these.
class DescriptorWriteHelper
{
public:
    DescriptorWriteHelper(u32 image_descriptor_count, u32 buffer_descriptor_count,
                          u32 texel_buffer_descriptor_count = 1);
    ~DescriptorWriteHelper();

    VkDescriptorImageInfo&           new_image_info(VkDescriptorImageInfo image_info);
    std::span<VkDescriptorImageInfo> new_image_infos(u32 count);
    VkDescriptorBufferInfo&          new_buffer_info(VkDescriptorBufferInfo buffer_info);
    VkBufferView&                    new_texel_buffer_view(VkBufferView texel_buffer_view);

    void append(VkDescriptorSet descriptor_set, u32 binding, VkDescriptorType type, VkImageView image_view,
                VkImageLayout layout);
    void append(VkDescriptorSet descriptor_set, u32 binding, VkSampler sampler);
    void append(VkDescriptorSet descriptor_set, u32 binding, VkDescriptorType type, VkBuffer buffer, u64 offset_bytes,
                u64 size_bytes);
    void append(VkDescriptorSet descriptor_set, u32 binding, VkDescriptorType type, VkBuffer buffer);
    void append(VkDescriptorSet descriptor_set, u32 binding, VkDescriptorType type, VkBufferView texel_buffer_view);

    void flush_descriptor_write_helper(VkDevice device);

private:
    std::span<VkDescriptorImageInfo>  image_infos;
    std::span<VkDescriptorBufferInfo> buffer_infos;
    std::span<VkBufferView>           texel_buffer_views;

    u64 image_info_size;
    u64 buffer_info_size;
    u64 texel_buffer_view_size;

public:
    std::vector<VkWriteDescriptorSet> writes;
};
} // namespace Reaper
