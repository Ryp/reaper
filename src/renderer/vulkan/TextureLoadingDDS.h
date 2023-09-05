////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MaterialResources.h"

namespace Reaper
{
StagingEntry copy_texture_to_staging_area_dds(VulkanBackend& backend, ResourceStagingArea& staging,
                                              const char* file_path);
}
