////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ostream>
#include <vector>

namespace Reaper { namespace Math
{
    struct Spline;
}}

namespace SplineSonic { namespace TrackGen
{
    struct TrackSkeletonNode;
    struct TrackSkinning;

    REAPER_TRACKGEN_API void SaveTrackSkeletonAsObj(std::ostream& output, std::vector<TrackSkeletonNode>& skeleton);
    REAPER_TRACKGEN_API void SaveTrackSplinesAsObj(std::ostream& output, std::vector<Reaper::Math::Spline>& splines, u32 tesselation);
    REAPER_TRACKGEN_API void SaveTrackBonesAsObj(std::ostream& output, std::vector<TrackSkinning>& skinningInfo);
}}
