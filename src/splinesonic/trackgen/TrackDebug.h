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

namespace SplineSonic
{
struct TrackSkeletonNode;
struct TrackSkinning;

SPLINESONIC_TRACKGEN_API
void SaveTrackSkeletonAsObj(std::ostream& output, std::vector<TrackSkeletonNode>& skeleton);

SPLINESONIC_TRACKGEN_API
void SaveTrackSplinesAsObj(std::ostream& output, std::vector<TrackSkeletonNode>& skeleton,
                           std::vector<Reaper::Math::Spline>& splines, u32 tesselation);

SPLINESONIC_TRACKGEN_API
void SaveTrackBonesAsObj(std::ostream& output, std::vector<TrackSkinning>& skinningInfo);
} // namespace SplineSonic
