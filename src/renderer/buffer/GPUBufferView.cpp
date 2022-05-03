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
        0, // offset_bytes
        properties.stride * properties.elementCount,
    };
}

GPUBufferView get_buffer_view(const GPUBufferProperties& properties, const BufferSubresource& subresource)
{
    return {
        properties.stride * subresource.element_offset,
        properties.stride * subresource.element_count,
    };
}
} // namespace Reaper
