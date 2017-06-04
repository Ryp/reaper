////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include <fstream>

#include "splinesonic/trackgen/Track.h"
#include "splinesonic/trackgen/TrackDebug.h"

using namespace SplineSonic::TrackGen;

TEST_CASE("Simple track generation")
{
    TrackSkeleton testTrack;
    GenerationInfo genInfo = {};

    genInfo.length = 100;
    genInfo.width = 12.0f;
    genInfo.chaos = 0.00f;

    GenerateTrackSkeleton(genInfo, testTrack);
}

TEST_CASE("Save track")
{
    TrackSkeleton testTrack;
    GenerationInfo genInfo = {};

    genInfo.length = 100;
    genInfo.width = 12.0f;
    genInfo.chaos = 1.0f;

    GenerateTrackSkeleton(genInfo, testTrack);

    std::ofstream file("test.obj");
    SaveTrackAsObj(file, testTrack);
}
