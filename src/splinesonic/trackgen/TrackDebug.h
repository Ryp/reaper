////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ostream>

#include "TrackGenExport.h"

namespace SplineSonic { namespace TrackGen
{
    struct TrackSkeleton;

    REAPER_TRACKGEN_API void SaveTrackAsObj(std::ostream& output, TrackSkeleton& track);
}}
