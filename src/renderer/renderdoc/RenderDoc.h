////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
struct ReaperRoot;

namespace RenderDoc
{
    void start_integration(ReaperRoot& root);
    void stop_integration(ReaperRoot& root);

    using DevicePointer = void*;

    void start_capture(ReaperRoot& root, DevicePointer vulkan_instance);
    void stop_capture(ReaperRoot& root, DevicePointer vulkan_instance);
} // namespace RenderDoc
} // namespace Reaper
