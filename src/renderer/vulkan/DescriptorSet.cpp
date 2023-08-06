////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "DescriptorSet.h"

#include "Buffer.h"
#include "Image.h"

namespace Reaper
{
VkDescriptorImageInfo create_descriptor_image_info(VkImageView image_view, VkImageLayout layout)
{
    return VkDescriptorImageInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = image_view,
        .imageLayout = layout,
    };
}

VkDescriptorImageInfo create_descriptor_image_info(VkSampler sampler)
{
    return VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
}

VkDescriptorBufferInfo create_descriptor_buffer_info(VkBuffer handle, u64 offset_bytes, u64 size_bytes)
{
    return VkDescriptorBufferInfo{
        .buffer = handle,
        .offset = offset_bytes,
        .range = size_bytes,
    };
}

VkWriteDescriptorSet create_image_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                   VkDescriptorType                          descriptor_type,
                                                   nonstd::span<const VkDescriptorImageInfo> image_infos)
{
    return VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_set,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<u32>(image_infos.size()),
        .descriptorType = descriptor_type,
        .pImageInfo = image_infos.data(),
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };
}

VkWriteDescriptorSet create_image_descriptor_write(VkDescriptorSet descriptor_set, u32 binding, VkDescriptorType type,
                                                   const VkDescriptorImageInfo* image_info)
{
    return create_image_descriptor_write(descriptor_set, binding, type, nonstd::span(image_info, 1));
}

VkWriteDescriptorSet create_buffer_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                    VkDescriptorType                           descriptor_type,
                                                    nonstd::span<const VkDescriptorBufferInfo> buffer_infos)
{
    return VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_set,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<u32>(buffer_infos.size()),
        .descriptorType = descriptor_type,
        .pImageInfo = nullptr,
        .pBufferInfo = buffer_infos.data(),
        .pTexelBufferView = nullptr,
    };
}

VkWriteDescriptorSet create_buffer_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                    VkDescriptorType              descriptor_type,
                                                    const VkDescriptorBufferInfo* buffer_info)
{
    return create_buffer_descriptor_write(descriptor_set, binding, descriptor_type, nonstd::span(buffer_info, 1));
}

VkWriteDescriptorSet create_texel_buffer_view_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                               VkDescriptorType                 descriptor_type,
                                                               nonstd::span<const VkBufferView> texel_buffer_views)
{
    return VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_set,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<u32>(texel_buffer_views.size()),
        .descriptorType = descriptor_type,
        .pImageInfo = nullptr,
        .pBufferInfo = nullptr,
        .pTexelBufferView = texel_buffer_views.data(),
    };
}

VkWriteDescriptorSet create_texel_buffer_view_descriptor_write(VkDescriptorSet descriptor_set, u32 binding,
                                                               VkDescriptorType    descriptor_type,
                                                               const VkBufferView* texel_buffer_view)
{
    return create_texel_buffer_view_descriptor_write(descriptor_set, binding, descriptor_type,
                                                     nonstd::span(texel_buffer_view, 1));
}

DescriptorWriteHelper::~DescriptorWriteHelper()
{
    delete[] image_infos.data();
    delete[] buffer_infos.data();
    delete[] texel_buffer_views.data();

    Assert(writes.empty());
}

VkDescriptorImageInfo& DescriptorWriteHelper::new_image_info(const VkDescriptorImageInfo&& image_info)
{
    VkDescriptorImageInfo& new_element = image_infos[image_info_size];

    new_element = image_info;
    image_info_size += 1;

    return new_element;
}

VkDescriptorBufferInfo& DescriptorWriteHelper::new_buffer_info(const VkDescriptorBufferInfo&& buffer_info)
{
    VkDescriptorBufferInfo& new_element = buffer_infos[buffer_info_size];

    new_element = buffer_info;
    buffer_info_size += 1;

    return new_element;
}

VkBufferView& DescriptorWriteHelper::new_texel_buffer_view(VkBufferView texel_buffer_view)
{
    VkBufferView& new_element = texel_buffer_views[texel_buffer_view_size];

    new_element = texel_buffer_view;
    texel_buffer_view_size += 1;

    return new_element;
}

DescriptorWriteHelper create_descriptor_write_helper(u32 image_descriptor_count, u32 buffer_descriptor_count,
                                                     u32 texel_buffer_descriptor_count)
{
    DescriptorWriteHelper helper;

    helper.image_infos = nonstd::span(new VkDescriptorImageInfo[image_descriptor_count], image_descriptor_count);
    helper.buffer_infos = nonstd::span(new VkDescriptorBufferInfo[buffer_descriptor_count], buffer_descriptor_count);
    helper.texel_buffer_views =
        nonstd::span(new VkBufferView[texel_buffer_descriptor_count], texel_buffer_descriptor_count);

    helper.image_info_size = 0;
    helper.buffer_info_size = 0;
    helper.texel_buffer_view_size = 0;

    return helper;
}

void append_write(DescriptorWriteHelper& write_context, VkDescriptorSet descriptor_set, u32 binding,
                  VkDescriptorType type, VkImageView image_view, VkImageLayout layout)
{
    Assert(type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
           "Invalid descriptor type");

    const VkDescriptorImageInfo& image_descriptor_info =
        write_context.new_image_info(create_descriptor_image_info(image_view, layout));

    write_context.writes.emplace_back(
        create_image_descriptor_write(descriptor_set, binding, type, &image_descriptor_info));
}

void append_write(DescriptorWriteHelper& write_context, VkDescriptorSet descriptor_set, u32 binding, VkSampler sampler)
{
    const VkDescriptorImageInfo& sampler_descriptor_info =
        write_context.new_image_info(create_descriptor_image_info(sampler));

    write_context.writes.emplace_back(
        create_image_descriptor_write(descriptor_set, binding, VK_DESCRIPTOR_TYPE_SAMPLER, &sampler_descriptor_info));
}

void append_write(DescriptorWriteHelper& write_context, VkDescriptorSet descriptor_set, u32 binding,
                  VkDescriptorType type, VkBuffer buffer, u64 offset_bytes, u64 size_bytes)
{
    Assert(type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
           "Invalid descriptor type");

    const VkDescriptorBufferInfo& buffer_info =
        write_context.new_buffer_info(create_descriptor_buffer_info(buffer, offset_bytes, size_bytes));

    write_context.writes.emplace_back(create_buffer_descriptor_write(descriptor_set, binding, type, &buffer_info));
}

void append_write(DescriptorWriteHelper& write_context, VkDescriptorSet descriptor_set, u32 binding,
                  VkDescriptorType type, VkBuffer buffer)
{
    append_write(write_context, descriptor_set, binding, type, buffer, 0, VK_WHOLE_SIZE);
}

void append_write(DescriptorWriteHelper& write_context, VkDescriptorSet descriptor_set, u32 binding,
                  VkDescriptorType type, VkBufferView texel_buffer_view)
{
    const VkBufferView& texel_buffer_info = write_context.new_texel_buffer_view(texel_buffer_view);

    write_context.writes.emplace_back(
        create_texel_buffer_view_descriptor_write(descriptor_set, binding, type, &texel_buffer_info));
}

void flush_descriptor_write_helper(DescriptorWriteHelper& write_helper, VkDevice device)
{
    vkUpdateDescriptorSets(device, static_cast<u32>(write_helper.writes.size()), write_helper.writes.data(), 0,
                           nullptr);

    write_helper.buffer_info_size = 0;
    write_helper.image_info_size = 0;
    write_helper.texel_buffer_view_size = 0;

    write_helper.writes.clear();
}
} // namespace Reaper
