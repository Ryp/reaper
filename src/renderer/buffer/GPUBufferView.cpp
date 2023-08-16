////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GPUBufferView.h"

#include "GPUBufferProperties.h"

namespace Reaper
{
GPUBufferView default_buffer_view(const GPUBufferProperties& properties)
{
    return GPUBufferView{
        .offset_bytes = 0,
        .size_bytes = properties.stride * properties.element_count,
    };
}

GPUBufferView get_buffer_view(const GPUBufferProperties& properties, const BufferSubresource& subresource)
{
    return GPUBufferView{
        .offset_bytes = properties.stride * subresource.element_offset,
        .size_bytes = properties.stride * subresource.element_count,
    };
}
} // namespace Reaper
