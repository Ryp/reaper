////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
// Small trick to hook Vulkan structures together
//
// Example: vk_hook(A, vk_hook(B, vk_hook(C)));
// Will link A.pNext to B, B.pNext to C.
template <typename VkStruct>
VkStruct* vk_hook(VkStruct& a, void* next = nullptr)
{
    a.pNext = next;
    return &a;
}
} // namespace Reaper
