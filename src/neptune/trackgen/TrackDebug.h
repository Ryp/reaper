////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "TrackGenExport.h"

#include <core/Types.h>

#include <ostream>
#include <vector>

namespace Reaper::Math
{
struct Spline;
}

namespace Neptune
{
struct TrackSkeletonNode;
struct TrackSkinning;

NEPTUNE_TRACKGEN_API
void write_track_skeleton_as_obj(std::ostream& output, std::vector<TrackSkeletonNode>& skeleton);

NEPTUNE_TRACKGEN_API
void write_track_splines_as_obj(std::ostream& output, std::vector<TrackSkeletonNode>& skeleton,
                                std::vector<Reaper::Math::Spline>& splines, u32 tesselation);

NEPTUNE_TRACKGEN_API
void write_track_bones_as_obj(std::ostream& output, std::vector<TrackSkinning>& skinningInfo);
} // namespace Neptune
